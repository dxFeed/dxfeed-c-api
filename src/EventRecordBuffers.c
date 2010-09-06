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
 
#include "EventRecordBuffers.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"

/* -------------------------------------------------------------------------- */
/*
 *	Internal data structures and types
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    void* buffer;
    size_t capacity;
} dx_event_record_buffer_t;

static dx_event_record_buffer_t g_event_record_buffer_array[dx_eid_count] = { 0 };

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manager prototype and body macros
 */
/* -------------------------------------------------------------------------- */

#define GET_RECORD_PTR_NAME(event_id) \
    event_id##_get_record_ptr
    
#define GET_RECORD_BUF_PTR_NAME(event_id) \
    event_id##_get_record_buf_ptr

#define GET_RECORD_PTR_BODY(event_id, record_type) \
    void* GET_RECORD_PTR_NAME(event_id) (size_t record_index) { \
        dx_event_record_buffer_t* record_buffer = &g_event_record_buffer_array[event_id]; \
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

#define GET_RECORD_BUF_PTR_BODY(event_id) \
    void* GET_RECORD_BUF_PTR_NAME(event_id) (void) { \
        return g_event_record_buffer_array[event_id].buffer; \
    }

/* -------------------------------------------------------------------------- */
/*
 *	Buffer managers implementation
 */
/* -------------------------------------------------------------------------- */

GET_RECORD_PTR_BODY(dx_eid_trade, dxf_trade_t)
GET_RECORD_BUF_PTR_BODY(dx_eid_trade)
GET_RECORD_PTR_BODY(dx_eid_quote, dxf_quote_t)
GET_RECORD_BUF_PTR_BODY(dx_eid_quote)
GET_RECORD_PTR_BODY(dx_eid_fundamental, dxf_fundamental_t)
GET_RECORD_BUF_PTR_BODY(dx_eid_fundamental)
GET_RECORD_PTR_BODY(dx_eid_market_maker, dxf_market_maker)
GET_RECORD_BUF_PTR_BODY(dx_eid_market_maker)

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manager collection
 */
/* -------------------------------------------------------------------------- */

const dx_buffer_manager_collection_t g_buffer_managers[dx_eid_count] = {
    { GET_RECORD_PTR_NAME(dx_eid_trade), GET_RECORD_BUF_PTR_NAME(dx_eid_trade) },
    { GET_RECORD_PTR_NAME(dx_eid_quote), GET_RECORD_BUF_PTR_NAME(dx_eid_quote) },
    { GET_RECORD_PTR_NAME(dx_eid_fundamental), GET_RECORD_BUF_PTR_NAME(dx_eid_fundamental) },
    { NULL, NULL },
    { GET_RECORD_PTR_NAME(dx_eid_market_maker), GET_RECORD_BUF_PTR_NAME(dx_eid_market_maker) },
};


void dx_clear_event_record_buffers (void) {
    size_t i = 0;
    
    for (; i < dx_eid_count; ++i) {
        if (g_event_record_buffer_array[i].buffer != NULL) {
            dx_free(g_event_record_buffer_array[i].buffer);
            
            g_event_record_buffer_array[i].buffer = NULL;
            g_event_record_buffer_array[i].capacity = 0;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary memory management stuff
 */
/* -------------------------------------------------------------------------- */

struct {
    dx_const_string_t* elements;
    size_t size;
    size_t capacity;
} g_string_buffers = { 0 };

/* -------------------------------------------------------------------------- */

bool dx_store_string_buffer (dx_const_string_t buf) {
    bool failed = false;
    
    DX_ARRAY_INSERT(g_string_buffers, dx_const_string_t, buf, g_string_buffers.size, dx_capacity_manager_halfer, failed);
    
    return !failed;
}

/* -------------------------------------------------------------------------- */

void dx_free_string_buffers (void) {
    size_t i = 0;
    
    for (; i < g_string_buffers.size; ++i) {
        dx_free((void*)g_string_buffers.elements[i]);
    }
    
    g_string_buffers.elements = NULL;
    g_string_buffers.size = 0;
    g_string_buffers.capacity = 0;
}
