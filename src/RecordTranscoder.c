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

/* -------------------------------------------------------------------------- */
/*
 *	Order index calculation constants
 */
/* -------------------------------------------------------------------------- */
static const dxf_long_t DX_ORDER_MAX_SOURCE_ID = (1L << 23) - 1;
static const dxf_long_t DX_ORDER_SOURCE_COMPOSITE_BID = 1;
static const dxf_long_t DX_ORDER_SOURCE_COMPOSITE_ASK = 2;
static const dxf_long_t DX_ORDER_SOURCE_REGIONAL_BID = 3;
static const dxf_long_t DX_ORDER_SOURCE_REGIONAL_ASK = 4;
static const dxf_long_t DX_ORDER_SOURCE_AGGREGATE_BID = 5;
static const dxf_long_t DX_ORDER_SOURCE_AGGREGATE_ASK = 6;
static const dxf_int_t DX_ORDER_SOURCE_ID_SHIFT = 40;
static const dxf_int_t DX_ORDER_EXCHANGE_SHIFT = 32;

static const dxf_int_t DX_ORDER_SIDE_SHIFT = 2;
static const dxf_int_t DX_ORDER_SIDE_MASK = 3;
static const dxf_int_t DX_ORDER_SIDE_BUY = 1;
static const dxf_int_t DX_ORDER_SIDE_SELL = 2;

static const dxf_int_t DX_ORDER_SCOPE_MASK = 3;

static const dxf_int_t DX_ORDER_EVENT_FLAGS_SHIFT = 24;
static const dxf_int_t DX_ORDER_MAX_SEQUENCE = (1 << 22) - 1;

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
*	Summary calculation constants
*/
/* -------------------------------------------------------------------------- */

static const dxf_byte_t DX_SUMMARY_PRICE_TYPE_MASK = 3;
static const dxf_byte_t DX_SUMMARY_DAY_CLOSE_PRICE_TYPE_SHIFT = 2;
static const dxf_byte_t DX_SUMMARY_PREV_DAY_CLOSE_PRICE_TYPE_SHIFT = 0;

/* -------------------------------------------------------------------------- */
/*
 *	Record transcoder connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    dxf_order_t* buffer;
    int cur_count;
    
    dxf_connection_t connection;
    void* rbcc;
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
    
    if (context->buffer != NULL) {
        dx_free(context->buffer);
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

dxf_event_data_t dx_get_event_data_buffer(dx_record_transcoder_connection_context_t* context,
                                          dx_event_id_t event_id, int count) {
    int struct_size = 0;
    if (event_id == dx_eid_order)
        struct_size = sizeof(dxf_order_t);
    else if (event_id == dx_eid_spread_order)
        struct_size = sizeof(dxf_spread_order_t);
    else {
        /* these other types don't require separate buffers yet */
        
        dx_set_error_code(dx_ec_internal_assert_violation);
        
        return NULL;
    }
    
    {
        if (context->cur_count < count) {
            if (context->buffer != NULL) {
                dx_free(context->buffer);
            }
            
            context->cur_count = 0;
            
            if ((context->buffer = dx_calloc(count, struct_size)) != NULL) {
                context->cur_count = count;
            }
        }
        
        return context->buffer;
    }
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
#define DX_RESET_RECORD_DATA(type, data_ptr) memset(data_ptr, 0, sizeof(##type))
    
/* -------------------------------------------------------------------------- */
/*
 *	Record transcoders implementation
 */
/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_trade_t) (dx_record_transcoder_connection_context_t* context,
                                         const dx_record_params_t* record_params,
                                         const dxf_event_params_t* event_params,
                                         void* record_buffer, int record_count) {
    dxf_trade_t* event_buffer = (dxf_trade_t*)record_buffer;
    int i = 0;
    dxf_const_string_t suffix = record_params->suffix;
    
    for (; i < record_count; ++i) {
        dxf_trade_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (suffix == NULL ? 0 : suffix[0]);
        
        cur_event->time *= 1000L;
        cur_event->exchange_code = exchange_code;
        dx_set_record_exchange_code(record_params->record_id, exchange_code);
    }
    
    return dx_process_event_data(context->connection, dx_eid_trade, record_params->symbol_name, 
        record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool dx_transcode_quote_to_order_bid (dx_record_transcoder_connection_context_t* context,
                                      const dx_record_params_t* record_params,
                                      const dxf_event_params_t* event_params,
                                      dx_quote_t* record_buffer, int record_count) {
    int i = 0;
    dxf_order_t* event_buffer = NULL;
    dxf_const_string_t suffix = record_params->suffix;
    
    if ((event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count)) == NULL) {
        return false;
    }
    
    for (; i < record_count; ++i) {
        dx_quote_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (suffix == NULL ? 0 : suffix[0]);

        DX_RESET_RECORD_DATA(dxf_order_t, cur_event);
        
        cur_event->time = ((dxf_long_t)(cur_record->bid_time) * 1000L);
        cur_event->price = cur_record->bid_price;
        cur_event->size = cur_record->bid_size;
		cur_event->index = ((exchange_code == 0 ? DX_ORDER_SOURCE_COMPOSITE_BID : DX_ORDER_SOURCE_REGIONAL_BID) << DX_ORDER_SOURCE_ID_SHIFT) | ((dxf_long_t)exchange_code << DX_ORDER_EXCHANGE_SHIFT);
        cur_event->side = DXF_ORDER_SIDE_BUY;
        cur_event->level = (exchange_code == 0 ? DXF_ORDER_LEVEL_COMPOSITE : DXF_ORDER_LEVEL_REGIONAL);
        cur_event->exchange_code = (exchange_code == 0 ? cur_record->bid_exchange_code : exchange_code);
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
    int i = 0;
    dxf_order_t* event_buffer = NULL;
    dxf_const_string_t suffix = record_params->suffix;

    if ((event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count)) == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_quote_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (suffix == NULL ? 0 : suffix[0]);

        DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

        cur_event->time = ((dxf_long_t)(cur_record->ask_time) * 1000L);
        cur_event->price = cur_record->ask_price;
        cur_event->size = cur_record->ask_size;
		cur_event->index = ((exchange_code == 0 ? DX_ORDER_SOURCE_COMPOSITE_ASK : DX_ORDER_SOURCE_REGIONAL_ASK) << DX_ORDER_SOURCE_ID_SHIFT) | ((dxf_long_t)exchange_code << DX_ORDER_EXCHANGE_SHIFT);
        cur_event->side = DXF_ORDER_SIDE_SELL;
        cur_event->level = (exchange_code == 0 ? DXF_ORDER_LEVEL_COMPOSITE : DXF_ORDER_LEVEL_REGIONAL);
        cur_event->exchange_code = (exchange_code == 0 ? cur_record->ask_exchange_code : exchange_code);
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
        dx_set_record_exchange_code(record_params->record_id, cur_event->bid_exchange_code);
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
                                           void* record_buffer, int record_count) {

    dxf_summary_t* event_buffer = (dxf_summary_t*)record_buffer;
    dxf_const_string_t suffix = record_params->suffix;
    dxf_char_t exchange_code = (suffix == NULL ? 0 : suffix[0]);
    int i = 0;

    for (; i < record_count; ++i) {
        dxf_summary_t* cur_event = event_buffer + i;
        cur_event->exchange_code = exchange_code;
        cur_event->day_close_price_type = (cur_event->flags >> DX_SUMMARY_DAY_CLOSE_PRICE_TYPE_SHIFT) & DX_SUMMARY_PRICE_TYPE_MASK;
        cur_event->prev_day_close_price_type = (cur_event->flags >> DX_SUMMARY_PREV_DAY_CLOSE_PRICE_TYPE_SHIFT) & DX_SUMMARY_PRICE_TYPE_MASK;
    }

    return dx_process_event_data(context->connection, dx_eid_summary, record_params->symbol_name,
        record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_profile_t) (dx_record_transcoder_connection_context_t* context,
                                           const dx_record_params_t* record_params,
                                           const dxf_event_params_t* event_params,
                                           void* record_buffer, int record_count) {
    dxf_profile_t* event_buffer = (dxf_profile_t*)record_buffer;
    int i = 0;

    for (; i < record_count; ++i) {
        dxf_profile_t* cur_event = event_buffer + i;

        cur_event->halt_start_time *= 1000L;
        cur_event->halt_end_time *= 1000L;
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
        dx_set_record_exchange_code(record_params->record_id, exchange_code);

        DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

        cur_event->exchange_code = exchange_code;
        cur_event->market_maker = dx_decode_from_integer(cur_record->mm_id);
        cur_event->time = ((dxf_long_t)cur_record->mmbid_time) * 1000L;
        cur_event->price = cur_record->mmbid_price;
        cur_event->size = cur_record->mmbid_size;
		cur_event->index = (DX_ORDER_SOURCE_AGGREGATE_BID << DX_ORDER_SOURCE_ID_SHIFT) 
            | ((dxf_long_t)exchange_code << DX_ORDER_EXCHANGE_SHIFT) | ((dxf_long_t)cur_record->mm_id);
        cur_event->side = DXF_ORDER_SIDE_BUY;
        cur_event->level = DXF_ORDER_LEVEL_AGGREGATE;
        cur_event->count = cur_record->mmbid_count;
        
        /* when we get REMOVE_EVENT flag almost all fields of record is null;
        in this case no fields are checked for null*/
        if (!IS_FLAG_SET(record_params->flags, dxf_ef_remove_event) &&
            (cur_event->market_maker == NULL || !dx_store_string_buffer(context->rbcc, cur_event->market_maker))) {
            
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
        dx_set_record_exchange_code(record_params->record_id, exchange_code);

        DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

        cur_event->exchange_code = exchange_code;
        cur_event->market_maker = dx_decode_from_integer(cur_record->mm_id);
        cur_event->time = ((dxf_long_t)cur_record->mmask_time) * 1000L;
        cur_event->price = cur_record->mmask_price;
        cur_event->size = cur_record->mmask_size;
        cur_event->index = (DX_ORDER_SOURCE_AGGREGATE_ASK << DX_ORDER_SOURCE_ID_SHIFT) 
            | ((dxf_long_t)exchange_code << DX_ORDER_EXCHANGE_SHIFT) | ((dxf_long_t)cur_record->mm_id);
        cur_event->side = DXF_ORDER_SIDE_SELL;
        cur_event->level = DXF_ORDER_LEVEL_AGGREGATE;
        cur_event->count = cur_record->mmask_count;

        /* when we get REMOVE_EVENT flag almost all fields of record is null;
        in this case no fields are checked for null*/
        if (!IS_FLAG_SET(record_params->flags, dxf_ef_remove_event) &&
            (cur_event->market_maker == NULL || !dx_store_string_buffer(context->rbcc, cur_event->market_maker))) {

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

#define MAX_SUFFIX_LEN_FOR_INDEX 3

dxf_long_t suffix_to_long(dxf_const_string_t suffix)
{
    int suffix_length = 0;
    int i = 0;
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
                                         void* record_buffer, int record_count) {
    int i = 0;
    dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);
    dxf_const_string_t suffix = record_params->suffix;

    if (event_buffer == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_order_t* cur_record = (dx_order_t*)record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (cur_record->flags & DX_RECORD_SUFFIX_MASK) >> DX_RECORD_SUFFIX_IN_FLAG_SHIFT;
        if (exchange_code > 0)
            exchange_code |= DX_RECORD_SUFFIX_HIGH_BITS;
        dx_set_record_exchange_code(record_params->record_id, exchange_code);

        DX_RESET_RECORD_DATA(dxf_order_t, cur_event);

        cur_event->index = (suffix_to_long(suffix) << DX_ORDER_SOURCE_ID_SHIFT) | cur_record->index;
        cur_event->side = ((cur_record->flags >> DX_ORDER_SIDE_SHIFT) & DX_ORDER_SIDE_MASK) == DX_ORDER_SIDE_SELL ? DXF_ORDER_SIDE_SELL : DXF_ORDER_SIDE_BUY;
        cur_event->level = cur_record->flags & DX_ORDER_SCOPE_MASK;
        cur_event->time = cur_record->time * 1000LL + ((cur_record->sequence >> 22) & 0x3ff);
        cur_event->exchange_code = exchange_code;
        cur_event->market_maker = dx_decode_from_integer(cur_record->mmid);
        cur_event->price = cur_record->price;
        cur_event->size = cur_record->size;
        dx_memset(cur_event->source, 0, sizeof(cur_event->source));
        dx_copy_string_len(cur_event->source, suffix, dx_string_length(suffix));
        cur_event->count = cur_record->count;
        cur_event->event_flags = (dxf_uint_t)cur_record->flags >> DX_ORDER_EVENT_FLAGS_SHIFT;
        cur_event->time_sequence = ((dxf_long_t)dx_get_seconds_from_time(cur_event->time) << 32) 
            | ((dxf_long_t)dx_get_millis_from_time(cur_event->time) << 22)
            | cur_record->sequence;
        cur_event->sequence = (dxf_int_t)cur_event->time_sequence & DX_ORDER_MAX_SEQUENCE;
        cur_event->scope = cur_record->flags & DX_ORDER_SCOPE_MASK;

        if (cur_event->market_maker != NULL &&
            !dx_store_string_buffer(context->rbcc, cur_event->market_maker)) {

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
    dxf_time_and_sale_t* event_buffer = (dxf_time_and_sale_t*)record_buffer;
    int i = 0;

    for (; i < record_count; ++i) {
        /* 'event_id' and 'type' fields were deliberately used to store the different values,
           so that the structure might be reused without any new buffer allocations */
        
        dxf_time_and_sale_t* cur_event = event_buffer + i;
        const dxf_int_t sequence = (dxf_int_t)(cur_event->event_id & 0xFFFFFFFFL);
        const dxf_int_t exchange_sale_conditions = (dxf_int_t)(cur_event->event_id >> 32);
        const dxf_int_t time = (dxf_int_t)cur_event->time;
        const dxf_int_t flags = cur_event->flags;

        cur_event->event_id = ((dxf_long_t)time << 32) | ((dxf_long_t)sequence & 0xFFFFFFFFL);
        cur_event->time = ((dxf_long_t)time * 1000L) + (((dxf_long_t)sequence >> 22) & 0x000003FFL);

        cur_event->event_flags = (dxf_uint_t)flags >> DX_TIME_AND_SALE_EVENT_FLAGS_SHIFT;
        cur_event->sequence = sequence;
        cur_event->exchange_sale_conditions = dx_decode_from_integer((((dxf_long_t)flags & 0xFF00L) << 24 ) | exchange_sale_conditions);
        cur_event->index = cur_event->event_id;
        cur_event->side = ((flags >> DX_TIME_AND_SALE_SIDE_SHIFT) & DX_TIME_AND_SALE_SIDE_MASK) == DX_ORDER_SIDE_SELL ? DXF_ORDER_SIDE_SELL : DXF_ORDER_SIDE_BUY;
        cur_event->is_spread_leg = ((flags & DX_TIME_AND_SALE_SPREAD_LEG) != 0);
        cur_event->is_trade = ((flags & DX_TIME_AND_SALE_ETH) != 0);
        cur_event->is_valid_tick = ((flags & DX_TIME_AND_SALE_VALID_TICK) != 0);
        
        /* when we get REMOVE_EVENT flag almost all fields of record is null;
           in this case no fileds are checked for null*/
        if (!IS_FLAG_SET(event_params->flags, dxf_ef_remove_event)) {
            if (cur_event->exchange_sale_conditions != NULL &&
                !dx_store_string_buffer(context->rbcc, cur_event->exchange_sale_conditions)) {

                return false;
            }

            switch (flags & DX_TIME_AND_SALE_TYPE_MASK) {
            case 0: cur_event->type = DXF_TIME_AND_SALE_TYPE_NEW; break;
            case 1: cur_event->type = DXF_TIME_AND_SALE_TYPE_CORRECTION; break;
            case 2: cur_event->type = DXF_TIME_AND_SALE_TYPE_CANCEL; break;
            default: return false;
            }
        }

        cur_event->is_cancel = (cur_event->type == DXF_TIME_AND_SALE_TYPE_CANCEL);
        cur_event->is_correction = (cur_event->type == DXF_TIME_AND_SALE_TYPE_CORRECTION);
        cur_event->is_new = (cur_event->type == DXF_TIME_AND_SALE_TYPE_NEW);
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
        dx_set_record_exchange_code(record_params->record_id, exchange_code);
    }

    return dx_process_event_data(context->connection, dx_eid_trade_eth, record_params->symbol_name,
        record_params->symbol_cipher, event_buffer, record_count, event_params);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_spread_order_t) (dx_record_transcoder_connection_context_t* context,
                                                const dx_record_params_t* record_params,
                                                const dxf_event_params_t* event_params,
                                                void* record_buffer, int record_count) {
    int i = 0;
    dxf_spread_order_t* event_buffer = (dxf_spread_order_t*)dx_get_event_data_buffer(context, dx_eid_spread_order, record_count);
    dxf_const_string_t suffix = record_params->suffix;

    if (event_buffer == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_spread_order_t* cur_record = (dx_spread_order_t*)record_buffer + i;
        dxf_spread_order_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (cur_record->flags & DX_RECORD_SUFFIX_MASK) >> DX_RECORD_SUFFIX_IN_FLAG_SHIFT;
        if (exchange_code > 0)
            exchange_code |= DX_RECORD_SUFFIX_HIGH_BITS;
        dx_set_record_exchange_code(record_params->record_id, exchange_code);

        DX_RESET_RECORD_DATA(dxf_spread_order_t, cur_event);

        cur_event->index = (suffix_to_long(suffix) << DX_ORDER_SOURCE_ID_SHIFT) | cur_record->index;
        cur_event->side = ((cur_record->flags >> DX_ORDER_SIDE_SHIFT) & DX_ORDER_SIDE_MASK) == DX_ORDER_SIDE_SELL ? DXF_ORDER_SIDE_SELL : DXF_ORDER_SIDE_BUY;
        cur_event->level = cur_record->flags & DX_ORDER_SCOPE_MASK;
        cur_event->time = cur_record->time * 1000LL + ((cur_record->sequence >> 22) & 0x3ff);
        cur_event->sequence = cur_record->sequence;
        cur_event->exchange_code = exchange_code;
        cur_event->price = cur_record->price;
        cur_event->size = cur_record->size;
        dx_memset(cur_event->source, 0, sizeof(cur_event->source));
        dx_copy_string_len(cur_event->source, suffix, dx_string_length(suffix));
        cur_event->count = cur_record->count;
        cur_event->event_flags = (dxf_uint_t)cur_record->flags >> DX_ORDER_EVENT_FLAGS_SHIFT;
        cur_event->time_sequence = ((dxf_long_t)dx_get_seconds_from_time(cur_event->time) << 32)
            | ((dxf_long_t)dx_get_millis_from_time(cur_event->time) << 22)
            | cur_record->sequence;
        cur_event->sequence = (dxf_int_t)cur_event->time_sequence & DX_ORDER_MAX_SEQUENCE;
        cur_event->scope = cur_record->flags & DX_ORDER_SCOPE_MASK;
        if (cur_record->spread_symbol != NULL) {
            cur_event->spread_symbol = dx_create_string_src(cur_record->spread_symbol);
            if (!dx_store_string_buffer(context->rbcc, cur_event->spread_symbol))
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
    }

    return dx_process_event_data(context->connection, dx_eid_series, record_params->symbol_name,
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
    RECORD_TRANSCODER_NAME(dx_series_t)
};

/* -------------------------------------------------------------------------- */

bool dx_transcode_record_data (dxf_connection_t connection, 
                               const dx_record_params_t* record_params, 
                               const dxf_event_params_t* event_params,
                               void* record_buffer, int record_count) {
    dx_record_transcoder_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_record_transcoder, NULL);
    return g_record_transcoders[record_params->record_info_id](context, record_params, event_params, record_buffer, record_count);
}