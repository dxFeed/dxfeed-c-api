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

#include "DXAlgorithms.h"
#include "DXErrorHandling.h"
#include "DXMemory.h"
#include "EventManager.h"

/* -------------------------------------------------------------------------- */
/*
 *	Event objects management functions
 */
/* -------------------------------------------------------------------------- */

#define EVENT_COPY_FUNCTION_NAME(struct_name) \
	struct_name##_event_copy

#define EVENT_COPY_FUNCTION_BODY(struct_name) \
	dxf_bool_t EVENT_COPY_FUNCTION_NAME(struct_name)(const dxf_event_data_t source, \
				OUT dx_string_array_ptr_t* string_buffer, OUT dxf_event_data_t* new_obj) { \
		struct_name* dest = NULL; \
		if (source == NULL || new_obj == NULL || string_buffer == NULL) { \
			return dx_set_error_code(dx_ec_invalid_func_param_internal); \
																				} \
		dest = dx_calloc(1, sizeof(struct_name)); \
		if (dest == NULL) { \
			return false; \
																				} \
		dx_memcpy(dest, source, sizeof(struct_name)); \
		*string_buffer = NULL; \
		*new_obj = (dxf_event_data_t)dest; \
		return true; \
														}

#define EVENT_COPY_WITH_STRING_FUNCTION_BODY(struct_name, string_param) \
	dxf_bool_t EVENT_COPY_FUNCTION_NAME(struct_name)(const dxf_event_data_t source, \
				OUT dx_string_array_ptr_t* string_buffer, OUT dxf_event_data_t* new_obj) { \
		struct_name* dest = NULL; \
		struct_name* src_obj = (struct_name*)source; \
		dxf_string_t temp_str = NULL; \
		if (source == NULL || new_obj == NULL || string_buffer == NULL) { \
			return dx_set_error_code(dx_ec_invalid_func_param_internal); \
												} \
		dest = dx_calloc(1, sizeof(struct_name)); \
		if (dest == NULL) { \
			return false; \
												} \
		dx_memcpy(dest, source, sizeof(struct_name)); \
		if (src_obj->string_param != NULL) { \
			dx_string_array_ptr_t temp_buf = (*string_buffer == NULL) ? dx_calloc(1, sizeof(dx_string_array_t)) : *string_buffer; \
			temp_str = dx_create_string_src(src_obj->string_param); \
			if (!dx_string_array_add(temp_buf, temp_str)) { \
				dx_free(dest); \
				if (*string_buffer == NULL) { \
					dx_free(temp_buf); \
				} \
				dx_free(temp_str); \
				return false; \
			} \
			dest->string_param = temp_str; \
			*string_buffer = temp_buf; \
		} \
		*new_obj = (dxf_event_data_t)dest; \
		return true; \
	}

#define EVENT_COPY_WITH_2_STRINGS_FUNCTION_BODY(struct_name, string_param_1, string_param_2) \
	dxf_bool_t EVENT_COPY_FUNCTION_NAME(struct_name)(const dxf_event_data_t source, \
				OUT dx_string_array_ptr_t* string_buffer, OUT dxf_event_data_t* new_obj) { \
		struct_name* dest = NULL; \
		struct_name* src_obj = (struct_name*)source; \
		dxf_string_t temp_str = NULL; \
		if (source == NULL || new_obj == NULL || string_buffer == NULL) { \
			return dx_set_error_code(dx_ec_invalid_func_param_internal); \
												} \
		dest = dx_calloc(1, sizeof(struct_name)); \
		if (dest == NULL) { \
			return false; \
												} \
		dx_memcpy(dest, source, sizeof(struct_name)); \
		if (src_obj->string_param_1 != NULL) { \
			dx_string_array_ptr_t temp_buf = (*string_buffer == NULL) ? dx_calloc(1, sizeof(dx_string_array_t)) : *string_buffer; \
			temp_str = dx_create_string_src(src_obj->string_param_1); \
			if (!dx_string_array_add(temp_buf, temp_str)) { \
				dx_free(dest); \
				if (*string_buffer == NULL) { \
					dx_free(temp_buf); \
				} \
				dx_free(temp_str); \
				return false; \
			} \
			dest->string_param_1 = temp_str; \
			*string_buffer = temp_buf; \
		} \
		if (src_obj->string_param_2 != NULL) { \
			dx_string_array_ptr_t temp_buf = (*string_buffer == NULL) ? dx_calloc(1, sizeof(dx_string_array_t)) : *string_buffer; \
			temp_str = dx_create_string_src(src_obj->string_param_2); \
			if (!dx_string_array_add(temp_buf, temp_str)) { \
				dx_free(dest); \
				dx_string_array_free(temp_buf); \
				if (*string_buffer == NULL) { \
					dx_free(temp_buf); \
				} \
				dx_free(temp_str); \
				return false; \
			} \
			dest->string_param_2 = temp_str; \
			*string_buffer = temp_buf; \
		} \
		*new_obj = (dxf_event_data_t)dest; \
		return true; \
	}

#define EVENT_COPY_WITH_3_STRINGS_FUNCTION_BODY(struct_name, string_param_1, string_param_2, string_param_3) \
	dxf_bool_t EVENT_COPY_FUNCTION_NAME(struct_name)(const dxf_event_data_t source, \
				OUT dx_string_array_ptr_t* string_buffer, OUT dxf_event_data_t* new_obj) { \
		struct_name* dest = NULL; \
		struct_name* src_obj = (struct_name*)source; \
		dxf_string_t temp_str = NULL; \
		if (source == NULL || new_obj == NULL || string_buffer == NULL) { \
			return dx_set_error_code(dx_ec_invalid_func_param_internal); \
												} \
		dest = dx_calloc(1, sizeof(struct_name)); \
		if (dest == NULL) { \
			return false; \
												} \
		dx_memcpy(dest, source, sizeof(struct_name)); \
		if (src_obj->string_param_1 != NULL) { \
			dx_string_array_ptr_t temp_buf = (*string_buffer == NULL) ? dx_calloc(1, sizeof(dx_string_array_t)) : *string_buffer; \
			temp_str = dx_create_string_src(src_obj->string_param_1); \
			if (!dx_string_array_add(temp_buf, temp_str)) { \
				dx_free(dest); \
				if (*string_buffer == NULL) { \
					dx_free(temp_buf); \
				} \
				dx_free(temp_str); \
				return false; \
			} \
			dest->string_param_1 = temp_str; \
			*string_buffer = temp_buf; \
		} \
		if (src_obj->string_param_2 != NULL) { \
			dx_string_array_ptr_t temp_buf = (*string_buffer == NULL) ? dx_calloc(1, sizeof(dx_string_array_t)) : *string_buffer; \
			temp_str = dx_create_string_src(src_obj->string_param_2); \
			if (!dx_string_array_add(temp_buf, temp_str)) { \
				dx_free(dest); \
				dx_string_array_free(temp_buf); \
				if (*string_buffer == NULL) { \
					dx_free(temp_buf); \
				} \
				dx_free(temp_str); \
				return false; \
			} \
			dest->string_param_2 = temp_str; \
			*string_buffer = temp_buf; \
		} \
		if (src_obj->string_param_3 != NULL) { \
			dx_string_array_ptr_t temp_buf = (*string_buffer == NULL) ? dx_calloc(1, sizeof(dx_string_array_t)) : *string_buffer; \
			temp_str = dx_create_string_src(src_obj->string_param_3); \
			if (!dx_string_array_add(temp_buf, temp_str)) { \
				dx_free(dest); \
				dx_string_array_free(temp_buf); \
				if (*string_buffer == NULL) { \
					dx_free(temp_buf); \
				} \
				dx_free(temp_str); \
				return false; \
			} \
			dest->string_param_3 = temp_str; \
			*string_buffer = temp_buf; \
		} \
		*new_obj = (dxf_event_data_t)dest; \
		return true; \
	}

EVENT_COPY_FUNCTION_BODY(dxf_trade_t)
EVENT_COPY_FUNCTION_BODY(dxf_quote_t)
EVENT_COPY_FUNCTION_BODY(dxf_summary_t)
EVENT_COPY_WITH_2_STRINGS_FUNCTION_BODY(dxf_profile_t, description, status_reason)
EVENT_COPY_WITH_STRING_FUNCTION_BODY(dxf_order_t, market_maker)
EVENT_COPY_WITH_3_STRINGS_FUNCTION_BODY(dxf_time_and_sale_t, exchange_sale_conditions, buyer, seller)
EVENT_COPY_FUNCTION_BODY(dxf_candle_t)
EVENT_COPY_FUNCTION_BODY(dxf_greeks_t)
EVENT_COPY_FUNCTION_BODY(dxf_theo_price_t)
EVENT_COPY_FUNCTION_BODY(dxf_underlying_t)
EVENT_COPY_FUNCTION_BODY(dxf_series_t)
EVENT_COPY_WITH_STRING_FUNCTION_BODY(dxf_configuration_t, object)

static const dx_event_copy_function_t g_event_copy_functions[dx_eid_count] = {
	EVENT_COPY_FUNCTION_NAME(dxf_trade_t),
	EVENT_COPY_FUNCTION_NAME(dxf_quote_t),
	EVENT_COPY_FUNCTION_NAME(dxf_summary_t),
	EVENT_COPY_FUNCTION_NAME(dxf_profile_t),
	EVENT_COPY_FUNCTION_NAME(dxf_order_t),
	EVENT_COPY_FUNCTION_NAME(dxf_time_and_sale_t),
	EVENT_COPY_FUNCTION_NAME(dxf_candle_t),
	EVENT_COPY_FUNCTION_NAME(dxf_trade_t), // As trade ETH
	EVENT_COPY_FUNCTION_NAME(dxf_order_t), // As spread order
	EVENT_COPY_FUNCTION_NAME(dxf_greeks_t),
	EVENT_COPY_FUNCTION_NAME(dxf_theo_price_t),
	EVENT_COPY_FUNCTION_NAME(dxf_underlying_t),
	EVENT_COPY_FUNCTION_NAME(dxf_series_t),
	EVENT_COPY_FUNCTION_NAME(dxf_configuration_t)
};

dx_event_copy_function_t dx_get_event_copy_function(dx_event_id_t event_id) {
	if (event_id >= dx_eid_count) {
		dx_set_error_code(dx_ec_invalid_func_param_internal);
		return NULL;
	}
	return g_event_copy_functions[event_id];
}

#define EVENT_FREE_FUNCTION_NAME(struct_name) \
	struct_name##_event_free

#define EVENT_FREE_FUNCTION_BODY(struct_name) \
	void EVENT_FREE_FUNCTION_NAME(struct_name)(dxf_event_data_t obj) { \
		dx_free(obj); \
	}

EVENT_FREE_FUNCTION_BODY(dxf_trade_t)
EVENT_FREE_FUNCTION_BODY(dxf_quote_t)
EVENT_FREE_FUNCTION_BODY(dxf_summary_t)
EVENT_FREE_FUNCTION_BODY(dxf_profile_t)
EVENT_FREE_FUNCTION_BODY(dxf_order_t)
EVENT_FREE_FUNCTION_BODY(dxf_time_and_sale_t)
EVENT_FREE_FUNCTION_BODY(dxf_candle_t)
EVENT_FREE_FUNCTION_BODY(dxf_trade_eth_t)
EVENT_FREE_FUNCTION_BODY(dxf_greeks_t)
EVENT_FREE_FUNCTION_BODY(dxf_theo_price_t)
EVENT_FREE_FUNCTION_BODY(dxf_underlying_t)
EVENT_FREE_FUNCTION_BODY(dxf_series_t)
EVENT_FREE_FUNCTION_BODY(dxf_configuration_t)

static const dx_event_free_function_t g_event_free_functions[dx_eid_count] = {
	EVENT_FREE_FUNCTION_NAME(dxf_trade_t),
	EVENT_FREE_FUNCTION_NAME(dxf_quote_t),
	EVENT_FREE_FUNCTION_NAME(dxf_summary_t),
	EVENT_FREE_FUNCTION_NAME(dxf_profile_t),
	EVENT_FREE_FUNCTION_NAME(dxf_order_t),
	EVENT_FREE_FUNCTION_NAME(dxf_time_and_sale_t),
	EVENT_FREE_FUNCTION_NAME(dxf_candle_t),
	EVENT_FREE_FUNCTION_NAME(dxf_trade_eth_t),
	EVENT_FREE_FUNCTION_NAME(dxf_order_t), // As spread order
	EVENT_FREE_FUNCTION_NAME(dxf_greeks_t),
	EVENT_FREE_FUNCTION_NAME(dxf_theo_price_t),
	EVENT_FREE_FUNCTION_NAME(dxf_underlying_t),
	EVENT_FREE_FUNCTION_NAME(dxf_series_t),
	EVENT_FREE_FUNCTION_NAME(dxf_configuration_t)
};

dx_event_free_function_t dx_get_event_free_function(dx_event_id_t event_id) {
	if (event_id >= dx_eid_count) {
		dx_set_error_code(dx_ec_invalid_func_param_internal);
		return NULL;
	}
	return g_event_free_functions[event_id];
}
