/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
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

/*
 * Index field contains source identifier, optional exchange code and low-end index (virtual id or MMID).
 * Index field has 2 formats depending on whether source is "special" (see OrderSource.isSpecialSourceId()).
 * Note: both formats are IMPLEMENTATION DETAILS, they are subject to change without notice.
 *   63..48   47..32   31..16   15..0
 * +--------+--------+--------+--------+
 * | Source |Exchange|      Index      |  <- "special" order sources (non-printable id with exchange)
 * +--------+--------+--------+--------+
 *   63..48   47..32   31..16   15..0
 * +--------+--------+--------+--------+
 * |     Source      |      Index      |  <- generic order sources (alphanumeric id without exchange)
 * +--------+--------+--------+--------+
 * Note: when modifying formats track usages of getIndex/setIndex, getSource/setSource and isSpecialSourceId
 * methods in order to find and modify all code dependent on current formats.
 */

static const dxf_long_t DX_ORDER_SOURCE_COMPOSITE_BID = dxf_sos_COMPOSITE_BID;
static const dxf_long_t DX_ORDER_SOURCE_COMPOSITE_ASK = dxf_sos_COMPOSITE_ASK;
static const dxf_long_t DX_ORDER_SOURCE_REGIONAL_BID  = dxf_sos_REGIONAL_BID;
static const dxf_long_t DX_ORDER_SOURCE_REGIONAL_ASK  = dxf_sos_REGIONAL_ASK;
static const dxf_long_t DX_ORDER_SOURCE_AGGREGATE_BID = dxf_sos_AGGREGATE_BID;
static const dxf_long_t DX_ORDER_SOURCE_AGGREGATE_ASK = dxf_sos_AGGREGATE_ASK;

static const dxf_byte_t DX_ORDER_INDEX_SSRC_SHIFT = 48;
static const dxf_byte_t DX_ORDER_INDEX_EXC_SHIFT  = 32;
static const dxf_byte_t DX_ORDER_INDEX_TSRC_SHIFT = 32;

/*
 * Flags property has several significant bits that are packed into an integer in the following way:
 *   31..15   14..11    10..4    3    2    1    0
 * +--------+--------+--------+----+----+----+----+
 * |        | Action |Exchange|  Side   |  Scope  |
 * +--------+--------+--------+----+----+----+----+
 */

static const dxf_byte_t DX_ORDER_FLAGS_SCOPE_SHIFT    = 0;
static const dxf_uint_t DX_ORDER_FLAGS_SCOPE_MASK     = 0x03;
static const dxf_byte_t DX_ORDER_FLAGS_SIDE_SHIFT     = 2;
static const dxf_uint_t DX_ORDER_FLAGS_SIDE_MASK      = 0x03;
static const dxf_byte_t DX_ORDER_FLAGS_EXCHANGE_SHIFT = 4;
static const dxf_uint_t DX_ORDER_FLAGS_EXCHANGE_MASK  = 0x7f;
static const dxf_byte_t DX_ORDER_FLAGS_ACTION_SHIFT = 11;
static const dxf_uint_t DX_ORDER_FLAGS_ACTION_MASK  = 0x0f;


#define DX_ORDER_FROM_QUOTE_BID_INDEX(e) (((e) == 0 ? DX_ORDER_SOURCE_COMPOSITE_BID : DX_ORDER_SOURCE_REGIONAL_BID) << DX_ORDER_INDEX_SSRC_SHIFT) | (char_to_bits(e) << DX_ORDER_INDEX_EXC_SHIFT)
#define DX_ORDER_FROM_QUOTE_ASK_INDEX(e) (((e) == 0 ? DX_ORDER_SOURCE_COMPOSITE_ASK : DX_ORDER_SOURCE_REGIONAL_ASK) << DX_ORDER_INDEX_SSRC_SHIFT) | (char_to_bits(e) << DX_ORDER_INDEX_EXC_SHIFT)

#define DX_ORDER_FORM_MM_BID_INDEX(mm) ((DX_ORDER_SOURCE_AGGREGATE_BID << DX_ORDER_INDEX_SSRC_SHIFT) | (char_to_bits((mm)->mm_exchange) << DX_ORDER_INDEX_EXC_SHIFT) | (int_to_bits((mm)->mm_id)))
#define DX_ORDER_FORM_MM_ASK_INDEX(mm) ((DX_ORDER_SOURCE_AGGREGATE_ASK << DX_ORDER_INDEX_SSRC_SHIFT) | (char_to_bits((mm)->mm_exchange) << DX_ORDER_INDEX_EXC_SHIFT) | (int_to_bits((mm)->mm_id)))

#define DX_ORDER_INDEX(order, suffix) ((suffix) << DX_ORDER_INDEX_TSRC_SHIFT | int_to_bits(order->index))

#define DX_ORDER_GET_SCOPE(rec)    ((dxf_order_scope_t)(((rec)->flags >> DX_ORDER_FLAGS_SCOPE_SHIFT) & DX_ORDER_FLAGS_SCOPE_MASK))
#define DX_ORDER_GET_SIDE(rec)     ((dxf_order_side_t)(((rec)->flags >> DX_ORDER_FLAGS_SIDE_SHIFT) & DX_ORDER_FLAGS_SIDE_MASK))
#define DX_ORDER_GET_EXCHANGE(rec) ((dxf_char_t)(((rec)->flags >> DX_ORDER_FLAGS_EXCHANGE_SHIFT) & DX_ORDER_FLAGS_EXCHANGE_MASK))
#define DX_ORDER_GET_ACTION(rec)   ((dxf_order_action_t)(((rec)->flags >> DX_ORDER_FLAGS_ACTION_SHIFT) & DX_ORDER_FLAGS_ACTION_MASK))

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
//static const dxf_byte_t DX_SERIES_INDEX_TIME_SHIFT = 32;

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

void dx_copy_event_params(const dxf_event_params_t* from, dxf_event_params_t* to) {
	dx_memcpy(to, from, sizeof(dxf_event_params_t));
}

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
	int res = true;
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

typedef int (*dx_record_transcoder_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buffer);

/* -------------------------------------------------------------------------- */
/*
 *	Record transcoders implementation
 */
/* -------------------------------------------------------------------------- */

static int dx_trade_t_transcoder_impl(dx_record_transcoder_connection_context_t* context,
	const dx_record_params_t* record_params,
	const dxf_event_params_t* event_params,
	dx_trade_t* record_buffer, dx_event_id_t event_id)
{
	if (event_id != dx_eid_trade && event_id != dx_eid_trade_eth) {
		return dx_set_error_code(dx_ec_internal_assert_violation);
	}

	dxf_trade_t* event_buffer = (dxf_trade_t*) dx_get_event_data_buffer(context, event_id, 1);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	if (!exchange_code)
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);

	event_buffer->time = DX_TIME_SEQ_TO_MS(record_buffer);
	event_buffer->sequence = DX_SEQUENCE(record_buffer);
	event_buffer->time_nanos = record_buffer->time_nanos;
	event_buffer->exchange_code = record_buffer->exchange_code ? record_buffer->exchange_code : exchange_code;
	event_buffer->price = record_buffer->price;
	event_buffer->size = record_buffer->size;

	if (event_id == dx_eid_trade) {
		event_buffer->tick = record_buffer->tick;
	}

	event_buffer->change = record_buffer->change;
	event_buffer->day_id = record_buffer->day_id;
	event_buffer->day_volume = record_buffer->day_volume;
	event_buffer->day_turnover = record_buffer->day_turnover;
	event_buffer->raw_flags = record_buffer->flags;
	event_buffer->direction = DX_TRADE_GET_DIR(record_buffer);
	event_buffer->is_eth = DX_TRADE_GET_ETH(record_buffer);
	event_buffer->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);

	return dx_process_event_data(context->connection, event_id, record_params->symbol_name, event_buffer,
								 event_params);
}

int RECORD_TRANSCODER_NAME(dx_trade_t) (dx_record_transcoder_connection_context_t* context,
	const dx_record_params_t* record_params,
	const dxf_event_params_t* event_params,
	void* record_buffer)
{
	return dx_trade_t_transcoder_impl(context, record_params, event_params, (dx_trade_t*)record_buffer, dx_eid_trade);
}

/* -------------------------------------------------------------------------- */

int dx_transcode_quote_to_order_bid (dx_record_transcoder_connection_context_t* context,
									const dx_record_params_t* record_params,
									const dxf_event_params_t* event_params,
									dx_quote_t* record_buffer) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, 1);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	event_buffer->index = DX_ORDER_FROM_QUOTE_BID_INDEX(exchange_code);
	event_buffer->time = DX_TIME_FIELD_TO_MS(record_buffer->bid_time);

	if (record_buffer->bid_time > record_buffer->ask_time) {
		event_buffer->time += DX_SEQUENCE_TO_MS(record_buffer);
		event_buffer->time_nanos = record_buffer->time_nanos;
	} else {
		event_buffer->time_nanos = 0;
	}

	event_buffer->sequence = DX_SEQUENCE(record_buffer);
	event_buffer->price = record_buffer->bid_price;
	event_buffer->size = record_buffer->bid_size;
	event_buffer->count = 0;
	event_buffer->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);
	event_buffer->side = dxf_osd_buy;
	event_buffer->exchange_code = (exchange_code == 0 ? record_buffer->bid_exchange_code : exchange_code);
	dx_memset(event_buffer->source, 0, sizeof(event_buffer->source));

	dxf_const_string_t source = dx_all_special_order_sources[exchange_code == 0 ? dxf_sos_COMPOSITE_BID : dxf_sos_REGIONAL_BID];

	dx_copy_string_len(event_buffer->source, source, dx_string_length(source));
	event_buffer->market_maker = NULL;

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name, event_buffer,
								 event_params);
}

/* ---------------------------------- */

int dx_transcode_quote_to_order_ask (dx_record_transcoder_connection_context_t* context,
									const dx_record_params_t* record_params,
									const dxf_event_params_t* event_params,
									dx_quote_t* record_buffer) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, 1);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	event_buffer->event_flags = 0;
	event_buffer->index = DX_ORDER_FROM_QUOTE_ASK_INDEX(exchange_code);
	event_buffer->time = DX_TIME_FIELD_TO_MS(record_buffer->ask_time);

	if (record_buffer->ask_time > record_buffer->bid_time) {
		event_buffer->time += DX_SEQUENCE_TO_MS(record_buffer);
		event_buffer->time_nanos = record_buffer->time_nanos;
	} else {
		event_buffer->time_nanos = 0;
	}

	event_buffer->sequence = DX_SEQUENCE(record_buffer);
	event_buffer->price = record_buffer->ask_price;
	event_buffer->size = record_buffer->ask_size;
	event_buffer->count = 0;
	event_buffer->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);
	event_buffer->side = dxf_osd_sell;
	event_buffer->exchange_code = (exchange_code == 0 ? record_buffer->ask_exchange_code : exchange_code);
	dx_memset(event_buffer->source, 0, sizeof(event_buffer->source));

	dxf_const_string_t source = dx_all_special_order_sources[exchange_code == 0 ? dxf_sos_COMPOSITE_ASK : dxf_sos_REGIONAL_ASK];

	dx_copy_string_len(event_buffer->source, source, dx_string_length(source));
	event_buffer->market_maker = NULL;

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name, event_buffer,
								 event_params);
}

/* ---------------------------------- */

int dx_transcode_quote (dx_record_transcoder_connection_context_t* context,
						const dx_record_params_t* record_params,
						const dxf_event_params_t* event_params,
						dx_quote_t* record_buffer) {
	dxf_quote_t* event_buffer = (dxf_quote_t*)dx_get_event_data_buffer(context, dx_eid_quote, 1);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	if (exchange_code != 0) {
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);
	}

	event_buffer->time = DX_TIME_FIELD_TO_MS(record_buffer->bid_time > record_buffer->ask_time ? record_buffer->bid_time : record_buffer->ask_time) + DX_SEQUENCE_TO_MS(record_buffer);
	event_buffer->sequence = DX_SEQUENCE(record_buffer);
	event_buffer->time_nanos = record_buffer->time_nanos;
	event_buffer->bid_time = DX_TIME_FIELD_TO_MS(record_buffer->bid_time);
	event_buffer->bid_exchange_code = exchange_code ? exchange_code : record_buffer->bid_exchange_code;
	event_buffer->bid_price = record_buffer->bid_price;
	event_buffer->bid_size = record_buffer->bid_size;
	event_buffer->ask_time = DX_TIME_FIELD_TO_MS(record_buffer->ask_time);
	event_buffer->ask_exchange_code = exchange_code ? exchange_code : record_buffer->ask_exchange_code;
	event_buffer->ask_price = record_buffer->ask_price;
	event_buffer->ask_size = record_buffer->ask_size;
	event_buffer->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);

	return dx_process_event_data(context->connection, dx_eid_quote, record_params->symbol_name, event_buffer,
								 event_params);
}

/* ---------------------------------- */

int RECORD_TRANSCODER_NAME(dx_quote_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buffer) {
	dxf_event_params_t bid_event_params;
	dxf_event_params_t ask_event_params;

	dx_copy_event_params(event_params, &bid_event_params);
	dx_copy_event_params(event_params, &ask_event_params);

	if ((event_params->flags & dxf_ef_snapshot_end) == dxf_ef_snapshot_end) {
		bid_event_params.flags = bid_event_params.flags ^ dxf_ef_snapshot_end;
	}

	if ((event_params->flags & dxf_ef_snapshot_begin) == dxf_ef_snapshot_begin) {
		ask_event_params.flags = ask_event_params.flags ^ dxf_ef_snapshot_begin;
	}

	if (!dx_transcode_quote_to_order_bid(context, record_params, &bid_event_params, (dx_quote_t*)record_buffer)) {
		return false;
	}

	if (!dx_transcode_quote_to_order_ask(context, record_params, &ask_event_params, (dx_quote_t*)record_buffer)) {
		return false;
	}

	if (!dx_transcode_quote(context, record_params, event_params, (dx_quote_t*)record_buffer)) {
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_summary_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buff) {

	dxf_summary_t* event_buffer = (dxf_summary_t*)dx_get_event_data_buffer(context, dx_eid_summary, 1);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	if (exchange_code != 0) {
		dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);
	}

	dx_summary_t* record_buffer = (dx_summary_t*)record_buff;

	event_buffer->day_id = record_buffer->day_id;
	event_buffer->day_open_price = record_buffer->day_open_price;
	event_buffer->day_high_price = record_buffer->day_high_price;
	event_buffer->day_low_price = record_buffer->day_low_price;
	event_buffer->day_close_price = record_buffer->day_close_price;
	event_buffer->prev_day_id = record_buffer->prev_day_id;
	event_buffer->prev_day_close_price = record_buffer->prev_day_close_price;
	event_buffer->prev_day_volume = record_buffer->prev_day_volume;
	event_buffer->open_interest = record_buffer->open_interest;
	event_buffer->raw_flags = record_buffer->flags;
	event_buffer->exchange_code = exchange_code;
	event_buffer->day_close_price_type = DX_SUMMARY_GET_DCPT(record_buffer);
	event_buffer->prev_day_close_price_type = DX_SUMMARY_GET_PDCPT(record_buffer);
	event_buffer->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);

	return dx_process_event_data(context->connection, dx_eid_summary, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_profile_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buff) {
	dxf_profile_t* event_buffer = (dxf_profile_t*)dx_get_event_data_buffer(context, dx_eid_profile, 1);

	if (event_buffer == NULL) {
		return false;
	}

	dx_profile_t* record_buffer = (dx_profile_t*)record_buff;

	event_buffer->beta = record_buffer->beta;
	event_buffer->eps = record_buffer->eps;
	event_buffer->div_freq = record_buffer->div_freq;
	event_buffer->exd_div_amount = record_buffer->exd_div_amount;
	event_buffer->exd_div_date = record_buffer->exd_div_date;
	event_buffer->high_52_week_price = record_buffer->high_price_52;
	event_buffer->low_52_week_price = record_buffer->low_price_52;
	event_buffer->shares = record_buffer->shares;
	event_buffer->free_float = record_buffer->free_float;
	event_buffer->high_limit_price = record_buffer->high_limit_price;
	event_buffer->low_limit_price = record_buffer->low_limit_price;
	event_buffer->halt_start_time = DX_TIME_FIELD_TO_MS(record_buffer->halt_start_time);
	event_buffer->halt_end_time = DX_TIME_FIELD_TO_MS(record_buffer->halt_end_time);
	event_buffer->raw_flags = record_buffer->flags;

	if (record_buffer->description != NULL) {
		event_buffer->description = dx_create_string_src(record_buffer->description);
		if (event_buffer->description && !dx_store_string_buffer(context->rbcc, event_buffer->description))
			return false;
	}

	if (record_buffer->status_reason != NULL) {
		event_buffer->status_reason = dx_create_string_src(record_buffer->status_reason);
		if (event_buffer->status_reason && !dx_store_string_buffer(context->rbcc, event_buffer->status_reason))
			return false;
	}

	event_buffer->trading_status = DX_PROFILE_GET_TS(record_buffer);
	event_buffer->ssr = DX_PROFILE_GET_SSR(record_buffer);

	return dx_process_event_data(context->connection, dx_eid_profile, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int dx_transcode_market_maker_to_order_bid (dx_record_transcoder_connection_context_t* context,
											const dx_record_params_t* record_params,
											const dxf_event_params_t* event_params,
											dx_market_maker_t* record_buffer) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, 1);

	if (event_buffer == NULL) {
		return false;
	}

	dxf_char_t exchange_code = (dxf_char_t)record_buffer->mm_exchange;
	dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);

	event_buffer->event_flags = event_params->flags;
	event_buffer->index = DX_ORDER_FORM_MM_BID_INDEX(record_buffer);
	event_buffer->time = DX_TIME_FIELD_TO_MS(record_buffer->mmbid_time);
	event_buffer->scope = dxf_osc_aggregate;
	dx_memset(event_buffer->source, 0, sizeof(event_buffer->source));

	dxf_const_string_t source = dx_all_special_order_sources[dxf_sos_AGGREGATE_BID];

	dx_copy_string_len(event_buffer->source, source, dx_string_length(source));

	if (IS_FLAG_SET(event_buffer->event_flags, dxf_ef_remove_event)) {
		return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name, event_buffer,
									 event_params);
	}

	event_buffer->sequence = 0;
	event_buffer->price = record_buffer->mmbid_price;
	event_buffer->size = record_buffer->mmbid_size;
	event_buffer->count = record_buffer->mmbid_count;
	event_buffer->side = dxf_osd_buy;
	event_buffer->exchange_code = record_buffer->mm_exchange;

	event_buffer->market_maker = dx_decode_from_integer(record_buffer->mm_id);

	if (!IS_FLAG_SET(record_params->flags, dxf_ef_remove_event) &&
		(event_buffer->market_maker == NULL || !dx_store_string_buffer(context->rbcc, event_buffer->market_maker))) {
		return false;
	}

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name, event_buffer,
								 event_params);
}

/* ---------------------------------- */

int dx_transcode_market_maker_to_order_ask (dx_record_transcoder_connection_context_t* context,
											const dx_record_params_t* record_params,
											const dxf_event_params_t* event_params,
											dx_market_maker_t* record_buffer) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, 1);

	if (event_buffer == NULL) {
		return false;
	}

	dxf_char_t exchange_code = (dxf_char_t)record_buffer->mm_exchange;
	dx_set_record_exchange_code(context->dscc, record_params->record_id, exchange_code);
	event_buffer->event_flags = event_params->flags;
	event_buffer->index = DX_ORDER_FORM_MM_ASK_INDEX(record_buffer);
	event_buffer->time = DX_TIME_FIELD_TO_MS(record_buffer->mmask_time);
	event_buffer->scope = dxf_osc_aggregate;
	dx_memset(event_buffer->source, 0, sizeof(event_buffer->source));

	dxf_const_string_t source = dx_all_special_order_sources[dxf_sos_AGGREGATE_ASK];

	dx_copy_string_len(event_buffer->source, source, dx_string_length(source));

	if (IS_FLAG_SET(event_buffer->event_flags, dxf_ef_remove_event)) {
		return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name, event_buffer,
									 event_params);
	}

	event_buffer->sequence = 0;
	event_buffer->price = record_buffer->mmask_price;
	event_buffer->size = record_buffer->mmask_size;
	event_buffer->count = record_buffer->mmask_count;
	event_buffer->side = dxf_osd_sell;
	event_buffer->exchange_code = record_buffer->mm_exchange;
	event_buffer->market_maker = dx_decode_from_integer(record_buffer->mm_id);

	if (!IS_FLAG_SET(record_params->flags, dxf_ef_remove_event) &&
		(event_buffer->market_maker == NULL || !dx_store_string_buffer(context->rbcc, event_buffer->market_maker))) {
		return false;
	}

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name, event_buffer,
								 event_params);
}

/* ---------------------------------- */

int RECORD_TRANSCODER_NAME(dx_market_maker_t) (dx_record_transcoder_connection_context_t* context,
												const dx_record_params_t* record_params,
												const dxf_event_params_t* event_params,
												void* record_buffer) {
	dxf_event_params_t bid_event_params;
	dxf_event_params_t ask_event_params;

	dx_copy_event_params(event_params, &bid_event_params);
	dx_copy_event_params(event_params, &ask_event_params);

	if ((event_params->flags & dxf_ef_snapshot_end) == dxf_ef_snapshot_end) {
		bid_event_params.flags = bid_event_params.flags ^ dxf_ef_snapshot_end;
	}

	if ((event_params->flags & dxf_ef_snapshot_begin) == dxf_ef_snapshot_begin) {
		ask_event_params.flags = ask_event_params.flags ^ dxf_ef_snapshot_begin;
	}

	if (!dx_transcode_market_maker_to_order_bid(context, record_params, &bid_event_params, (dx_market_maker_t*)record_buffer)) {
		return false;
	}

	if (!dx_transcode_market_maker_to_order_ask(context, record_params, &ask_event_params, (dx_market_maker_t*)record_buffer)) {
		return false;
	}

	return true;
}

/* ---------------------------------- */

#define MAX_SUFFIX_LEN_FOR_INDEX 4

dxf_long_t suffix_to_long(dxf_const_string_t suffix)
{
	if (suffix == NULL)
		return 0;
	size_t suffix_length = dx_string_length(suffix);
	if (suffix_length == 0)
		return 0;

	size_t i = 0;
	dxf_long_t ret = 0;
	if (suffix_length > MAX_SUFFIX_LEN_FOR_INDEX)
		i = suffix_length - MAX_SUFFIX_LEN_FOR_INDEX;
	for (; i < suffix_length; i++) {
		dxf_long_t ch = (dxf_long_t)suffix[i] & 0xFF;
		ret = (ret << 8) | ch;
	}
	return ret;
}

int RECORD_TRANSCODER_NAME(dx_order_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buff) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, 1);
	dxf_const_string_t suffix = record_params->suffix;
	dxf_long_t src = suffix_to_long(suffix);

	if (event_buffer == NULL) {
		return false;
	}

	dx_order_t* record_buffer = (dx_order_t*)record_buff;

	dx_memset(event_buffer->source, 0, sizeof(event_buffer->source));
	dx_copy_string_len(event_buffer->source, suffix, dx_string_length(suffix));

	event_buffer->event_flags = event_params->flags;
	event_buffer->index = DX_ORDER_INDEX(record_buffer, src);
	event_buffer->time = DX_TIME_SEQ_TO_MS(record_buffer);
	event_buffer->scope = DX_ORDER_GET_SCOPE(record_buffer);
	event_buffer->exchange_code = DX_ORDER_GET_EXCHANGE(record_buffer);

	if (IS_FLAG_SET(event_buffer->event_flags, dxf_ef_remove_event)) {
		return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name, event_buffer,
									 event_params);
	}

	event_buffer->sequence = DX_SEQUENCE(record_buffer);
	event_buffer->time_nanos = record_buffer->time_nanos;

	event_buffer->action = DX_ORDER_GET_ACTION(record_buffer);
	event_buffer->action_time = record_buffer->action_time;
	event_buffer->order_id = record_buffer->order_id;
	event_buffer->aux_order_id = record_buffer->aux_order_id;

	event_buffer->price = record_buffer->price;
	event_buffer->size = record_buffer->size;
	event_buffer->executed_size = record_buffer->executed_size;
	event_buffer->count = record_buffer->count;

	event_buffer->trade_id = record_buffer->trade_id;
	event_buffer->trade_price = record_buffer->trade_price;
	event_buffer->trade_size = record_buffer->trade_size;

	event_buffer->side = DX_ORDER_GET_SIDE(record_buffer);
	event_buffer->market_maker = dx_decode_from_integer(record_buffer->mmid);

	if (event_buffer->market_maker != NULL && !dx_store_string_buffer(context->rbcc, event_buffer->market_maker)) {
		return false;
	}

	return dx_process_event_data(context->connection, dx_eid_order, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_time_and_sale_t) (dx_record_transcoder_connection_context_t* context,
												const dx_record_params_t* record_params,
												const dxf_event_params_t* event_params,
												void* record_buff) {
	dxf_time_and_sale_t* event_buffer = (dxf_time_and_sale_t*)dx_get_event_data_buffer(context, dx_eid_time_and_sale, 1);
	dxf_char_t exchange_code = DX_EXCHANGE_FROM_RECORD(record_params);

	if (event_buffer == NULL) {
		return false;
	}

	dx_time_and_sale_t* record_buffer = (dx_time_and_sale_t*)record_buff;

	event_buffer->event_flags = event_params->flags;
	event_buffer->index = (int_to_bits(record_buffer->time) << DX_TNS_INDEX_TIME_SHIFT) | int_to_bits(record_buffer->sequence);
	event_buffer->time = DX_TIME_SEQ_TO_MS(record_buffer);

	if (IS_FLAG_SET(event_buffer->event_flags, dxf_ef_remove_event)) {
		return dx_process_event_data(context->connection, dx_eid_time_and_sale, record_params->symbol_name, event_buffer,
									 event_params);
	}

	event_buffer->exchange_code = record_buffer->exchange_code;
	event_buffer->price = record_buffer->price;
	event_buffer->size = record_buffer->size;
	event_buffer->bid_price = record_buffer->bid_price;
	event_buffer->ask_price = record_buffer->ask_price;

	event_buffer->exchange_sale_conditions = dx_decode_from_integer(record_buffer->exchange_sale_conditions);
	if (event_buffer->exchange_sale_conditions && !dx_store_string_buffer(context->rbcc, event_buffer->exchange_sale_conditions)) {
		return false;
	}

	event_buffer->raw_flags = record_buffer->flags;

	if (record_buffer->buyer != NULL) {
		event_buffer->buyer = dx_create_string_src(record_buffer->buyer);
		if (event_buffer->buyer && !dx_store_string_buffer(context->rbcc, event_buffer->buyer)) {
			return false;
		}
	}

	if (record_buffer->seller != NULL) {
		event_buffer->seller = dx_create_string_src(record_buffer->seller);
		if (event_buffer->seller && !dx_store_string_buffer(context->rbcc, event_buffer->seller)) {
			return false;
		}
	}

	event_buffer->side = DX_TNS_GET_SIDE(record_buffer);
	event_buffer->type = DX_TNS_GET_TYPE(record_buffer);
	event_buffer->is_valid_tick = DX_TNS_GET_VALID_TICK(record_buffer);
	event_buffer->is_eth_trade = DX_TNS_GET_ETH(record_buffer);
	event_buffer->trade_through_exempt = DX_TNS_GET_TTE(record_buffer);
	event_buffer->is_spread_leg = DX_TNS_GET_SPREAD_LEG(record_buffer);
	event_buffer->scope = (exchange_code == 0 ? dxf_osc_composite : dxf_osc_regional);

	return dx_process_event_data(context->connection, dx_eid_time_and_sale, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_candle_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buff) {
	dxf_candle_t* event_buffer = (dxf_candle_t*)dx_get_event_data_buffer(context, dx_eid_candle, 1);

	if (event_buffer == NULL) {
		return false;
	}

	dx_candle_t* record_buffer = (dx_candle_t*)record_buff;

	event_buffer->event_flags = event_params->flags;
	event_buffer->index = (int_to_bits(record_buffer->time) << DX_CANDLE_INDEX_TIME_SHIFT) | int_to_bits(record_buffer->sequence);
	event_buffer->time = DX_TIME_SEQ_TO_MS(record_buffer);

	if (IS_FLAG_SET(event_buffer->event_flags, dxf_ef_remove_event)) {
		return dx_process_event_data(context->connection, dx_eid_candle, record_params->symbol_name, event_buffer,
									 event_params);
	}

	event_buffer->sequence = DX_SEQUENCE(record_buffer);
	event_buffer->count = record_buffer->count;
	event_buffer->open = record_buffer->open;
	event_buffer->high = record_buffer->high;
	event_buffer->low = record_buffer->low;
	event_buffer->close = record_buffer->close;
	event_buffer->volume = record_buffer->volume;
	event_buffer->vwap = record_buffer->vwap;
	event_buffer->bid_volume = record_buffer->bid_volume;
	event_buffer->ask_volume = record_buffer->ask_volume;
	event_buffer->imp_volatility = record_buffer->imp_volatility;
	event_buffer->open_interest = record_buffer->open_interest;

	return dx_process_event_data(context->connection, dx_eid_candle, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_trade_eth_t) (dx_record_transcoder_connection_context_t* context,
											const dx_record_params_t* record_params,
											const dxf_event_params_t* event_params,
											void* record_buffer) {
	return dx_trade_t_transcoder_impl(context, record_params, event_params, (dx_trade_t*)record_buffer, dx_eid_trade_eth);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_spread_order_t) (dx_record_transcoder_connection_context_t* context,
												const dx_record_params_t* record_params,
												const dxf_event_params_t* event_params,
												void* record_buff) {
	dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, 1);
	dxf_const_string_t suffix = record_params->suffix;
	dxf_long_t src = suffix_to_long(suffix);

	if (event_buffer == NULL) {
		return false;
	}

	dx_spread_order_t* record_buffer = (dx_spread_order_t*)record_buff;

	dx_memset(event_buffer->source, 0, sizeof(event_buffer->source));
	dx_copy_string_len(event_buffer->source, suffix, dx_string_length(suffix));

	event_buffer->event_flags = event_params->flags;
	event_buffer->index = DX_ORDER_INDEX(record_buffer, src);
	event_buffer->time = DX_TIME_SEQ_TO_MS(record_buffer);

	if (IS_FLAG_SET(event_buffer->event_flags, dxf_ef_remove_event)) {
		return dx_process_event_data(context->connection, dx_eid_spread_order, record_params->symbol_name, event_buffer,
									 event_params);
	}

	event_buffer->sequence = DX_SEQUENCE(record_buffer);
	event_buffer->time_nanos = record_buffer->time_nanos;

	event_buffer->action = DX_ORDER_GET_ACTION(record_buffer);
	event_buffer->action_time = record_buffer->action_time;
	event_buffer->order_id = record_buffer->order_id;
	event_buffer->aux_order_id = record_buffer->aux_order_id;

	event_buffer->price = record_buffer->price;
	event_buffer->size = record_buffer->size;
	event_buffer->executed_size = record_buffer->executed_size;
	event_buffer->count = record_buffer->count;

	event_buffer->trade_id = record_buffer->trade_id;
	event_buffer->trade_price = record_buffer->trade_price;
	event_buffer->trade_size = record_buffer->trade_size;

	event_buffer->exchange_code = DX_ORDER_GET_EXCHANGE(record_buffer);
	event_buffer->side = DX_ORDER_GET_SIDE(record_buffer);
	event_buffer->scope = DX_ORDER_GET_SCOPE(record_buffer);

	if (record_buffer->spread_symbol != NULL) {
		event_buffer->spread_symbol = dx_create_string_src(record_buffer->spread_symbol);
		if (event_buffer->spread_symbol && !dx_store_string_buffer(context->rbcc, event_buffer->spread_symbol))
			return false;
	}

	return dx_process_event_data(context->connection, dx_eid_spread_order, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_greeks_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buff) {
	dxf_greeks_t* event_buffer = (dxf_greeks_t*)dx_get_event_data_buffer(context, dx_eid_greeks, 1);

	if (event_buffer == NULL) {
		return false;
	}

	dx_greeks_t* record_buffer = (dx_greeks_t*)record_buff;

	event_buffer->event_flags = event_params->flags;
	event_buffer->index = (int_to_bits(record_buffer->time) << DX_GREEKS_INDEX_TIME_SHIFT) | int_to_bits(record_buffer->sequence);
	event_buffer->time = DX_TIME_SEQ_TO_MS(record_buffer);

	if (IS_FLAG_SET(event_buffer->event_flags, dxf_ef_remove_event)) {
		return dx_process_event_data(context->connection, dx_eid_greeks, record_params->symbol_name, event_buffer,
									 event_params);
	}

	event_buffer->price = record_buffer->price;
	event_buffer->volatility = record_buffer->volatility;
	event_buffer->delta = record_buffer->delta;
	event_buffer->gamma = record_buffer->gamma;
	event_buffer->theta = record_buffer->theta;
	event_buffer->rho = record_buffer->rho;
	event_buffer->vega = record_buffer->vega;

	return dx_process_event_data(context->connection, dx_eid_greeks, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_theo_price_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buffer) {
	dxf_theo_price_t* event_buffer = (dxf_theo_price_t*)record_buffer;

	event_buffer->time = DX_TIME_FIELD_TO_MS(event_buffer->time);

	return dx_process_event_data(context->connection, dx_eid_theo_price, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_underlying_t) (dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params,
										const dxf_event_params_t* event_params,
										void* record_buff) {
	dxf_underlying_t* event_buffer = (dxf_underlying_t*)dx_get_event_data_buffer(context, dx_eid_underlying, 1);

	if (event_buffer == NULL) {
		return false;
	}

	dx_underlying_t* record_buffer = (dx_underlying_t*)record_buff;

	event_buffer->volatility = record_buffer->volatility;
	event_buffer->front_volatility = record_buffer->front_volatility;
	event_buffer->back_volatility = record_buffer->back_volatility;
	event_buffer->call_volume = record_buffer->call_volume;
	event_buffer->put_volume = record_buffer->put_volume;
	event_buffer->option_volume = isnan(record_buffer->put_volume)
	   ? record_buffer->call_volume
	   : isnan(record_buffer->call_volume) ? record_buffer->put_volume : (record_buffer->put_volume + record_buffer->call_volume);
	event_buffer->put_call_ratio = record_buffer->put_call_ratio;

	return dx_process_event_data(context->connection, dx_eid_underlying, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_series_t)(dx_record_transcoder_connection_context_t* context,
										const dx_record_params_t* record_params, const dxf_event_params_t* event_params,
										void* record_buff) {
	dxf_series_t* event_buffer = (dxf_series_t*)dx_get_event_data_buffer(context, dx_eid_series, 1);

	if (event_buffer == NULL) {
		return false;
	}

	dx_series_t* record_buffer = (dx_series_t*)record_buff;

	event_buffer->event_flags = event_params->flags;
	event_buffer->index = record_buffer->index;
	event_buffer->time = DX_TIME_SEQ_TO_MS(record_buffer);

	if (IS_FLAG_SET(event_buffer->event_flags, dxf_ef_remove_event)) {
		return dx_process_event_data(context->connection, dx_eid_series, record_params->symbol_name, event_buffer,
									 event_params);
	}

	event_buffer->sequence = DX_SEQUENCE(record_buffer);
	event_buffer->expiration = record_buffer->expiration;
	event_buffer->volatility = record_buffer->volatility;
	event_buffer->call_volume = record_buffer->call_volume;
	event_buffer->put_volume = record_buffer->put_volume;
	event_buffer->option_volume = isnan(record_buffer->put_volume)
		? record_buffer->call_volume
		: isnan(record_buffer->call_volume) ? record_buffer->put_volume : record_buffer->put_volume + record_buffer->call_volume;
	event_buffer->put_call_ratio = record_buffer->put_call_ratio;
	event_buffer->forward_price = record_buffer->forward_price;
	event_buffer->dividend = record_buffer->dividend;
	event_buffer->interest = record_buffer->interest;

	return dx_process_event_data(context->connection, dx_eid_series, record_params->symbol_name, event_buffer,
								 event_params);
}

/* -------------------------------------------------------------------------- */

int RECORD_TRANSCODER_NAME(dx_configuration_t) (dx_record_transcoder_connection_context_t* context,
												const dx_record_params_t* record_params,
												const dxf_event_params_t* event_params,
												void* record_buff) {
	dxf_configuration_t* event_buffer = (dxf_configuration_t*)dx_get_event_data_buffer(context, dx_eid_configuration, 1);

	if (event_buffer == NULL) {
		return false;
	}

	dx_configuration_t* record_buffer = (dx_configuration_t*)record_buff;

	event_buffer->version = record_buffer->version;

	if (!dx_configuration_deserialize_string(&(record_buffer->object), &(event_buffer->object)) ||
		!dx_store_string_buffer(context->rbcc, event_buffer->object)) {
		return false;
	}

	return dx_process_event_data(context->connection, dx_eid_configuration, record_params->symbol_name, event_buffer,
								 event_params);
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

int dx_transcode_record_data (dxf_connection_t connection,
							const dx_record_params_t* record_params,
							const dxf_event_params_t* event_params,
							void* record_buffer) {
	dx_record_transcoder_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_record_transcoder, NULL);
	return g_record_transcoders[record_params->record_info_id](context, record_params, event_params, record_buffer);
}
