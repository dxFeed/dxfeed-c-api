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

#include <limits.h>

#include "RecordTranscoder.h"
#include "EventData.h"
#include "DXMemory.h"
#include "EventSubscription.h"
#include "DataStructures.h"
#include "DXAlgorithms.h"
#include "RecordBuffers.h"
#include "ConnectionContextData.h"
#include "DXErrorHandling.h"
#include "ConfigurationDeserializer.h"

/* -------------------------------------------------------------------------- */
/*
 *	Common fiddling macros and constants
 */
/* -------------------------------------------------------------------------- */

static const dxf_uint_t DX_SEQUENCE_MASK     = 0x3FFFFFU;
static const dxf_byte_t DX_SEQUENCE_MS_SHIFT = 22;
static const dxf_uint_t DX_SEQUENCE_MS_MASK  = 0x3FFU;

static const dxf_long_t DX_TIME_TO_MS = 1000L;
                                   
#define DX_TIME_FIELD_TO_MS(field) (((dxf_long_t)(field)) * DX_TIME_TO_MS) // -> dxf_long_t
#define DX_SEQUENCE_TO_MS(rec)     (((rec)->sequence >> DX_SEQUENCE_MS_SHIFT) & DX_SEQUENCE_MS_MASK) // -> dxf_uint_t
#define DX_TIME_SEQ_TO_MS(rec)     (DX_TIME_FIELD_TO_MS((rec)->time) + DX_SEQUENCE_TO_MS(rec)) // -> dxf_long_t
#define DX_SEQUENCE(rec)           ((rec)->sequence & DX_SEQUENCE_MASK) // -> dxf_uint_t

#define DX_EXCHANGE_FROM_RECORD(rp) ((rp)->suffix == NULL ? 0 : (rp)->suffix[0])

dxf_ulong_t int_to_bits(dxf_int_t value)
{
	return (dxf_ulong_t)(dxf_uint_t)value;
}

dxf_ulong_t char_to_bits(dxf_char_t value)
{
	size_t leading_zero_bits = CHAR_BIT * (sizeof(dxf_ulong_t) - sizeof(dxf_char_t));
	dxf_ulong_t res = ((dxf_ulong_t)value) << leading_zero_bits;
	return res >> leading_zero_bits;
}

/* -------------------------------------------------------------------------- */
/*
 *	Trade flags constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_uint_t DX_TRADE_FLAGS_ETH       = 1;
static const dxf_byte_t DX_TRADE_FLAGS_DIR_SHIFT = 1;
static const dxf_uint_t DX_TRADE_FLAGS_DIR_MASK  = 0x7;

#define DX_TRADE_GET_DIR(rec) ((dxf_direction_t)(((rec)->flags >> DX_TRADE_FLAGS_DIR_SHIFT) & DX_TRADE_FLAGS_DIR_MASK))
#define DX_TRADE_GET_ETH(rec) (((rec)->flags & DX_TRADE_FLAGS_ETH) == DX_TRADE_FLAGS_ETH)

/* -------------------------------------------------------------------------- */
/*
 *	Summary flags constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_byte_t DX_SUMMARY_FLAGS_DCPT_SHIFT  = 2;
static const dxf_byte_t DX_SUMMARY_FLAGS_PDCPT_SHIFT = 0;
static const dxf_uint_t DX_SUMMARY_FLAGS_CPT_MASK    = 0x3;

#define DX_SUMMARY_GET_DCPT(rec)  ((dxf_price_type_t)(((rec)->flags >> DX_SUMMARY_FLAGS_DCPT_SHIFT) & DX_SUMMARY_FLAGS_CPT_MASK))
#define DX_SUMMARY_GET_PDCPT(rec) ((dxf_price_type_t)(((rec)->flags >> DX_SUMMARY_FLAGS_PDCPT_SHIFT) & DX_SUMMARY_FLAGS_CPT_MASK))

/* -------------------------------------------------------------------------- */
/*
 *	Profile flags constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_byte_t DX_PROFILE_FLAGS_TS_SHIFT  = 0;
static const dxf_uint_t DX_PROFILE_FLAGS_TS_MASK   = 0x3;
static const dxf_byte_t DX_PROFILE_FLAGS_SSR_SHIFT = 2;
static const dxf_uint_t DX_PROFILE_FLAGS_SSR_MASK  = 0x3;

#define DX_PROFILE_GET_TS(rec)  ((dxf_trading_status_t)(((rec)->flags >> DX_PROFILE_FLAGS_TS_SHIFT) & DX_PROFILE_FLAGS_TS_MASK))
#define DX_PROFILE_GET_SSR(rec) ((dxf_short_sale_restriction_t)(((rec)->flags >> DX_PROFILE_FLAGS_SSR_SHIFT) & DX_PROFILE_FLAGS_SSR_MASK))

/* -------------------------------------------------------------------------- */
/*
 *	Order index calculation constants
 */
/* -------------------------------------------------------------------------- */


static const dxf_long_t DX_ORDER_SOURCE_COMPOSITE_BID = 1;
static const dxf_long_t DX_ORDER_SOURCE_COMPOSITE_ASK = 2;
static const dxf_long_t DX_ORDER_SOURCE_REGIONAL_BID  = 3;
static const dxf_long_t DX_ORDER_SOURCE_REGIONAL_ASK  = 4;
static const dxf_long_t DX_ORDER_SOURCE_AGGREGATE_BID = 5;
static const dxf_long_t DX_ORDER_SOURCE_AGGREGATE_ASK = 6;

static const dxf_byte_t DX_ORDER_INDEX_SSRC_SHIFT = 48;
static const dxf_byte_t DX_ORDER_INDEX_EXC_SHIFT  = 32;
static const dxf_byte_t DX_ORDER_INDEX_TSRC_SHIFT = 32;

static const dxf_byte_t DX_ORDER_FLAGS_SCOPE_SHIFT    = 0;
static const dxf_uint_t DX_ORDER_FLAGS_SCOPE_MASK     = 0x03;
static const dxf_byte_t DX_ORDER_FLAGS_SIDE_SHIFT     = 2;
static const dxf_uint_t DX_ORDER_FLAGS_SIDE_MASK      = 0x03;
static const dxf_byte_t DX_ORDER_FLAGS_EXCHANGE_SHIFT = 4;
static const dxf_uint_t DX_ORDER_FLAGS_EXCHANGE_MASK  = 0x7f;

#define DX_ORDER_FROM_QUOTE_BID_INDEX(e) (((e) == 0 ? DX_ORDER_SOURCE_COMPOSITE_BID : DX_ORDER_SOURCE_REGIONAL_BID) << DX_ORDER_INDEX_SSRC_SHIFT) | (char_to_bits(e) << DX_ORDER_INDEX_EXC_SHIFT)
#define DX_ORDER_FROM_QUOTE_ASK_INDEX(e) (((e) == 0 ? DX_ORDER_SOURCE_COMPOSITE_ASK : DX_ORDER_SOURCE_REGIONAL_ASK) << DX_ORDER_INDEX_SSRC_SHIFT) | (char_to_bits(e) << DX_ORDER_INDEX_EXC_SHIFT)

#define DX_ORDER_FORM_MM_BID_INDEX(mm) ((DX_ORDER_SOURCE_AGGREGATE_BID << DX_ORDER_INDEX_SSRC_SHIFT) | (char_to_bits((mm)->mm_exchange) << DX_ORDER_INDEX_EXC_SHIFT) | (int_to_bits((mm)->mm_id)))
#define DX_ORDER_FORM_MM_ASK_INDEX(mm) ((DX_ORDER_SOURCE_AGGREGATE_ASK << DX_ORDER_INDEX_SSRC_SHIFT) | (char_to_bits((mm)->mm_exchange) << DX_ORDER_INDEX_EXC_SHIFT) | (int_to_bits((mm)->mm_id)))

#define DX_ORDER_INDEX(order, suffix) ((suffix) << DX_ORDER_INDEX_TSRC_SHIFT | int_to_bits(order->index))

#define DX_ORDER_GET_SCOPE(rec)    ((dxf_order_scope_t)(((rec)->flags >> DX_ORDER_FLAGS_SCOPE_SHIFT) & DX_ORDER_FLAGS_SCOPE_MASK))
#define DX_ORDER_GET_SIDE(rec)     ((dxf_order_side_t)(((rec)->flags >> DX_ORDER_FLAGS_SIDE_SHIFT) & DX_ORDER_FLAGS_SIDE_MASK))
#define DX_ORDER_GET_EXCHANGE(rec) ((dxf_char_t)(((rec)->flags >> DX_ORDER_FLAGS_EXCHANGE_SHIFT) & DX_ORDER_FLAGS_EXCHANGE_MASK))

/* -------------------------------------------------------------------------- */
/*
 *	TimeAndSale calculation constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_byte_t DX_TNS_FLAGS_TYPE_SHIFT = 0;
static const dxf_uint_t DX_TNS_FLAGS_TYPE_MASK  = 0x3;
static const dxf_uint_t DX_TNS_FLAGS_VALID_TICK = 0x4;
static const dxf_uint_t DX_TNS_FLAGS_ETH        = 0x08;
static const dxf_uint_t DX_TNS_FLAGS_SPREAD     = 0x10;
static const dxf_byte_t DX_TNS_FLAGS_SIDE_SHIFT = 5;
static const dxf_uint_t DX_TNS_FLAGS_SIDE_MASK  = 0x3;
static const dxf_byte_t DX_TNS_FLAGS_TTE_SHIFT  = 8;
static const dxf_uint_t DX_TNS_FLAGS_TTE_MASK   = 0xff;

static const dxf_byte_t DX_TNS_INDEX_TIME_SHIFT = 32;

#define DX_TNS_GET_TYPE(rec)       ((dxf_tns_type_t)(((rec)->flags >> DX_TNS_FLAGS_TYPE_SHIFT) & DX_TNS_FLAGS_TYPE_MASK))
#define DX_TNS_GET_VALID_TICK(rec) (((rec)->flags & DX_TNS_FLAGS_VALID_TICK) == DX_TNS_FLAGS_VALID_TICK)
#define DX_TNS_GET_ETH(rec)        (((rec)->flags & DX_TNS_FLAGS_ETH) == DX_TNS_FLAGS_ETH)
#define DX_TNS_GET_SPREAD_LEG(rec) (((rec)->flags & DX_TNS_FLAGS_SPREAD) == DX_TNS_FLAGS_SPREAD)
#define DX_TNS_GET_SIDE(rec)       ((dxf_order_side_t)(((rec)->flags >> DX_TNS_FLAGS_SIDE_SHIFT) & DX_TNS_FLAGS_SIDE_MASK))
#define DX_TNS_GET_TTE(rec)        ((dxf_char_t)(((rec)->flags >> DX_TNS_FLAGS_TTE_SHIFT) & DX_TNS_FLAGS_TTE_MASK))

/* -------------------------------------------------------------------------- */
/*
 *	Candle calculation constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_byte_t DX_CANDLE_INDEX_TIME_SHIFT = 32;

/* -------------------------------------------------------------------------- */
/*
 *	Candle calculation constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_byte_t DX_GREEKS_INDEX_TIME_SHIFT = 32;

/* -------------------------------------------------------------------------- */
/*
 *	Series calculation constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_byte_t DX_SERIES_INDEX_TIME_SHIFT = 32;

/* -------------------------------------------------------------------------- */
/*
 *	Record transcoder connection context
 */
/* -------------------------------------------------------------------------- */

/* Must be synchronized with dx_event_id_t */
const size_t dx_event_sizes[] = {
    sizeof(dxf_trade_t),
	sizeof(dxf_quote_t),
	sizeof(dxf_summary_t),
	sizeof(dxf_profile_t),
	sizeof(dxf_order_t),
	sizeof(dxf_time_and_sale_t),
	sizeof(dxf_candle_t),
	sizeof(dxf_trade_t), // As trade ETH
	sizeof(dxf_order_t), // As spread order
	sizeof(dxf_greeks_t),
	sizeof(dxf_theo_price_t),
	sizeof(dxf_underlying_t),
	sizeof(dxf_series_t),
	sizeof(dxf_configuration_t)
};

typedef struct {
	struct {
		dxf_event_data_t buffer;
		int count;
	} event_buffers[dx_eid_count];

	dxf_connection_t connection;
	void* rbcc;
	void* dscc;
} dx_record_transcoder_connection_context_t;

#define CTX(context) \
	((dx_record_transcoder_connection_context_t*)context)

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_record_transcoder) {
	dx_record_transcoder_connection_context_t* context = NULL;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	context = dx_calloc(1, sizeof(dx_record_transcoder_connection_context_t));

	if (context == NULL) {
		return false;
	}

	context->connection = connection;

	if ((context->rbcc = dx_get_record_buffers_connection_context(connection)) == NULL) {
		dx_free(context);

		return dx_set_error_code(dx_cec_connection_context_not_initialized);
	}

	if ((context->dscc = dx_get_data_structures_connection_context(connection)) == NULL) {
		dx_free(context);

		return dx_set_error_code(dx_cec_connection_context_not_initialized);
	}

	if (!dx_set_subsystem_data(connection, dx_ccs_record_transcoder, context)) {
		dx_free(context);

		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_record_transcoder) {
	bool res = true;
	dx_record_transcoder_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_record_transcoder, &res);

	if (context == NULL) {
		return res;
	}

	for (int i = dx_eid_begin; i < dx_eid_count; i++) {
		if (context->event_buffers[i].buffer)
			dx_free(context->event_buffers[i].buffer);
	}

	dx_free(context);

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_record_transcoder) {
	return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Event data buffer functions

 *  some transcoders require separate data structures to be allocated and filled
 *  based on the record data they receive
 */
/* -------------------------------------------------------------------------- */

void* dx_initialize_event_data_buffer(int count, size_t struct_size,
									OUT void** buffer_data, OUT int* buffer_count) {
	if (*buffer_count >= count) {
		dx_memset(*buffer_data, 0, count * struct_size);
		return *buffer_data;
	}

	if (*buffer_data != NULL) {
		dx_free(*buffer_data);
	}

	*buffer_data = NULL;
	*buffer_count = 0;

	if ((*buffer_data = dx_calloc(count, struct_size)) != NULL) {
		*buffer_count = count;
	}

	return *buffer_data;
}

dxf_event_data_t dx_get_event_data_buffer(dx_record_transcoder_connection_context_t* context,
										dx_event_id_t event_id, int count) {

	if (event_id < dx_eid_begin || event_id >= dx_eid_count) {
		dx_set_error_code(dx_ec_internal_assert_violation);
		return NULL;
	}
	return dx_initialize_event_data_buffer(count, dx_event_sizes[event_id],
			(void**)&(context->event_buffers[event_id].buffer), &(context->event_buffers[event_id].count));
}

/* -------------------------------------------------------------------------- */
/*
 *	Record transcoder macros and prototypes
 */
/* -------------------------------------------------------------------------- */

#define RECORD_TRANSCODER_NAME(struct_name) \
	struct_name##_transcoder

typedef bool (*dx_record_transcoder_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buffer, int record_count);

/* -------------------------------------------------------------------------- */
/*
 *	Record transcoders implementation
 */
/* -------------------------------------------------------------------------- */

static bool dx_trade_t_transcoder_impl(dx_record_transcoder_connection_context_t* context,
	const dx_record_params_t* record_params,
	const dxf_event_params_t* event_params,
	dx_trade_t* record_buffer, int record_count, dx_event_id_t event_id)
{
	if (event_id != dx_eid_trade && event_id != dx_eid_trade_eth) {
		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	dxf_trade_t* event_buffer = (dxf_trade_t*) dx_get_event_data_buffer(context, event_id, record_count);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	if (!exchange_code)
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);

	for (int i = 0; i < record_count; ++i) {
		dx_trade_t* cur_record = record_buffer + i;
		dxf_trade_t* cur_event = event_buffer + i;

		cur_event->time = DX_TIME_SEQ_TO_MS(cur_record);
		cur_event->sequence = DX_SEQUENCE(cur_record);
		cur_event->time_nanos = cur_record->time_nanos;
		cur_event->exchange_code = cur_record->exchange_code ? cur_record->exchange_code : exchange_code;
		cur_event->price = cur_record->price;
		cur_event->size = cur_record->size;
		if (event_id == dx_eid_trade) {
			cur_event->tick = cur_record->tick;
			cur_event->change = cur_record->change;
		}
		cur_event->raw_flags = cur_record->flags;
		cur_event->day_volume = cur_record->day_volume;
		cur_event->day_turnover = cur_record->day_turnover;
		cur_event->direction = DX_TRADE_GET_DIR(cur_record);
		cur_event->is_eth = DX_TRADE_GET_ETH(cur_record);
		cur_event->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);
	}

	return dx_process_event_data(context->connection, event_id, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

bool RECORD_TRANSCODER_NAME(dx_trade_t) (dx_record_transcoder_connection_context_t* context,
	const dx_record_params_t* record_params,
	const dxf_event_params_t* event_params,
	dx_trade_t* record_buffer, int record_count)
{
	return dx_trade_t_transcoder_impl(context, record_params, event_params, record_buffer, record_count, dx_eid_trade);
}

/* -------------------------------------------------------------------------- */

bool dx_transcode_quote_to_order_bid (dx_record_transcoder_connection_context_t* context,
									const dx_record_params_t* record_params,
									const dxf_event_params_t* event_params,
									dx_quote_t* record_buffer, int record_count) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_quote_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;

		cur_event->index = DX_ORDER_FROM_QUOTE_BID_INDEX(exchange_code);
		cur_event->time = DX_TIME_FIELD_TO_MS(cur_record->bid_time);
		if (cur_record->bid_time > cur_record->ask_time) {
			cur_event->time += DX_SEQUENCE_TO_MS(cur_record);
			cur_event->time_nanos = cur_record->time_nanos;
 		} else {
			cur_event->time_nanos = 0;
 		}
		cur_event->sequence = DX_SEQUENCE(cur_record);
		cur_event->price = cur_record->bid_price;
		cur_event->size = cur_record->bid_size;
		cur_event->count = 0;
		cur_event->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);
		cur_event->side = dxf_osd_buy;
		cur_event->exchange_code = (exchange_code == 0 ? cur_record->bid_exchange_code : exchange_code);
		dx_memset(cur_event->source, 0, sizeof(cur_event->source));
		cur_event->market_maker = NULL;
	}

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* ---------------------------------- */

bool dx_transcode_quote_to_order_ask (dx_record_transcoder_connection_context_t* context,
									const dx_record_params_t* record_params,
									const dxf_event_params_t* event_params,
									dx_quote_t* record_buffer, int record_count) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_quote_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;

		cur_event->event_flags = 0;
		cur_event->index = DX_ORDER_FROM_QUOTE_ASK_INDEX(exchange_code);
		cur_event->time = DX_TIME_FIELD_TO_MS(cur_record->ask_time);
		if (cur_record->ask_time > cur_record->bid_time) {
			cur_event->time += DX_SEQUENCE_TO_MS(cur_record);
			cur_event->time_nanos = cur_record->time_nanos;
 		} else {
			cur_event->time_nanos = 0;
 		}
		cur_event->sequence = DX_SEQUENCE(cur_record);
		cur_event->price = cur_record->ask_price;
		cur_event->size = cur_record->ask_size;
		cur_event->count = 0;
		cur_event->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);
		cur_event->side = dxf_osd_sell;
		cur_event->exchange_code = (exchange_code == 0 ? cur_record->ask_exchange_code : exchange_code);
		dx_memset(cur_event->source, 0, sizeof(cur_event->source));
		cur_event->market_maker = NULL;
	}

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* ---------------------------------- */

bool dx_transcode_quote (dx_record_transcoder_connection_context_t* context,
						const dx_record_params_t* record_params,
						const dxf_event_params_t* event_params,
						dx_quote_t* record_buffer, int record_count) {
	dxf_quote_t* event_buffer = (dxf_quote_t*)dx_get_event_data_buffer(context, dx_eid_quote, record_count);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	if (exchange_code != 0) {
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);
	}

	for (int i = 0; i < record_count; ++i) {
		dx_quote_t* cur_record = record_buffer + i;
		dxf_quote_t* cur_event = event_buffer + i;

		cur_event->time = DX_TIME_FIELD_TO_MS(cur_record->bid_time > cur_record->ask_time ? cur_record->bid_time : cur_record->ask_time) + DX_SEQUENCE_TO_MS(cur_record);
		cur_event->sequence = DX_SEQUENCE(cur_record);
		cur_event->time_nanos = cur_record->time_nanos;
		cur_event->bid_time = DX_TIME_FIELD_TO_MS(cur_record->bid_time);
		cur_event->bid_exchange_code = exchange_code ? exchange_code : cur_record->bid_exchange_code;
		cur_event->bid_price = cur_record->bid_price;
		cur_event->bid_size = cur_record->bid_size;
		cur_event->ask_time = DX_TIME_FIELD_TO_MS(cur_record->ask_time);
		cur_event->ask_exchange_code = exchange_code ? exchange_code : cur_record->ask_exchange_code;
		cur_event->ask_price = cur_record->ask_price;
		cur_event->ask_size = cur_record->ask_size;
		cur_event->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);
	}

	return dx_process_event_data(context->connection, dx_eid_quote, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* ---------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_quote_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_quote_t* record_buffer, int record_count) {

	if (!dx_transcode_quote_to_order_bid(context, record_params, event_params, record_buffer, record_count)) {
		return false;
	}

	if (!dx_transcode_quote_to_order_ask(context, record_params, event_params, record_buffer, record_count)) {
		return false;
	}

	if (!dx_transcode_quote(context, record_params, event_params, record_buffer, record_count)) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_summary_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_summary_t* record_buffer, int record_count) {

	dxf_summary_t* event_buffer = (dxf_summary_t*)dx_get_event_data_buffer(context, dx_eid_summary, record_count);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	if (exchange_code != 0) {
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);
	}

	for (int i = 0; i < record_count; ++i) {
		dx_summary_t* cur_record = record_buffer + i;
		dxf_summary_t* cur_event = event_buffer + i;

		cur_event->day_id = cur_record->day_id;	
		cur_event->day_open_price = cur_record->day_open_price;
		cur_event->day_high_price = cur_record->day_high_price;
		cur_event->day_low_price = cur_record->day_low_price;
		cur_event->day_close_price = cur_record->day_close_price;
		cur_event->prev_day_id = cur_record->prev_day_id;
		cur_event->prev_day_close_price = cur_record->prev_day_close_price;
		cur_event->prev_day_volume = cur_record->prev_day_volume;
		cur_event->open_interest = cur_record->open_interest;
		cur_event->raw_flags = cur_record->flags;
		cur_event->exchange_code = exchange_code;
		cur_event->day_close_price_type = DX_SUMMARY_GET_DCPT(cur_record);
		cur_event->prev_day_close_price_type = DX_SUMMARY_GET_PDCPT(cur_record);
		cur_event->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);
	}

	return dx_process_event_data(context->connection, dx_eid_summary, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_profile_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_profile_t* record_buffer, int record_count) {
	dxf_profile_t* event_buffer = (dxf_profile_t*)dx_get_event_data_buffer(context, dx_eid_profile, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_profile_t* cur_record = record_buffer + i;
		dxf_profile_t* cur_event = event_buffer + i;

		cur_event->beta = cur_record->beta;
		cur_event->eps = cur_record->eps;
		cur_event->div_freq = cur_record->div_freq;
		cur_event->exd_div_amount = cur_record->exd_div_amount;
		cur_event->exd_div_date = cur_record->exd_div_date;
		cur_event->_52_high_price = cur_record->_52_high_price;
		cur_event->_52_low_price = cur_record->_52_low_price;
		cur_event->shares = cur_record->shares;
		cur_event->free_float = cur_record->free_float;
		cur_event->high_limit_price = cur_record->high_limit_price;
		cur_event->low_limit_price = cur_record->low_limit_price;
		cur_event->halt_start_time = DX_TIME_FIELD_TO_MS(cur_record->halt_start_time);
		cur_event->halt_end_time = DX_TIME_FIELD_TO_MS(cur_record->halt_end_time);
		cur_event->raw_flags = cur_record->flags;

		if (cur_record->description != NULL) {
			cur_event->description = dx_create_string_src(cur_record->description);
			if (cur_event->description && !dx_store_string_buffer(context->rbcc, cur_event->description))
				return false;
		}

		if (cur_record->status_reason != NULL) {
			cur_event->status_reason = dx_create_string_src(cur_record->status_reason);
			if (cur_event->status_reason && !dx_store_string_buffer(context->rbcc, cur_event->status_reason))
				return false;
		}

		cur_event->trading_status = DX_PROFILE_GET_TS(cur_record);
		cur_event->ssr = DX_PROFILE_GET_SSR(cur_record);
	}

	return dx_process_event_data(context->connection, dx_eid_profile, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool dx_transcode_market_maker_to_order_bid (dx_record_transcoder_connection_context_t* context,
											const dx_record_params_t* record_params,
											const dxf_event_params_t* event_params,
											dx_market_maker_t* record_buffer, int record_count) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_market_maker_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;
		dxf_char_t exchange_code = (dxf_char_t)cur_record->mm_exchange;
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);

		cur_event->event_flags = event_params->flags;
		cur_event->index = DX_ORDER_FORM_MM_BID_INDEX(cur_record);
		cur_event->time = DX_TIME_FIELD_TO_MS(cur_record->mmbid_time);
		cur_event->sequence = 0;
		cur_event->price = cur_record->mmbid_price;
		cur_event->size = cur_record->mmbid_size;
		cur_event->count = cur_record->mmbid_count;
		cur_event->scope = dxf_osc_aggregate;
		cur_event->side = dxf_osd_buy;
		cur_event->exchange_code = cur_record->mm_exchange;
		dx_memset(cur_event->source, 0, sizeof(cur_event->source));
		cur_event->market_maker = dx_decode_from_integer(cur_record->mm_id);

		if (!IS_FLAG_SET(record_params->flags, dxf_ef_remove_event) && (cur_event->market_maker == NULL || !dx_store_string_buffer(context->rbcc, cur_event->market_maker))) {
			return false;
		}
	}

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* ---------------------------------- */

bool dx_transcode_market_maker_to_order_ask (dx_record_transcoder_connection_context_t* context,
											const dx_record_params_t* record_params,
											const dxf_event_params_t* event_params,
											dx_market_maker_t* record_buffer, int record_count) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_market_maker_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;
		dxf_char_t exchange_code = (dxf_char_t)cur_record->mm_exchange;
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);

		cur_event->event_flags = event_params->flags;
		cur_event->index = DX_ORDER_FORM_MM_ASK_INDEX(cur_record);
		cur_event->time = DX_TIME_FIELD_TO_MS(cur_record->mmask_time);
		cur_event->sequence = 0;
		cur_event->price = cur_record->mmask_price;
		cur_event->size = cur_record->mmask_size;
		cur_event->count = cur_record->mmask_count;
		cur_event->scope = dxf_osc_aggregate;
		cur_event->side = dxf_osd_sell;
		cur_event->exchange_code = cur_record->mm_exchange;
		dx_memset(cur_event->source, 0, sizeof(cur_event->source));
		cur_event->market_maker = dx_decode_from_integer(cur_record->mm_id);

		if (!IS_FLAG_SET(record_params->flags, dxf_ef_remove_event) && (cur_event->market_maker == NULL || !dx_store_string_buffer(context->rbcc, cur_event->market_maker))) {
			return false;
		}
	}

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* ---------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_market_maker_t) (dx_record_transcoder_connection_context_t* context,
												const dx_record_params_t* record_params,
												const dxf_event_params_t* event_params,
												dx_market_maker_t* record_buffer, int record_count) {
	if (!dx_transcode_market_maker_to_order_bid(context, record_params, event_params, record_buffer, record_count)) {
		return false;
	}

	if (!dx_transcode_market_maker_to_order_ask(context, record_params, event_params, record_buffer, record_count)) {
		return false;
	}

	return true;
}

/* ---------------------------------- */

#define MAX_SUFFIX_LEN_FOR_INDEX 4

dxf_long_t suffix_to_long(dxf_const_string_t suffix)
{
	size_t suffix_length = 0;
	size_t i = 0;
	dxf_long_t ret = 0;
	if (suffix == NULL)
		return 0;
	suffix_length = dx_string_length(suffix);
	if (suffix_length == 0)
		return 0;

	if (suffix_length > MAX_SUFFIX_LEN_FOR_INDEX)
		i = suffix_length - MAX_SUFFIX_LEN_FOR_INDEX;
	for (; i < suffix_length; i++) {
		dxf_long_t ch = (dxf_long_t)suffix[i] & 0xFF;
		ret = (ret << 8) | ch;
	}
	return ret;
}

bool RECORD_TRANSCODER_NAME(dx_order_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_order_t* record_buffer, int record_count) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);
	dxf_const_string_t suffix = record_params->suffix;
	dxf_long_t src = suffix_to_long(suffix);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_order_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;

		cur_event->event_flags = event_params->flags;
		cur_event->index = DX_ORDER_INDEX(cur_record, src);
		cur_event->time = DX_TIME_SEQ_TO_MS(cur_record);
		cur_event->time_nanos = cur_record->time_nanos;
		cur_event->sequence = DX_SEQUENCE(cur_record);
		cur_event->price = cur_record->price;
		cur_event->size = cur_record->size;
		cur_event->count = cur_record->count;
		cur_event->scope = DX_ORDER_GET_SCOPE(cur_record);
		cur_event->side = DX_ORDER_GET_SIDE(cur_record);
		cur_event->exchange_code = DX_ORDER_GET_EXCHANGE(cur_record);

		dx_memset(cur_event->source, 0, sizeof(cur_event->source));
		dx_copy_string_len(cur_event->source, suffix, dx_string_length(suffix));

		cur_event->market_maker = dx_decode_from_integer(cur_record->mmid);

		if (cur_event->market_maker != NULL && !dx_store_string_buffer(context->rbcc, cur_event->market_maker)) {
			return false;
		}
	}

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_time_and_sale_t) (dx_record_transcoder_connection_context_t* context,
												const dx_record_params_t* record_params,
												const dxf_event_params_t* event_params,
												dx_time_and_sale_t* record_buffer, int record_count) {
	dxf_time_and_sale_t* event_buffer = (dxf_time_and_sale_t*)dx_get_event_data_buffer(context, dx_eid_time_and_sale, record_count);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_time_and_sale_t* cur_record = record_buffer + i;
		dxf_time_and_sale_t* cur_event = event_buffer + i;

		cur_event->event_flags = event_params->flags;
		cur_event->index = (int_to_bits(cur_record->time) << DX_TNS_INDEX_TIME_SHIFT) | int_to_bits(cur_record->sequence);
		cur_event->time = DX_TIME_SEQ_TO_MS(cur_record);
		cur_event->exchange_code = cur_record->exchange_code;
		cur_event->price = cur_record->price;
		cur_event->size = cur_record->size;
		cur_event->bid_price = cur_record->bid_price;
		cur_event->ask_price = cur_record->ask_price;

		cur_event->exchange_sale_conditions = dx_decode_from_integer(cur_record->exchange_sale_conditions);
		if (cur_event->exchange_sale_conditions && !dx_store_string_buffer(context->rbcc, cur_event->exchange_sale_conditions)) {
			return false;
		}

		cur_event->raw_flags = cur_record->flags;

		if (cur_record->buyer != NULL) {
			cur_event->buyer = dx_create_string_src(cur_record->buyer);
			if (cur_event->buyer && !dx_store_string_buffer(context->rbcc, cur_event->buyer)) {
				return false;
			}
		}

		if (cur_record->seller != NULL) {
			cur_event->seller = dx_create_string_src(cur_record->seller);
			if (cur_event->seller && !dx_store_string_buffer(context->rbcc, cur_event->seller)) {
				return false;
			}
		}

		cur_event->side = DX_TNS_GET_SIDE(cur_record);
		cur_event->type = DX_TNS_GET_TYPE(cur_record);
		cur_event->is_valid_tick = DX_TNS_GET_VALID_TICK(cur_record);
		cur_event->is_eth_trade = DX_TNS_GET_ETH(cur_record);
		cur_event->trade_through_exempt = DX_TNS_GET_TTE(cur_record);
		cur_event->is_spread_leg = DX_TNS_GET_SPREAD_LEG(cur_record);
		cur_event->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);
	}

	return dx_process_event_data(context->connection, dx_eid_time_and_sale, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_candle_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_candle_t* record_buffer, int record_count) {
	dxf_candle_t* event_buffer = (dxf_candle_t*)dx_get_event_data_buffer(context, dx_eid_candle, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_candle_t* cur_record = record_buffer + i;
		dxf_candle_t* cur_event = event_buffer + i;

		cur_event->event_flags = event_params->flags;
		cur_event->index = (int_to_bits(cur_record->time) << DX_CANDLE_INDEX_TIME_SHIFT) | int_to_bits(cur_record->sequence);
		cur_event->time = DX_TIME_SEQ_TO_MS(cur_record);
		cur_event->sequence = DX_SEQUENCE(cur_record);
		cur_event->count = cur_record->count;
		cur_event->open = cur_record->open;
		cur_event->high = cur_record->high;
		cur_event->low = cur_record->low;
		cur_event->close = cur_record->close;
		cur_event->volume = cur_record->volume;
		cur_event->vwap = cur_record->vwap;
		cur_event->bid_volume = cur_record->bid_volume;
		cur_event->ask_volume = cur_record->ask_volume;
		cur_event->open_interest = cur_record->open_interest;
		cur_event->imp_volatility = cur_record->imp_volatility;
	}

	return dx_process_event_data(context->connection, dx_eid_candle, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_trade_eth_t) (dx_record_transcoder_connection_context_t* context,
											const dx_record_params_t* record_params,
											const dxf_event_params_t* event_params,
											dx_trade_t* record_buffer, int record_count) {
	return dx_trade_t_transcoder_impl(context, record_params, event_params, record_buffer, record_count, dx_eid_trade_eth);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_spread_order_t) (dx_record_transcoder_connection_context_t* context,
												const dx_record_params_t* record_params,
												const dxf_event_params_t* event_params,
												dx_spread_order_t* record_buffer, int record_count) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);
	dxf_const_string_t suffix = record_params->suffix;
	dxf_long_t src = suffix_to_long(suffix);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_spread_order_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;

		cur_event->event_flags = event_params->flags;
		cur_event->index = DX_ORDER_INDEX(cur_record, src);
		cur_event->time = DX_TIME_SEQ_TO_MS(cur_record);
		cur_event->time_nanos = cur_record->time_nanos;
		cur_event->sequence = DX_SEQUENCE(cur_record);
		cur_event->price = cur_record->price;
		cur_event->size = cur_record->size;
		cur_event->count = cur_record->count;
		cur_event->scope = DX_ORDER_GET_SCOPE(cur_record);
		cur_event->side = DX_ORDER_GET_SIDE(cur_record);
		cur_event->exchange_code = DX_ORDER_GET_EXCHANGE(cur_record);

		dx_memset(cur_event->source, 0, sizeof(cur_event->source));
		dx_copy_string_len(cur_event->source, suffix, dx_string_length(suffix));

		if (cur_record->spread_symbol != NULL) {
			cur_event->spread_symbol = dx_create_string_src(cur_record->spread_symbol);
			if (cur_event->spread_symbol && !dx_store_string_buffer(context->rbcc, cur_event->spread_symbol))
				return false;
		}
	}

	return dx_process_event_data(context->connection, dx_eid_spread_order, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_greeks_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_greeks_t* record_buffer, int record_count) {
	dxf_greeks_t* event_buffer = (dxf_greeks_t*)dx_get_event_data_buffer(context, dx_eid_greeks, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_greeks_t* cur_record = record_buffer + i;
		dxf_greeks_t* cur_event = event_buffer + i;

		cur_event->event_flags = event_params->flags;
		cur_event->index = (int_to_bits(cur_record->time) << DX_GREEKS_INDEX_TIME_SHIFT) | int_to_bits(cur_record->sequence);
		cur_event->time = DX_TIME_SEQ_TO_MS(cur_record);
		cur_event->price = cur_record->price;
		cur_event->volatility = cur_record->volatility;
		cur_event->delta = cur_record->delta;
		cur_event->gamma = cur_record->gamma;
		cur_event->theta = cur_record->theta;
		cur_event->rho = cur_record->rho;
		cur_event->vega = cur_record->vega;
	}

	return dx_process_event_data(context->connection, dx_eid_greeks, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_theo_price_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_theo_price_t* record_buffer, int record_count) {
	dxf_theo_price_t* event_buffer = (dxf_theo_price_t*)record_buffer;

	for (int i = 0; i < record_count; ++i) {
		dxf_theo_price_t* cur_event = event_buffer + i;
		cur_event->time = DX_TIME_FIELD_TO_MS(cur_event->time);
	}

	return dx_process_event_data(context->connection, dx_eid_theo_price, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_underlying_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_underlying_t* record_buffer, int record_count) {
	dxf_underlying_t* event_buffer = (dxf_underlying_t*)record_buffer;

	return dx_process_event_data(context->connection, dx_eid_underlying, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_series_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_series_t* record_buffer, int record_count) {
	dxf_series_t* event_buffer = (dxf_series_t*)dx_get_event_data_buffer(context, dx_eid_series, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_series_t* cur_record = record_buffer + i;
		dxf_series_t* cur_event = event_buffer + i;

		cur_event->event_flags = event_params->flags;
		cur_event->index = cur_record->index;
		cur_event->time = DX_TIME_SEQ_TO_MS(cur_record);
		cur_event->sequence = DX_SEQUENCE(cur_record);
		cur_event->expiration = cur_record->expiration;
		cur_event->volatility = cur_record->volatility;
		cur_event->put_call_ratio = cur_record->put_call_ratio;
		cur_event->forward_price = cur_record->forward_price;
		cur_event->dividend = cur_record->dividend;
		cur_event->interest = cur_record->interest;
	}

	return dx_process_event_data(context->connection, dx_eid_series, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_configuration_t) (dx_record_transcoder_connection_context_t* context,
												const dx_record_params_t* record_params,
												const dxf_event_params_t* event_params,
												dx_configuration_t* record_buffer, int record_count) {
	dxf_configuration_t* event_buffer = (dxf_configuration_t*)dx_get_event_data_buffer(context, dx_eid_configuration, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_configuration_t* cur_record = record_buffer + i;
		dxf_configuration_t* cur_event = event_buffer + i;

		cur_event->version = cur_record->version;

		if (!dx_configuration_deserialize_string(&(cur_record->object), &(cur_event->object)) ||
			!dx_store_string_buffer(context->rbcc, cur_event->object)) {
			return false;
		}
	}

	return dx_process_event_data(context->connection, dx_eid_configuration, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */
/*
 *	Interface functions implementation
 */
/* -------------------------------------------------------------------------- */

static const dx_record_transcoder_t g_record_transcoders[dx_rid_count] = {
	RECORD_TRANSCODER_NAME(dx_trade_t),
	RECORD_TRANSCODER_NAME(dx_quote_t),
	RECORD_TRANSCODER_NAME(dx_summary_t),
	RECORD_TRANSCODER_NAME(dx_profile_t),
	RECORD_TRANSCODER_NAME(dx_market_maker_t),
	RECORD_TRANSCODER_NAME(dx_order_t),
	RECORD_TRANSCODER_NAME(dx_time_and_sale_t),
	RECORD_TRANSCODER_NAME(dx_candle_t),
	RECORD_TRANSCODER_NAME(dx_trade_eth_t),
	RECORD_TRANSCODER_NAME(dx_spread_order_t),
	RECORD_TRANSCODER_NAME(dx_greeks_t),
	RECORD_TRANSCODER_NAME(dx_theo_price_t),
	RECORD_TRANSCODER_NAME(dx_underlying_t),
	RECORD_TRANSCODER_NAME(dx_series_t),
	RECORD_TRANSCODER_NAME(dx_configuration_t)
};

/* -------------------------------------------------------------------------- */

bool dx_transcode_record_data (dxf_connection_t connection,
							const dx_record_params_t* record_params,
							const dxf_event_params_t* event_params,
							void* record_buffer, int record_count) {
	dx_record_transcoder_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_record_transcoder, NULL);
	return g_record_transcoders[record_params->record_info_id](context, record_params, event_params, record_buffer, record_count);
}
