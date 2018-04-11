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
 *	Trade flags constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_uint_t DX_TRADE_FLAGS_ETH       = 1;
static const dxf_byte_t DX_TRADE_FLAGS_DIR_SHIFT = 1;
static const dxf_uint_t DX_TRADE_FLAGS_DIR_MASK  = 0x7;

/* -------------------------------------------------------------------------- */
/*
 *	Summary flags constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_byte_t DX_SUMMARY_FLAGS_DCPT_SHIFT  = 2;
static const dxf_byte_t DX_SUMMARY_FLAGS_PDCPT_SHIFT = 0;
static const dxf_uint_t DX_SUMMARY_FLAGS_CPT_MASK    = 0x3;

/* -------------------------------------------------------------------------- */
/*
 *	Profile flags constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_byte_t DX_PROFILE_FLAGS_TS_SHIFT  = 0;
static const dxf_uint_t DX_PROFILE_FLAGS_TS_MASK   = 0x3;
static const dxf_byte_t DX_PROFILE_FLAGS_SSR_SHIFT = 2;
static const dxf_uint_t DX_PROFILE_FLAGS_SSR_MASK  = 0x3;

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

#define DX_ORDER_FROM_QUOTE_BID_INDEX(e) (((e) == 0 ? DX_ORDER_SOURCE_COMPOSITE_BID : DX_ORDER_SOURCE_REGIONAL_BID) << DX_ORDER_INDEX_SSRC_SHIFT) | ((dxf_long_t)(e) << DX_ORDER_INDEX_EXC_SHIFT)
#define DX_ORDER_FROM_QUOTE_ASK_INDEX(e) (((e) == 0 ? DX_ORDER_SOURCE_COMPOSITE_ASK : DX_ORDER_SOURCE_REGIONAL_ASK) << DX_ORDER_INDEX_SSRC_SHIFT) | ((dxf_long_t)(e) << DX_ORDER_INDEX_EXC_SHIFT)

#define DX_ORDER_FORM_MM_BID_INDEX(mm) ((DX_ORDER_SOURCE_AGGREGATE_BID << DX_ORDER_INDEX_SSRC_SHIFT) | ((dxf_long_t)(mm)->mm_exchange << DX_ORDER_INDEX_EXC_SHIFT) | ((mm)->mm_id))
#define DX_ORDER_FORM_MM_ASK_INDEX(mm) ((DX_ORDER_SOURCE_AGGREGATE_ASK << DX_ORDER_INDEX_SSRC_SHIFT) | ((dxf_long_t)(mm)->mm_exchange << DX_ORDER_INDEX_EXC_SHIFT) | ((mm)->mm_id))

#define DX_ORDER_INDEX(order, suffix) ((suffix) << DX_ORDER_INDEX_TSRC_SHIFT | order->index)

#define DX_ORDER_GET_SCOPE(order)    ((dxf_order_scope_t)(((order)->flags >> DX_ORDER_FLAGS_SCOPE_SHIFT) & DX_ORDER_FLAGS_SCOPE_MASK))
#define DX_ORDER_GET_SIDE(order)     ((dxf_order_side_t)(((order)->flags >> DX_ORDER_FLAGS_SIDE_SHIFT) & DX_ORDER_FLAGS_SIDE_MASK))
#define DX_ORDER_GET_EXCHANGE(order) ((dxf_char_t)(((order)->flags >> DX_ORDER_FLAGS_EXCHANGE_SHIFT) & DX_ORDER_FLAGS_EXCHANGE_MASK))

/* -------------------------------------------------------------------------- */
/*
 *	TimeAndSale calculation constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_int_t DX_TIME_AND_SALE_EVENT_FLAGS_SHIFT = 24;
static const dxf_int_t DX_TIME_AND_SALE_SIDE_MASK = 3;
static const dxf_int_t DX_TIME_AND_SALE_SIDE_SHIFT = 5;
static const dxf_int_t DX_TIME_AND_SALE_SPREAD_LEG = 1 << 4;
static const dxf_int_t DX_TIME_AND_SALE_ETH = 1 << 3;
static const dxf_int_t DX_TIME_AND_SALE_VALID_TICK = 1 << 2;
static const dxf_int_t DX_TIME_AND_SALE_TYPE_MASK = 3;

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
	sizeof(dxf_trade_eth_t),
	sizeof(dxf_order_t),
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

// Fill struct data with zero's.
#define DX_RESET_RECORD_DATA(type, data_ptr) dx_memset(data_ptr, 0, sizeof(type))

/* -------------------------------------------------------------------------- */
/*
 *	Record transcoders implementation
 */
/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_trade_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_trade_t* record_buffer, int record_count) {
	dxf_trade_t* event_buffer = NULL;
	dxf_char_t exchange_code = (record_params->suffix == NULL ? 0 : record_params->suffix[0]);

	if ((event_buffer = (dxf_trade_t*)dx_get_event_data_buffer(context, dx_eid_trade, record_count)) == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_trade_t* cur_record = record_buffer + i;
		dxf_trade_t* cur_event = event_buffer + i;
		
		cur_event->time = cur_record->time * 1000L + ((cur_record->sequence >> 22) & 0x3FF);
		cur_event->sequence = cur_record->sequence & 0x3FFFFFU;
		cur_event->time_nanos = cur_record->time_nanos;
		cur_event->exchange_code = cur_record->exchange_code ? cur_record->exchange_code : exchange_code;
		cur_event->price = cur_record->price;
		cur_event->size = cur_record->size;
		cur_event->tick = cur_record->tick;
		cur_event->change = cur_record->change;
		cur_event->raw_flags = cur_record->flags;
		cur_event->day_volume = cur_record->day_volume;
		cur_event->day_turnover = cur_record->day_turnover;
		cur_event->direction = (dxf_direction_t)((cur_record->flags >> DX_TRADE_FLAGS_DIR_SHIFT) & DX_TRADE_FLAGS_DIR_MASK);
		cur_event->is_eth = (cur_record->flags & DX_TRADE_FLAGS_ETH) == DX_TRADE_FLAGS_ETH;

		if (!cur_record->exchange_code)
			dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);
	}

	return dx_process_event_data(context->connection, dx_eid_trade, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool dx_transcode_quote_to_order_bid (dx_record_transcoder_connection_context_t* context,
									const dx_record_params_t* record_params,
									const dxf_event_params_t* event_params,
									dx_quote_t* record_buffer, int record_count) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);
	dxf_char_t exchange_code = (record_params->suffix == NULL ? 0 : record_params->suffix[0]);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_quote_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;

		DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

		cur_event->event_flags = 0;
		cur_event->index = DX_ORDER_FROM_QUOTE_BID_INDEX(exchange_code);
		cur_event->time = cur_record->bid_time * 1000L;
		if (cur_record->bid_time > cur_record->ask_time) {
			cur_event->time += ((cur_record->sequence >> 22) & 0x3FF);
			cur_event->time_nanos = cur_record->time_nanos;
 		} else {
			cur_event->time_nanos = 0;
 		}
		cur_event->sequence = cur_record->sequence & 0x3FFFFFU;
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
	dxf_char_t exchange_code = (record_params->suffix == NULL ? 0 : record_params->suffix[0]);

	if (event_buffer == NULL) {
		return false;
	}

	for (int i = 0; i < record_count; ++i) {
		dx_quote_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;

		DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

		cur_event->event_flags = 0;
		cur_event->index = DX_ORDER_FROM_QUOTE_ASK_INDEX(exchange_code);
		cur_event->time = cur_record->ask_time * 1000L;
		if (cur_record->ask_time > cur_record->bid_time) {
			cur_event->time += ((cur_record->sequence >> 22) & 0x3FF);
			cur_event->time_nanos = cur_record->time_nanos;
 		} else {
			cur_event->time_nanos = 0;
 		}
		cur_event->sequence = cur_record->sequence & 0x3FFFFFU;
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
	dxf_quote_t* event_buffer = (dxf_quote_t*)record_buffer;
	int i = 0;
	dxf_const_string_t suffix = record_params->suffix;
	dxf_char_t exchange_code = (suffix == NULL ? 0 : suffix[0]);

	for (; i < record_count; ++i) {
		dxf_quote_t* cur_event = event_buffer + i;

		cur_event->bid_time *= 1000L;
		if (exchange_code != 0)
			cur_event->bid_exchange_code = exchange_code;
		cur_event->ask_time *= 1000L;
		if (exchange_code != 0)
			cur_event->ask_exchange_code = exchange_code;

		cur_event->time = (cur_event->bid_time > cur_event->ask_time ? cur_event->bid_time : cur_event->ask_time) + ((cur_event->sequence >> 22) & 0x3FF);
		cur_event->sequence &= 0x3FFFFFU;

		dx_set_record_exchange_code(context->dscc, record_params->record_id, cur_event->bid_exchange_code);
	}

	return dx_process_event_data(context->connection, dx_eid_quote, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* ---------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_quote_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buffer, int record_count) {
	/* note that it's important to call the order transcoders before the quote one,
	because the quote transcoder alters some values right within the same record buffer,
	which would affect the order transcoding if it took place before it. */

	if (!dx_transcode_quote_to_order_bid(context, record_params, event_params, (dx_quote_t*)record_buffer,
		record_count)) {

		return false;
	}

	if (!dx_transcode_quote_to_order_ask(context, record_params, event_params, (dx_quote_t*)record_buffer,
		record_count)) {

		return false;
	}

	if (!dx_transcode_quote(context, record_params, event_params, (dx_quote_t*)record_buffer, record_count)) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_summary_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_summary_t* record_buffer, int record_count) {

	dxf_summary_t* event_buffer = NULL;
	dxf_char_t exchange_code = (record_params->suffix == NULL ? 0 : record_params->suffix[0]);

	if ((event_buffer = (dxf_summary_t*)dx_get_event_data_buffer(context, dx_eid_summary, record_count)) == NULL) {
		return false;
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
		cur_event->open_interest = cur_record->open_interest;
		cur_event->raw_flags = cur_record->flags;
		cur_event->exchange_code = exchange_code;
		cur_event->day_close_price_type = (dxf_price_type_t)(((cur_record)->flags >> DX_SUMMARY_FLAGS_DCPT_SHIFT)  & DX_SUMMARY_FLAGS_CPT_MASK);
		cur_event->prev_day_close_price_type = (dxf_price_type_t)(((cur_record)->flags >> DX_SUMMARY_FLAGS_PDCPT_SHIFT) & DX_SUMMARY_FLAGS_CPT_MASK);
	}

	return dx_process_event_data(context->connection, dx_eid_summary, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_profile_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										dx_profile_t* record_buffer, int record_count) {
	dxf_profile_t* event_buffer = NULL;

	if ((event_buffer = (dxf_profile_t*)dx_get_event_data_buffer(context, dx_eid_profile, record_count)) == NULL) {
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
		cur_event->halt_start_time = cur_record->halt_start_time * 1000L;
		cur_event->halt_end_time = cur_record->halt_end_time * 1000L;
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

		cur_event->trading_status = (dxf_trading_status_t)((cur_record->flags >> DX_PROFILE_FLAGS_TS_SHIFT) & DX_PROFILE_FLAGS_TS_MASK);
		cur_event->ssr = (dxf_short_sale_restriction_t)((cur_record->flags >> DX_PROFILE_FLAGS_SSR_SHIFT) & DX_PROFILE_FLAGS_SSR_MASK);
	}

	return dx_process_event_data(context->connection, dx_eid_profile, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool dx_transcode_market_maker_to_order_bid (dx_record_transcoder_connection_context_t* context,
											const dx_record_params_t* record_params,
											const dxf_event_params_t* event_params,
											dx_market_maker_t* record_buffer, int record_count) {
	int i = 0;
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (; i < record_count; ++i) {
		dx_market_maker_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;
		dxf_char_t exchange_code = (dxf_char_t)cur_record->mm_exchange;
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);

		DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

		cur_event->event_flags = event_params->flags;
		cur_event->index = DX_ORDER_FORM_MM_BID_INDEX(cur_record);
		cur_event->time = cur_record->mmbid_time * 1000L;
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
	int i = 0;
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (; i < record_count; ++i) {
		dx_market_maker_t* cur_record = record_buffer + i;
		dxf_order_t* cur_event = event_buffer + i;
		dxf_char_t exchange_code = (dxf_char_t)cur_record->mm_exchange;
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);

		DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

		cur_event->event_flags = event_params->flags;
		cur_event->index = DX_ORDER_FORM_MM_ASK_INDEX(cur_record);
		cur_event->time = cur_record->mmask_time * 1000L;
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
												void* record_buffer, int record_count) {
	if (!dx_transcode_market_maker_to_order_bid(context, record_params, event_params,
		(dx_market_maker_t*)record_buffer, record_count)) {

		return false;
	}

	if (!dx_transcode_market_maker_to_order_ask(context, record_params, event_params,
		(dx_market_maker_t*)record_buffer, record_count)) {

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
		dxf_char_t exchange_code = DX_ORDER_GET_EXCHANGE(cur_record);

		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);

		DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

		cur_event->event_flags = event_params->flags;
		cur_event->index = DX_ORDER_INDEX(cur_record, src);
		cur_event->time = cur_record->time * 1000L + ((cur_record->sequence >> 22) & 0x3FF);
		cur_event->time_nanos = cur_record->time_nanos;
		cur_event->sequence = cur_record->sequence & 0x3FFFFFU;
		cur_event->price = cur_record->price;
		cur_event->size = cur_record->size;
		cur_event->count = cur_record->count;
		cur_event->scope = DX_ORDER_GET_SCOPE(cur_record);
		cur_event->side = DX_ORDER_GET_SIDE(cur_record);
		cur_event->exchange_code = exchange_code;

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
												void* record_buffer, int record_count) {
	int i = 0;
	dxf_time_and_sale_t* event_buffer = (dxf_time_and_sale_t*)dx_get_event_data_buffer(context, dx_eid_time_and_sale, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (; i < record_count; ++i) {
		dx_time_and_sale_t* cur_record = (dx_time_and_sale_t*)record_buffer + i;
		dxf_time_and_sale_t* cur_event = event_buffer + i;

		dxf_int_t flags = cur_record->flags;

		cur_event->event_flags = event_params->flags;
		cur_event->time = cur_record->time * 1000L;
		cur_event->sequence = cur_record->sequence;
		cur_event->exchange_code = cur_record->exchange_code;
		cur_event->price = cur_record->price;
		cur_event->size = cur_record->size;
		cur_event->bid_price = cur_record->bid_price;
		cur_event->ask_price = cur_record->ask_price;
		cur_event->exchange_sale_conditions = dx_decode_from_integer(cur_record->exchange_sale_conditions);
		cur_event->flags = cur_record->flags;
		cur_event->buyer = cur_record->buyer;
		cur_event->seller = cur_record->seller;

		cur_event->index = ((dxf_long_t)cur_record->time) << 32 | (cur_record->sequence & 0xFFFFFFFF);
		cur_event->side = 0; /* ((flags >> DX_TIME_AND_SALE_SIDE_SHIFT) & DX_TIME_AND_SALE_SIDE_MASK) == DX_ORDER_SIDE_SELL ? DXF_ORDER_SIDE_SELL : DXF_ORDER_SIDE_BUY;*/;
		cur_event->is_eth_trade = ((flags & DX_TIME_AND_SALE_ETH) != 0);
		cur_event->is_spread_leg = ((flags & DX_TIME_AND_SALE_SPREAD_LEG) != 0);
		cur_event->is_valid_tick = ((flags & DX_TIME_AND_SALE_VALID_TICK) != 0);

		switch (flags & DX_TIME_AND_SALE_TYPE_MASK) {
		case 0: cur_event->type = DXF_TIME_AND_SALE_TYPE_NEW; break;
		case 1: cur_event->type = DXF_TIME_AND_SALE_TYPE_CORRECTION; break;
		case 2: cur_event->type = DXF_TIME_AND_SALE_TYPE_CANCEL; break;
		default: return false;
		}
		cur_event->is_cancel = (cur_event->type == DXF_TIME_AND_SALE_TYPE_CANCEL);
		cur_event->is_correction = (cur_event->type == DXF_TIME_AND_SALE_TYPE_CORRECTION);
		cur_event->is_new = (cur_event->type == DXF_TIME_AND_SALE_TYPE_NEW);

		/* when we get REMOVE_EVENT flag almost all fields of record is null;
		in this case no fileds are checked for null*/
		if (!IS_FLAG_SET(event_params->flags, dxf_ef_remove_event)) {
			if (cur_event->exchange_sale_conditions != NULL && !dx_store_string_buffer(context->rbcc, cur_event->exchange_sale_conditions)) {
				return false;
			}
		}

	}

	return dx_process_event_data(context->connection, dx_eid_time_and_sale, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_candle_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buffer, int record_count) {
	dxf_candle_t* event_buffer = (dxf_candle_t*)record_buffer;
	int i = 0;

	for (; i < record_count; ++i) {
		dxf_candle_t* cur_event = event_buffer + i;
		cur_event->time *= DX_TIME_SECOND;
		cur_event->index = ((dxf_long_t)dx_get_seconds_from_time(cur_event->time) << 32)
			| ((dxf_long_t)dx_get_millis_from_time(cur_event->time) << 22)
			| cur_event->sequence;
		cur_event->event_flags = event_params->flags;
	}

	return dx_process_event_data(context->connection, dx_eid_candle, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_trade_eth_t) (dx_record_transcoder_connection_context_t* context,
											const dx_record_params_t* record_params,
											const dxf_event_params_t* event_params,
											void* record_buffer, int record_count) {
	dxf_trade_eth_t* event_buffer = (dxf_trade_eth_t*)record_buffer;
	dxf_const_string_t suffix = record_params->suffix;
	int i = 0;

	for (; i < record_count; ++i) {
		dxf_trade_eth_t* cur_event = event_buffer + i;
		dxf_char_t exchange_code = (suffix == NULL ? 0 : suffix[0]);
		cur_event->time *= 1000L;
		cur_event->exchange_code = exchange_code;
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);
	}

	return dx_process_event_data(context->connection, dx_eid_trade_eth, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
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
		dxf_char_t exchange_code = DX_ORDER_GET_EXCHANGE(cur_record);

		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);

		DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

		cur_event->event_flags = event_params->flags;
		cur_event->index = DX_ORDER_INDEX(cur_record, src);
		cur_event->time = cur_record->time * 1000L + ((cur_record->sequence >> 22) & 0x3FF);
		cur_event->time_nanos = cur_record->time_nanos;
		cur_event->sequence = cur_record->sequence & 0x3FFFFFU;
		cur_event->price = cur_record->price;
		cur_event->size = cur_record->size;
		cur_event->count = cur_record->count;
		cur_event->scope = DX_ORDER_GET_SCOPE(cur_record);
		cur_event->side = DX_ORDER_GET_SIDE(cur_record);
		cur_event->exchange_code = exchange_code;

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
										void* record_buffer, int record_count) {
	dxf_greeks_t* event_buffer = (dxf_greeks_t*)record_buffer;
	int i = 0;

	for (; i < record_count; ++i) {
		dxf_greeks_t* cur_event = event_buffer + i;
		cur_event->time *= DX_TIME_SECOND;
		cur_event->index = ((dxf_long_t)dx_get_seconds_from_time(cur_event->time) << 32)
			| ((dxf_long_t)dx_get_millis_from_time(cur_event->time) << 22)
			| cur_event->sequence;
		cur_event->event_flags = event_params->flags;
	}

	return dx_process_event_data(context->connection, dx_eid_greeks, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_theo_price_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buffer, int record_count) {
	dxf_theo_price_t* event_buffer = (dxf_theo_price_t*)record_buffer;
	int i = 0;

	for (; i < record_count; ++i) {
		dxf_theo_price_t* cur_event = event_buffer + i;
		cur_event->theo_time *= 1000L;
	}

	return dx_process_event_data(context->connection, dx_eid_theo_price, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_underlying_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buffer, int record_count) {
	dxf_underlying_t* event_buffer = (dxf_underlying_t*)record_buffer;

	return dx_process_event_data(context->connection, dx_eid_underlying, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_series_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buffer, int record_count) {
	dxf_series_t* event_buffer = (dxf_series_t*)record_buffer;
	int i = 0;

	for (; i < record_count; ++i) {
		dxf_series_t* cur_event = event_buffer + i;
		cur_event->index = (dxf_long_t)cur_event->expiration << 32 | cur_event->sequence;
		cur_event->event_flags = event_params->flags;
	}

	return dx_process_event_data(context->connection, dx_eid_series, record_params->symbol_name,
		record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_configuration_t) (dx_record_transcoder_connection_context_t* context,
												const dx_record_params_t* record_params,
												const dxf_event_params_t* event_params,
												void* record_buffer, int record_count) {
	int i = 0;
	dxf_configuration_t* event_buffer = (dxf_configuration_t*)dx_get_event_data_buffer(context, dx_eid_configuration, record_count);

	if (event_buffer == NULL) {
		return false;
	}

	for (; i < record_count; ++i) {
		dx_configuration_t* cur_record = (dx_configuration_t*)record_buffer + i;
		dxf_configuration_t* cur_event = event_buffer + i;

		DX_RESET_RECORD_DATA(dxf_configuration_t, cur_event);

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