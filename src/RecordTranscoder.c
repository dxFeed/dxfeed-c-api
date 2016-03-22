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

dxf_event_data_t dx_get_event_data_buffer (dx_record_transcoder_connection_context_t* context,
                                          dx_event_id_t event_id, int count) {
    if (event_id != dx_eid_order) {
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
            
            if ((context->buffer = dx_calloc(count, sizeof(dxf_order_t))) != NULL) {
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
                                        dxf_const_string_t suffix, dxf_const_string_t symbol_name,
										dxf_int_t symbol_cipher, void* record_buffer,
                                        int record_count, dx_record_id_t record_id);
    
/* -------------------------------------------------------------------------- */
/*
 *	Record transcoders implementation
 */
/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_trade_t) (dx_record_transcoder_connection_context_t* context,
                                         dxf_const_string_t suffix,
                                         dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                         void* record_buffer, int record_count, dx_record_id_t record_id) {
    dxf_trade_t* event_buffer = (dxf_trade_t*)record_buffer;
    int i = 0;
    
    for (; i < record_count; ++i) {
        dxf_trade_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (suffix == NULL ? 0 : suffix[0]);
        
        cur_event->time *= 1000L;
        cur_event->exchange_code = exchange_code;
        dx_set_record_exchange_code(record_id, exchange_code);
    }
    
    return dx_process_event_data(context->connection, dx_eid_trade, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* -------------------------------------------------------------------------- */

bool dx_transcode_quote_to_order_bid (dx_record_transcoder_connection_context_t* context,
                                      dxf_const_string_t suffix,
                                      dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                      dx_quote_t* record_buffer, int record_count) {
    
    int i = 0;
    dxf_order_t* event_buffer = NULL;
    
    if ((event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count)) == NULL) {
        return false;
    }
    
    for (; i < record_count; ++i) {
        dx_quote_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (suffix == NULL ? 0 : suffix[0]);
        
        cur_event->time = ((dxf_long_t)(cur_record->bid_time) * 1000L);
        cur_event->price = cur_record->bid_price;
        cur_event->size = cur_record->bid_size;
        //TODO: check this
		cur_event->index = ((exchange_code == 0 ? DX_ORDER_SOURCE_COMPOSITE_BID : DX_ORDER_SOURCE_REGIONAL_BID) << DX_ORDER_SOURCE_ID_SHIFT) | ((dxf_long_t)exchange_code << DX_ORDER_EXCHANGE_SHIFT);
        cur_event->side = DXF_ORDER_SIDE_BUY;
        cur_event->level = (exchange_code == 0 ? DXF_ORDER_LEVEL_COMPOSITE : DXF_ORDER_LEVEL_REGIONAL);
        cur_event->exchange_code = (cur_record->bid_exchange_code == 0 ? exchange_code : cur_record->bid_exchange_code);
        cur_event->market_maker = NULL;
    }
    
    return dx_process_event_data(context->connection, dx_eid_order, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool dx_transcode_quote_to_order_ask (dx_record_transcoder_connection_context_t* context,
                                      dxf_const_string_t suffix,
                                      dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                      dx_quote_t* record_buffer, int record_count) {
    int i = 0;
    dxf_order_t* event_buffer = NULL;

    if ((event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count)) == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_quote_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (suffix == NULL ? 0 : suffix[0]);

        cur_event->time = ((dxf_long_t)(cur_record->ask_time) * 1000L);
        cur_event->price = cur_record->ask_price;
        cur_event->size = cur_record->ask_size;
        //TODO:: check this
		cur_event->index = ((exchange_code == 0 ? DX_ORDER_SOURCE_COMPOSITE_ASK : DX_ORDER_SOURCE_REGIONAL_ASK) << DX_ORDER_SOURCE_ID_SHIFT) | ((dxf_long_t)exchange_code << DX_ORDER_EXCHANGE_SHIFT);
        cur_event->side = DXF_ORDER_SIDE_SELL;
        cur_event->level = (exchange_code == 0 ? DXF_ORDER_LEVEL_COMPOSITE : DXF_ORDER_LEVEL_REGIONAL);
        cur_event->exchange_code = (cur_record->ask_exchange_code == 0 ? exchange_code : cur_record->ask_exchange_code);
        cur_event->market_maker = NULL;
    }

    return dx_process_event_data(context->connection, dx_eid_order, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool dx_transcode_quote (dx_record_transcoder_connection_context_t* context,
                         dxf_const_string_t suffix,
                         dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                         dx_quote_t* record_buffer, int record_count, dx_record_id_t record_id) {
    dxf_quote_t* event_buffer = (dxf_quote_t*)record_buffer;
    int i = 0;
    dxf_char_t exchange_code = (suffix == NULL ? '/0' : suffix[0]);

    for (; i < record_count; ++i) {
        dxf_quote_t* cur_event = event_buffer + i;

        cur_event->bid_time *= 1000L;
		if (cur_event->bid_exchange_code == 0)
            cur_event->bid_exchange_code = exchange_code;
        cur_event->ask_time *= 1000L;
		if (cur_event->ask_exchange_code == 0)
            cur_event->ask_exchange_code = exchange_code;
        //TODO: what code we need to set?
        dx_set_record_exchange_code(record_id, cur_event->bid_exchange_code);
    }

    return dx_process_event_data(context->connection, dx_eid_quote, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_quote_t) (dx_record_transcoder_connection_context_t* context,
                                         dxf_const_string_t suffix,
                                         dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                         void* record_buffer, int record_count, dx_record_id_t record_id) {
    /* note that it's important to call the order transcoders before the quote one,
       because the quote transcoder alters some values right within the same record buffer,
       which would affect the order transcoding if it took place before it. */
    
    if (!dx_transcode_quote_to_order_bid(context, suffix, symbol_name, symbol_cipher, (dx_quote_t*)record_buffer, record_count)) {
        return false;
    }
    
    if (!dx_transcode_quote_to_order_ask(context, suffix, symbol_name, symbol_cipher, (dx_quote_t*)record_buffer, record_count)) {
        return false;
    }
    
    if (!dx_transcode_quote(context, suffix, symbol_name, symbol_cipher, (dx_quote_t*)record_buffer, record_count, record_id)) {
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_fundamental_t) (dx_record_transcoder_connection_context_t* context,
                                               dxf_const_string_t suffix,
                                               dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                               void* record_buffer, int record_count, dx_record_id_t record_id) {
    /* no transcoding actions are required */
    
    return dx_process_event_data(context->connection, dx_eid_summary, symbol_name, symbol_cipher, record_buffer, record_count);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_profile_t) (dx_record_transcoder_connection_context_t* context,
                                           dxf_const_string_t suffix,
                                           dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                           void* record_buffer, int record_count, dx_record_id_t record_id) {
    /* no transcoding actions are required */

    return dx_process_event_data(context->connection, dx_eid_profile, symbol_name, symbol_cipher, record_buffer, record_count);
}

/* -------------------------------------------------------------------------- */

bool dx_transcode_market_maker_to_order_bid (dx_record_transcoder_connection_context_t* context,
                                             dxf_const_string_t suffix,
                                             dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                             dx_market_maker_t* record_buffer, int record_count, dx_record_id_t record_id) {
    int i = 0;
    dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);    

    if (event_buffer == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_market_maker_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (dxf_char_t)cur_record->mm_exchange;
        dx_set_record_exchange_code(record_id, exchange_code);

        cur_event->exchange_code = exchange_code;
        cur_event->market_maker = dx_decode_from_integer(cur_record->mm_id);
        cur_event->time = ((dxf_long_t)cur_record->mmbid_time) * 1000L;
        cur_event->price = cur_record->mmbid_price;
        cur_event->size = cur_record->mmbid_size;
        cur_event->index = (((dxf_long_t)exchange_code << 32) | ((dxf_long_t)cur_record->mm_id) | 0x8200000000000000L);
        //TODO: check this
		cur_event->index = (DX_ORDER_SOURCE_AGGREGATE_BID << DX_ORDER_SOURCE_ID_SHIFT) 
            | ((dxf_long_t)exchange_code << DX_ORDER_EXCHANGE_SHIFT) | ((dxf_long_t)cur_record->mm_id);
        cur_event->side = DXF_ORDER_SIDE_BUY;
        cur_event->level = DXF_ORDER_LEVEL_AGGREGATE;
        
        if (cur_event->market_maker == NULL ||
            !dx_store_string_buffer(context->rbcc, cur_event->market_maker)) {
            
            return false;
        }
    }

    return dx_process_event_data(context->connection, dx_eid_order, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool dx_transcode_market_maker_to_order_ask (dx_record_transcoder_connection_context_t* context,
                                             dxf_const_string_t suffix,
                                             dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                             dx_market_maker_t* record_buffer, int record_count, dx_record_id_t record_id) {
    int i = 0;
    dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);

    if (event_buffer == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_market_maker_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (dxf_char_t)cur_record->mm_exchange;
        //TODO: duplicates?
        dx_set_record_exchange_code(record_id, exchange_code);

        cur_event->exchange_code = exchange_code;
        cur_event->market_maker = dx_decode_from_integer(cur_record->mm_id);
        cur_event->time = ((dxf_long_t)cur_record->mmask_time) * 1000L;
        cur_event->price = cur_record->mmask_price;
        cur_event->size = cur_record->mmask_size;
        //TODO: check this
        cur_event->index = (DX_ORDER_SOURCE_AGGREGATE_ASK << DX_ORDER_SOURCE_ID_SHIFT) 
            | ((dxf_long_t)exchange_code << DX_ORDER_EXCHANGE_SHIFT) | ((dxf_long_t)cur_record->mm_id);
        cur_event->side = DXF_ORDER_SIDE_SELL;
        cur_event->level = DXF_ORDER_LEVEL_AGGREGATE;

        if (cur_event->market_maker == NULL ||
            !dx_store_string_buffer(context->rbcc, cur_event->market_maker)) {

            return false;
        }
    }

    return dx_process_event_data(context->connection, dx_eid_order, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_market_maker_t) (dx_record_transcoder_connection_context_t* context,
                                                dxf_const_string_t suffix,
                                                dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                                void* record_buffer, int record_count, dx_record_id_t record_id) {
    if (!dx_transcode_market_maker_to_order_bid(context, suffix, symbol_name, symbol_cipher, (dx_market_maker_t*)record_buffer, record_count, record_id)) {
        return false;
    }

    if (!dx_transcode_market_maker_to_order_ask(context, suffix, symbol_name, symbol_cipher, (dx_market_maker_t*)record_buffer, record_count, record_id)) {
        return false;
    }
    
    return true;
}

/* ---------------------------------- */

dxf_long_t suffix_to_long(dxf_const_string_t suffix)
{
    int char_index = RECORD_SUFFIX_SIZE;
    int suffix_length = dx_string_length(suffix);
    dxf_long_t ret = 0;
    if (suffix_length < char_index)
        char_index = suffix_length;
    for (; char_index >= 0; char_index--) {
        dxf_long_t ch = (dxf_long_t)suffix[char_index];
        if (char_index == 0)
            ch <<= 8;
        ret |= ch & 0xFF;
    }
    return ret;
}

bool RECORD_TRANSCODER_NAME(dx_order_t) (dx_record_transcoder_connection_context_t* context,
                                         dxf_const_string_t suffix,
                                         dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                         void* record_buffer, int record_count, dx_record_id_t record_id) {
    int i = 0;
    dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(context, dx_eid_order, record_count);

    if (event_buffer == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_order_t* cur_record = (dx_order_t*)record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dxf_char_t exchange_code = (cur_record->flags & DX_RECORD_SUFFIX_MASK) >> DX_RECORD_SUFFIX_IN_FLAG_SHIFT;
        if (exchange_code > 0)
            exchange_code |= DX_RECORD_SIFFIX_HIGH_BITS;
        dx_set_record_exchange_code(record_id, exchange_code);

        /* HARDCODE */
        //cur_event->index = ((((dxf_long_t)'I' << 8 | (dxf_long_t)'S') << 8 | (dxf_long_t)'T') << DX_ORDER_SOURCE_ID_SHIFT) | cur_record->index;
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

        if (cur_event->market_maker != NULL &&
            !dx_store_string_buffer(context->rbcc, cur_event->market_maker)) {

            return false;
        }
    }

    return dx_process_event_data(context->connection, dx_eid_order, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_time_and_sale_t) (dx_record_transcoder_connection_context_t* context,
                                                 dxf_const_string_t suffix,
                                                 dxf_const_string_t symbol_name, dxf_int_t symbol_cipher,
                                                 void* record_buffer, int record_count, dx_record_id_t record_id) {
    dx_time_and_sale_t* event_buffer = (dx_time_and_sale_t*)record_buffer;
    int i = 0;

    for (; i < record_count; ++i) {
        /* 'event_id' and 'type' fields were deliberately used to store the different values,
           so that the structure might be reused without any new buffer allocations */
        
        dx_time_and_sale_t* cur_event = event_buffer + i;
        const dxf_int_t sequence = (dxf_int_t)(cur_event->event_id & 0xFFFFFFFFL);
        const dxf_int_t exchange_sale_conditions = (dxf_int_t)(cur_event->event_id >> 32);
        const dxf_int_t time = (dxf_int_t)cur_event->time;
        const dxf_int_t flags = cur_event->type;

        cur_event->event_id = ((dxf_long_t)time << 32) | ((dxf_long_t)sequence & 0xFFFFFFFFL);
        cur_event->time = ((dxf_long_t)time * 1000L) + (((dxf_long_t)sequence >> 22) & 0x000003FFL);

        cur_event->exchange_sale_conditions = dx_decode_from_integer((((dxf_long_t)flags & 0xFF00L) << 24 ) | exchange_sale_conditions);
        cur_event->is_trade = ((flags & 0x4) != 0);
        
        if (cur_event->exchange_sale_conditions != NULL &&
            !dx_store_string_buffer(context->rbcc, cur_event->exchange_sale_conditions)) {
            
            return false;
        }
        
        switch (flags & 0x3) {
        case 0: cur_event->type = DXF_TIME_AND_SALE_TYPE_NEW; break;
        case 1: cur_event->type = DXF_TIME_AND_SALE_TYPE_CORRECTION; break;
        case 2: cur_event->type = DXF_TIME_AND_SALE_TYPE_CANCEL; break;
        default: return false;
        }
    }

    return dx_process_event_data(context->connection, dx_eid_time_and_sale, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* -------------------------------------------------------------------------- */
/*
 *	Interface functions implementation
 */
/* -------------------------------------------------------------------------- */

static const dx_record_transcoder_t g_record_transcoders[dx_rid_count] = {
    RECORD_TRANSCODER_NAME(dx_trade_t),
    RECORD_TRANSCODER_NAME(dx_quote_t),
    RECORD_TRANSCODER_NAME(dx_fundamental_t),
    RECORD_TRANSCODER_NAME(dx_profile_t),
    RECORD_TRANSCODER_NAME(dx_market_maker_t),
    RECORD_TRANSCODER_NAME(dx_order_t),
    RECORD_TRANSCODER_NAME(dx_time_and_sale_t)
};

/* -------------------------------------------------------------------------- */

bool dx_transcode_record_data (dxf_connection_t connection,
                               dx_record_id_t record_id, dxf_const_string_t suffix, dxf_const_string_t symbol_name,
                               dxf_int_t symbol_cipher, void* record_buffer, int record_count) {
    dx_record_transcoder_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_record_transcoder, NULL);
    const dx_record_item_t* record_info = dx_get_record_by_id(record_id);
    return g_record_transcoders[record_info->info_id](context, suffix, symbol_name, symbol_cipher, record_buffer, record_count, record_id);
}