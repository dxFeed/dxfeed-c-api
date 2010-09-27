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
 
#include "RecordTranscoder.h"
#include "EventData.h"
#include "DXMemory.h"
#include "EventSubscription.h"
#include "DataStructures.h"
#include "DXAlgorithms.h"
#include "RecordBuffers.h"

/* -------------------------------------------------------------------------- */
/*
 *	Event data buffer functions
 
 *  some transcoders require separate data structures to be allocated and filled
 *  based on the record data they receive
 */
/* -------------------------------------------------------------------------- */

dx_event_data_t dx_get_event_data_buffer (dx_event_id_t event_id, int count) {
    if (event_id != dx_eid_order) {
        /* these other types don't require separate buffers yet */
        
        return NULL;
    }
    
    {
        static dxf_order_t* buffer = NULL;
        static int cur_count = 0;
        
        if (cur_count < count) {
            if (buffer != NULL) {
                dx_free(buffer);
            }
            
            cur_count = 0;
            
            if ((buffer = dx_calloc(count, sizeof(dxf_order_t))) != NULL) {
                cur_count = count;
            }
        }
        
        return buffer;
    }
}

/* -------------------------------------------------------------------------- */
/*
 *	Record transcoder macros and prototypes
 */
/* -------------------------------------------------------------------------- */

#define RECORD_TRANSCODER_NAME(struct_name) \
    struct_name##_transcoder
    
typedef bool (*dx_record_transcoder_t) (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                        void* record_buffer, int record_count);
    
/* -------------------------------------------------------------------------- */
/*
 *	Record transcoders implementation
 */
/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_trade_t) (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                         void* record_buffer, int record_count) {
    dxf_trade_t* event_buffer = (dxf_trade_t*)record_buffer;
    int i = 0;
    
    for (; i < record_count; ++i) {
        dxf_trade_t* cur_event = event_buffer + i;
        
        cur_event->time *= 1000L;
    }
    
    return dx_process_event_data(dx_eid_trade, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* -------------------------------------------------------------------------- */

bool dx_transcode_quote_to_order_bid (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                      dx_quote_t* record_buffer, int record_count) {
    static dx_char_t exchange_code = 0;
    static bool is_exchange_code_initialized = false;
    
    int i = 0;
    dxf_order_t* event_buffer = NULL;
    
    if (!is_exchange_code_initialized) {
        exchange_code = dx_get_record_exchange_code(dx_rid_quote);
        is_exchange_code_initialized = true;
    }
    
    if ((event_buffer = (dxf_order_t*)dx_get_event_data_buffer(dx_eid_order, record_count)) == NULL) {
        return false;
    }
    
    for (; i < record_count; ++i) {
        dx_quote_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        
        cur_event->time = ((dx_long_t)(cur_record->bid_time) * 1000L);
        cur_event->price = cur_record->bid_price;
        cur_event->size = cur_record->bid_size;
        cur_event->index = (((dx_long_t)exchange_code << 32) | 0x8000000000000000L);
        cur_event->side = DXF_ORDER_SIDE_BUY;
        cur_event->level = (exchange_code == 0 ? DXF_ORDER_LEVEL_COMPOSITE : DXF_ORDER_LEVEL_REGIONAL);
        cur_event->exchange_code = exchange_code;
        cur_event->market_maker = NULL;
    }
    
    return dx_process_event_data(dx_eid_order, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool dx_transcode_quote_to_order_ask (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                      dx_quote_t* record_buffer, int record_count) {
    static dx_char_t exchange_code = 0;
    static bool is_exchange_code_initialized = false;
    
    int i = 0;
    dxf_order_t* event_buffer = NULL;

    if (!is_exchange_code_initialized) {
        exchange_code = dx_get_record_exchange_code(dx_rid_quote);
        is_exchange_code_initialized = true;
    }

    if ((event_buffer = (dxf_order_t*)dx_get_event_data_buffer(dx_eid_order, record_count)) == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_quote_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;

        cur_event->time = ((dx_long_t)(cur_record->ask_time) * 1000L);
        cur_event->price = cur_record->ask_price;
        cur_event->size = cur_record->ask_size;
        cur_event->index = (((dx_long_t)exchange_code << 32) | 0x8100000000000000L);
        cur_event->side = DXF_ORDER_SIDE_SELL;
        cur_event->level = (exchange_code == 0 ? DXF_ORDER_LEVEL_COMPOSITE : DXF_ORDER_LEVEL_REGIONAL);
        cur_event->exchange_code = exchange_code;
        cur_event->market_maker = NULL;
    }

    return dx_process_event_data(dx_eid_order, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool dx_transcode_quote (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                         dx_quote_t* record_buffer, int record_count) {
    dxf_quote_t* event_buffer = (dxf_quote_t*)record_buffer;
    int i = 0;

    for (; i < record_count; ++i) {
        dxf_quote_t* cur_event = event_buffer + i;

        cur_event->bid_time *= 1000L;
        cur_event->ask_time *= 1000L;
    }

    return dx_process_event_data(dx_eid_quote, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_quote_t) (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                         void* record_buffer, int record_count) {
    /* note that it's important to call the order transcoders before the quote one,
       because the quote transcoder alters some values right within the same record buffer,
       which would affect the order transcoding if it took place before it. */
    
    if (!dx_transcode_quote_to_order_bid(symbol_name, symbol_cipher, (dx_quote_t*)record_buffer, record_count)) {
        return false;
    }
    
    if (!dx_transcode_quote_to_order_ask(symbol_name, symbol_cipher, (dx_quote_t*)record_buffer, record_count)) {
        return false;
    }
    
    if (!dx_transcode_quote(symbol_name, symbol_cipher, (dx_quote_t*)record_buffer, record_count)) {
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_fundamental_t) (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                               void* record_buffer, int record_count) {
    /* no transcoding actions are required */
    
    return dx_process_event_data(dx_eid_summary, symbol_name, symbol_cipher, record_buffer, record_count);
}

/* -------------------------------------------------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_profile_t) (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                           void* record_buffer, int record_count) {
    /* no transcoding actions are required */

    return dx_process_event_data(dx_eid_profile, symbol_name, symbol_cipher, record_buffer, record_count);
}

/* -------------------------------------------------------------------------- */

bool dx_transcode_market_maker_to_order_bid (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                             dx_market_maker_t* record_buffer, int record_count) {
    int i = 0;
    dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(dx_eid_order, record_count);    

    if (event_buffer == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_market_maker_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dx_char_t exchange_code = (dx_char_t)cur_record->mm_exchange;

        cur_event->exchange_code = exchange_code;
        cur_event->market_maker = dx_decode_from_integer(cur_record->mm_id);
        cur_event->price = cur_record->mmbid_price;
        cur_event->size = cur_record->mmbid_size;
        cur_event->index = (((dx_long_t)exchange_code << 32) | ((dx_long_t)cur_record->mm_id) | 0x8200000000000000L);
        cur_event->side = DXF_ORDER_SIDE_BUY;
        cur_event->level = DXF_ORDER_LEVEL_AGGREGATE;
        
        if (cur_event->market_maker == NULL ||
            !dx_store_string_buffer(cur_event->market_maker)) {
            
            return false;
        }
    }

    return dx_process_event_data(dx_eid_order, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool dx_transcode_market_maker_to_order_ask (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                             dx_market_maker_t* record_buffer, int record_count) {
    int i = 0;
    dxf_order_t* event_buffer = (dxf_order_t*)dx_get_event_data_buffer(dx_eid_order, record_count);

    if (event_buffer == NULL) {
        return false;
    }

    for (; i < record_count; ++i) {
        dx_market_maker_t* cur_record = record_buffer + i;
        dxf_order_t* cur_event = event_buffer + i;
        dx_char_t exchange_code = (dx_char_t)cur_record->mm_exchange;

        cur_event->exchange_code = exchange_code;
        cur_event->market_maker = dx_decode_from_integer(cur_record->mm_id);
        cur_event->price = cur_record->mmask_price;
        cur_event->size = cur_record->mmask_size;
        cur_event->index = (((dx_long_t)exchange_code << 32) | ((dx_long_t)cur_record->mm_id) | 0x8300000000000000L);
        cur_event->side = DXF_ORDER_SIDE_SELL;
        cur_event->level = DXF_ORDER_LEVEL_AGGREGATE;

        if (cur_event->market_maker == NULL ||
            !dx_store_string_buffer(cur_event->market_maker)) {

            return false;
        }
    }

    return dx_process_event_data(dx_eid_order, symbol_name, symbol_cipher, event_buffer, record_count);
}

/* ---------------------------------- */

bool RECORD_TRANSCODER_NAME(dx_market_maker_t) (dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                                                void* record_buffer, int record_count) {
    if (!dx_transcode_market_maker_to_order_bid(symbol_name, symbol_cipher, (dx_market_maker_t*)record_buffer, record_count)) {
        return false;
    }

    if (!dx_transcode_market_maker_to_order_ask(symbol_name, symbol_cipher, (dx_market_maker_t*)record_buffer, record_count)) {
        return false;
    }
    
    return true;
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
    RECORD_TRANSCODER_NAME(dx_market_maker_t)
};

/* -------------------------------------------------------------------------- */

bool dx_transcode_record_data (dx_record_id_t record_id, dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                               void* record_buffer, int record_count) {
    return g_record_transcoders[record_id](symbol_name, symbol_cipher, record_buffer, record_count);
}