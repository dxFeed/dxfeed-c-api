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

#include "DXFeed.h"

#include "RecordBuffers.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"
#include "ObjectArray.h"

/* -------------------------------------------------------------------------- */
/*
 *	Internal data structures and types
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	void* buffer;
	int capacity;
} dx_event_record_buffer_t;

/* -------------------------------------------------------------------------- */
/*
 *	Record buffers connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dx_event_record_buffer_t record_buffer_array[dx_rid_count];
	dx_string_array_t string_buffers;
	dx_byte_buffer_array_t byte_array_buffers;
} dx_record_buffers_connection_context_t;

#define CTX(context) \
	((dx_record_buffers_connection_context_t*)context)

#define CONTEXT_FIELD(field) \
	(((dx_record_buffers_connection_context_t*)dx_get_subsystem_data(connection, dx_ccs_record_buffers))->field)

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_record_buffers) {
	dx_record_buffers_connection_context_t* context = dx_calloc(1, sizeof(dx_record_buffers_connection_context_t));

	if (context == NULL) {
		return false;
	}

	if (!dx_set_subsystem_data(connection, dx_ccs_record_buffers, context)) {
		dx_free(context);

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_record_buffers (dx_event_record_buffer_t* record_buffers);
void dx_free_string_buffers_impl (dx_string_array_t* string_buffers);
void dx_free_byte_array_buffers_impl(dx_byte_buffer_array_t* byte_array_buffers);

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_record_buffers) {
	bool res = true;
	dx_record_buffers_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_record_buffers, &res);

	if (context == NULL) {
		return res;
	}

	dx_clear_record_buffers(context->record_buffer_array);
	dx_free_string_buffers_impl(&(context->string_buffers));
	dx_free_byte_array_buffers_impl(&(context->byte_array_buffers));
	dx_free(context);

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_record_buffers) {
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions implementation
 */
/* -------------------------------------------------------------------------- */

void* dx_get_record_buffers_connection_context (dxf_connection_t connection) {
	return dx_get_subsystem_data(connection, dx_ccs_record_buffers, NULL);
}

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manager prototype and body macros
 */
/* -------------------------------------------------------------------------- */

#define GET_RECORD_PTR_NAME(record_id) \
	record_id##_get_record_ptr

#define GET_RECORD_BUF_PTR_NAME(record_id) \
	record_id##_get_record_buf_ptr

#define GET_RECORD_PTR_BODY(record_id, record_type) \
	void* GET_RECORD_PTR_NAME(record_id) (void* context, int record_index) { \
		dx_event_record_buffer_t* record_buffer = NULL; \
		\
		record_buffer = &(CTX(context)->record_buffer_array[record_id]); \
		\
		if (record_index >= record_buffer->capacity) { \
			record_type* new_buffer = dx_calloc(record_index + 1, sizeof(record_type)); \
			\
			if (new_buffer == NULL) { \
				return NULL; \
			} \
			\
			if (record_buffer->buffer != NULL) { \
				dx_memcpy(new_buffer, record_buffer->buffer, record_buffer->capacity * sizeof(record_type)); \
				dx_free(record_buffer->buffer); \
			} \
			\
			record_buffer->buffer = new_buffer; \
			record_buffer->capacity = record_index + 1; \
		} \
		\
		return ((record_type*)record_buffer->buffer) + record_index; \
	}

#define GET_RECORD_BUF_PTR_BODY(record_id) \
	void* GET_RECORD_BUF_PTR_NAME(record_id) (void* context) { \
		return CTX(context)->record_buffer_array[record_id].buffer; \
	}

/* -------------------------------------------------------------------------- */
/*
 *	Buffer managers implementation
 */
/* -------------------------------------------------------------------------- */

GET_RECORD_PTR_BODY(dx_rid_trade, dx_trade_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_trade)
GET_RECORD_PTR_BODY(dx_rid_quote, dx_quote_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_quote)
GET_RECORD_PTR_BODY(dx_rid_summary, dx_summary_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_summary)
GET_RECORD_PTR_BODY(dx_rid_profile, dx_profile_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_profile)
GET_RECORD_PTR_BODY(dx_rid_market_maker, dx_market_maker_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_market_maker)
GET_RECORD_PTR_BODY(dx_rid_order, dx_order_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_order)
GET_RECORD_PTR_BODY(dx_rid_time_and_sale, dx_time_and_sale_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_time_and_sale)
GET_RECORD_PTR_BODY(dx_rid_candle, dx_candle_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_candle)
GET_RECORD_PTR_BODY(dx_rid_trade_eth, dx_trade_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_trade_eth)
GET_RECORD_PTR_BODY(dx_rid_spread_order, dx_spread_order_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_spread_order)
GET_RECORD_PTR_BODY(dx_rid_greeks, dx_greeks_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_greeks)
GET_RECORD_PTR_BODY(dx_rid_theo_price, dx_theo_price_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_theo_price)
GET_RECORD_PTR_BODY(dx_rid_underlying, dx_underlying_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_underlying)
GET_RECORD_PTR_BODY(dx_rid_series, dx_series_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_series)
GET_RECORD_PTR_BODY(dx_rid_configuration, dx_configuration_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_configuration)

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manager collection
 */
/* -------------------------------------------------------------------------- */

const dx_buffer_manager_collection_t g_buffer_managers[dx_rid_count] = {
	{ GET_RECORD_PTR_NAME(dx_rid_trade), GET_RECORD_BUF_PTR_NAME(dx_rid_trade) },
	{ GET_RECORD_PTR_NAME(dx_rid_quote), GET_RECORD_BUF_PTR_NAME(dx_rid_quote) },
	{ GET_RECORD_PTR_NAME(dx_rid_summary), GET_RECORD_BUF_PTR_NAME(dx_rid_summary) },
	{ GET_RECORD_PTR_NAME(dx_rid_profile), GET_RECORD_BUF_PTR_NAME(dx_rid_profile) },
	{ GET_RECORD_PTR_NAME(dx_rid_market_maker), GET_RECORD_BUF_PTR_NAME(dx_rid_market_maker) },
	{ GET_RECORD_PTR_NAME(dx_rid_order), GET_RECORD_BUF_PTR_NAME(dx_rid_order) },
	{ GET_RECORD_PTR_NAME(dx_rid_time_and_sale), GET_RECORD_BUF_PTR_NAME(dx_rid_time_and_sale) },
	{ GET_RECORD_PTR_NAME(dx_rid_candle), GET_RECORD_BUF_PTR_NAME(dx_rid_candle) },
	{ GET_RECORD_PTR_NAME(dx_rid_trade_eth), GET_RECORD_BUF_PTR_NAME(dx_rid_trade_eth) },
	{ GET_RECORD_PTR_NAME(dx_rid_spread_order), GET_RECORD_BUF_PTR_NAME(dx_rid_spread_order) },
	{ GET_RECORD_PTR_NAME(dx_rid_greeks), GET_RECORD_BUF_PTR_NAME(dx_rid_greeks) },
	{ GET_RECORD_PTR_NAME(dx_rid_theo_price), GET_RECORD_BUF_PTR_NAME(dx_rid_theo_price) },
	{ GET_RECORD_PTR_NAME(dx_rid_underlying), GET_RECORD_BUF_PTR_NAME(dx_rid_underlying) },
	{ GET_RECORD_PTR_NAME(dx_rid_series), GET_RECORD_BUF_PTR_NAME(dx_rid_series) },
	{ GET_RECORD_PTR_NAME(dx_rid_configuration), GET_RECORD_BUF_PTR_NAME(dx_rid_configuration) }
};

void dx_clear_record_buffers (dx_event_record_buffer_t* record_buffers) {
	dx_record_info_id_t info_id = dx_rid_begin;

	for (; info_id < dx_rid_count; ++info_id) {
		if (record_buffers[info_id].buffer != NULL) {
			dx_free(record_buffers[info_id].buffer);

			record_buffers[info_id].buffer = NULL;
			record_buffers[info_id].capacity = 0;
		}
	}
}

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary memory management stuff
 */
/* -------------------------------------------------------------------------- */

bool dx_store_string_buffer (void* context, dxf_const_string_t buf) {
	return dx_string_array_add(&(CTX(context)->string_buffers), buf);
}

/* -------------------------------------------------------------------------- */

bool dx_store_byte_array_buffer(void* context, dxf_byte_array_t buf) {
	return dx_byte_buffer_array_add(&(CTX(context)->byte_array_buffers), buf);
}

/* -------------------------------------------------------------------------- */

void dx_free_string_buffers_impl (dx_string_array_t* string_buffers) {
	dx_string_array_free(string_buffers);
}

void dx_free_byte_array_buffers_impl(dx_byte_buffer_array_t* byte_array_buffers) {
	dx_byte_buffer_array_free(byte_array_buffers);
}

/* ---------------------------------- */

void dx_free_buffers(void* context) {
	dx_free_string_buffers_impl(&(CTX(context)->string_buffers));
	dx_free_byte_array_buffers_impl(&(CTX(context)->byte_array_buffers));
}
