/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 1.1 (the "License"); you may not use this file except in compliance with
 * the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for the specific language governing rights and limitations under the
 * License.
 *
 * The Initial Developer of the Original Code is Devexperts LLC.
 * Portions created by the Initial Developer are Copyright (C) 2010
 * the Initial Developer. All Rights Reserved.
 *
 * Contributor(s):
 *
 */

#include <wctype.h>
#include <math.h>

#include "Candle.h"
#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "EventData.h"
#include "DXErrorHandling.h"

typedef struct {
	dxf_string_t base_symbol;
	dxf_char_t exchange_code;
	dxf_double_t period_value;
	dxf_candle_type_period_attribute_t period_type;
	dxf_candle_price_attribute_t price;
	dxf_candle_session_attribute_t session;
	dxf_candle_alignment_attribute_t alignment;
	dxf_double_t price_level;
} dx_candle_attributes_data_t;

typedef struct {
	dxf_string_t string;
	dxf_long_t period_interval_millis;
} dx_candle_type;

static const dx_candle_type g_candle_type_period[dxf_ctpa_count] = {
	{ L"t", 0LL },
	{ L"s", 1000LL },
	{ L"m", 60LL * 1000LL },
	{ L"h", 60LL * 60LL * 1000LL },
	{ L"d", 24LL * 60LL * 60LL * 1000LL },
	{ L"w", 7LL * 24LL * 60LL * 60LL * 1000LL },
	{ L"mo", 30LL * 24LL * 60LL * 60LL * 1000LL },
	{ L"o", 30LL * 24LL * 60LL * 60LL * 1000LL },
	{ L"y", 365LL * 24LL * 60LL * 60LL * 1000LL },
	{ L"v", 0LL },
	{ L"p", 0LL },
	{ L"pm", 0LL },
	{ L"pr", 0LL }
};

static const dxf_string_t g_candle_price[dxf_cpa_count] = {
	L"last",
	L"bid",
	L"ask",
	L"mark",
	L"s"
};

static const dxf_string_t g_candle_session[dxf_csa_count] = {
	L"false", /*ANY*/
	L"true"   /*REGULAR*/
};

static const dxf_string_t g_candle_alignment[dxf_caa_count] = {
	L"m", /*MIDNIGHT*/
	L"s"  /*SESSION*/
};


DXFEED_API ERRORCODE dxf_create_candle_symbol_attributes(dxf_const_string_t base_symbol,
														dxf_char_t exchange_code,
														dxf_double_t period_value,
														dxf_candle_type_period_attribute_t period_type,
														dxf_candle_price_attribute_t price,
														dxf_candle_session_attribute_t session,
														dxf_candle_alignment_attribute_t alignment,
														dxf_double_t price_level,
														OUT dxf_candle_attributes_t* candle_attributes) {
	dx_candle_attributes_data_t* attributes;
	if (candle_attributes == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);

		return DXF_FAILURE;
	}
	if (period_value < 0) {
		dx_set_error_code(dx_ceec_invalid_candle_period_value);

		return DXF_FAILURE;
	}

	attributes = dx_calloc(1, sizeof(dx_candle_attributes_data_t));
	if (attributes == NULL) {
		dx_set_error_code(dx_mec_insufficient_memory);
		return DXF_FAILURE;
	}
	attributes->base_symbol = dx_create_string_src(base_symbol);
	if (attributes->base_symbol == NULL) {
		dx_free(attributes);
		dx_set_error_code(dx_mec_insufficient_memory);
		return DXF_FAILURE;
	}
	attributes->exchange_code = exchange_code;
	attributes->price = price;
	attributes->session = session;
	attributes->period_value = period_value;
	attributes->period_type = period_type;
	attributes->alignment = alignment;
	attributes->price_level = price_level;

	*candle_attributes = attributes;
	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_delete_candle_symbol_attributes(dxf_candle_attributes_t candle_attributes) {
	dx_candle_attributes_data_t *attributes = candle_attributes;
	if (candle_attributes == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		return DXF_FAILURE;
	}
	dx_free(attributes->base_symbol);
	dx_free(candle_attributes);

	return DXF_SUCCESS;
}

bool dx_candle_symbol_to_string(dxf_candle_attributes_t _attr, OUT dxf_string_t* string) {
	dx_candle_attributes_data_t *attributes = _attr;
	dxf_char_t buffer_str[1000];
	bool put_comma = false;

	if (attributes == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param_internal);
		return false;
	}

	dx_copy_string(buffer_str, attributes->base_symbol);
	if (iswalpha(attributes->exchange_code)) {
		dxf_char_t tmpstr[3] = { 0 };
		tmpstr[0] = L'&';
		tmpstr[1] = attributes->exchange_code;
		dx_concatenate_strings(buffer_str, tmpstr);
	}

	if (attributes->period_value == DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT &&
		attributes->period_type == dxf_ctpa_default &&
		attributes->alignment == dxf_caa_default &&
		attributes->price == dxf_cpa_default &&
		attributes->session == dxf_csa_default &&
		isnan(attributes->price_level)) {

		*string = dx_create_string_src(buffer_str);
		if (*string == NULL) {
			dx_set_error_code(dx_mec_insufficient_memory);
			return false;
		}

		return true;
	}

	dx_concatenate_strings(buffer_str, L"{");

	/*attribute (period) has no name and is written the first, and the rest should be sorted alphabetically.*/
	if (attributes->period_type != dxf_ctpa_default ||
		attributes->period_value != DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT) {

		dx_concatenate_strings(buffer_str, L"=");
		if (attributes->period_value != DXF_CANDLE_PERIOD_VALUE_ATTRIBUTE_DEFAULT) {
			dxf_char_t tmpstr[100];
			swprintf(tmpstr, 99, L"%g", attributes->period_value);
			dx_concatenate_strings(buffer_str, tmpstr);
		}
		dx_concatenate_strings(buffer_str, g_candle_type_period[attributes->period_type].string);
		put_comma = true;
	}

	if (attributes->alignment != dxf_caa_default) {
		if (put_comma) {
			dx_concatenate_strings(buffer_str, L",");
		}
		dx_concatenate_strings(buffer_str, L"a=");
		dx_concatenate_strings(buffer_str, g_candle_alignment[attributes->alignment]);

		put_comma = true;
	}

	if (!isnan(attributes->price_level)) {
		if (put_comma) {
			dx_concatenate_strings(buffer_str, L",");
		}
		dx_concatenate_strings(buffer_str, L"pl=");
		dxf_char_t tmpstr[100];
		swprintf(tmpstr, 99, L"%g", attributes->price_level);
		dx_concatenate_strings(buffer_str, tmpstr);
	}

	if (attributes->price != dxf_cpa_default) {
		if (put_comma) {
			dx_concatenate_strings(buffer_str, L",");
		}
		dx_concatenate_strings(buffer_str, L"price=");
		dx_concatenate_strings(buffer_str, g_candle_price[attributes->price]);

		put_comma = true;
	}

	if (attributes->session != dxf_csa_default) {
		if (put_comma) {
			dx_concatenate_strings(buffer_str, L",");
		}
		dx_concatenate_strings(buffer_str, L"tho=");
		dx_concatenate_strings(buffer_str, g_candle_session[attributes->session]);

		put_comma = true;
	}

	dx_concatenate_strings(buffer_str, L"}");

	*string = dx_create_string_src(buffer_str);
	if (*string == NULL) {
		dx_set_error_code(dx_mec_insufficient_memory);
		return false;
	}

	return true;
}

DXFEED_API ERRORCODE dxf_add_candle_symbol(dxf_subscription_t subscription, dxf_candle_attributes_t candle_attributes) {
	dxf_string_t candle_symbol;

	if (!dx_candle_symbol_to_string(candle_attributes, &candle_symbol)) {
		return DXF_FAILURE;
	}

	if (!dxf_add_symbol(subscription, candle_symbol)) {
		CHECKED_FREE(candle_symbol);
		return DXF_FAILURE;
	}

	CHECKED_FREE(candle_symbol);
	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_remove_candle_symbol(dxf_subscription_t subscription, dxf_candle_attributes_t candle_attributes) {
	dxf_string_t candle_symbol;

	if (!dx_candle_symbol_to_string(candle_attributes, &candle_symbol)) {
		return DXF_FAILURE;
	}

	if (!dxf_remove_symbol(subscription, candle_symbol)) {
		CHECKED_FREE(candle_symbol);
		return DXF_FAILURE;
	}

	CHECKED_FREE(candle_symbol);
	return DXF_SUCCESS;
}
