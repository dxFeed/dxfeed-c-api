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
 
#include "RecordBuffers.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"

/* -------------------------------------------------------------------------- */
/*
 *	Internal data structures and types
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    void* buffer;
    int capacity;
} dx_event_record_buffer_t;

static dx_event_record_buffer_t g_event_record_buffer_array[dx_rid_count] = { 0 };

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
    void* GET_RECORD_PTR_NAME(record_id) (int record_index) { \
        dx_event_record_buffer_t* record_buffer = &g_event_record_buffer_array[record_id]; \
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
    void* GET_RECORD_BUF_PTR_NAME(record_id) (void) { \
        return g_event_record_buffer_array[record_id].buffer; \
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
GET_RECORD_PTR_BODY(dx_rid_fundamental, dx_fundamental_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_fundamental)
GET_RECORD_PTR_BODY(dx_rid_profile, dx_profile_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_profile)
GET_RECORD_PTR_BODY(dx_rid_market_maker, dx_market_maker_t)
GET_RECORD_BUF_PTR_BODY(dx_rid_market_maker)

/* -------------------------------------------------------------------------- */
/*
 *	Buffer manager collection
 */
/* -------------------------------------------------------------------------- */

const dx_buffer_manager_collection_t g_buffer_managers[dx_rid_count] = {
    { GET_RECORD_PTR_NAME(dx_rid_trade), GET_RECORD_BUF_PTR_NAME(dx_rid_trade) },
    { GET_RECORD_PTR_NAME(dx_rid_quote), GET_RECORD_BUF_PTR_NAME(dx_rid_quote) },
    { GET_RECORD_PTR_NAME(dx_rid_fundamental), GET_RECORD_BUF_PTR_NAME(dx_rid_fundamental) },
    { GET_RECORD_PTR_NAME(dx_rid_profile), GET_RECORD_BUF_PTR_NAME(dx_rid_profile) },
    { GET_RECORD_PTR_NAME(dx_rid_market_maker), GET_RECORD_BUF_PTR_NAME(dx_rid_market_maker) },
};

void dx_clear_record_buffers (void) {
    int i = 0;
    
    for (; i < dx_rid_count; ++i) {
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
    int size;
    int capacity;
} g_string_buffers = { 0 };

/* -------------------------------------------------------------------------- */

bool dx_store_string_buffer (dx_const_string_t buf) {
    bool failed = false;
    
    DX_ARRAY_INSERT(g_string_buffers, dx_const_string_t, buf, g_string_buffers.size, dx_capacity_manager_halfer, failed);
    
    return !failed;
}

/* -------------------------------------------------------------------------- */

void dx_free_string_buffers (void) {
    int i = 0;
    
    for (; i < g_string_buffers.size; ++i) {
        dx_free((void*)g_string_buffers.elements[i]);
    }
    
    if (g_string_buffers.elements != NULL) {
        dx_free((void*)g_string_buffers.elements);
    }
    
    g_string_buffers.elements = NULL;
    g_string_buffers.size = 0;
    g_string_buffers.capacity = 0;
}
