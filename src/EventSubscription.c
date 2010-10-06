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
 
#include "EventSubscription.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "DXMemory.h"
#include "DXThreads.h"
#include "SymbolCodec.h"
#include "DXAlgorithms.h"
#include "Logger.h"
#include "ConnectionContextData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Subscription error codes
 */
/* -------------------------------------------------------------------------- */

static const dx_error_code_descr_t g_event_subscription_errors[] = {
    { dx_es_invalid_event_type, L"Invalid event type" },
    { dx_es_invalid_subscr_id, L"Invalid subscription descriptor" },
    { dx_es_invalid_internal_structure_state, L"Internal software error" },
    { dx_es_invalid_symbol_name, L"Invalid symbol name" },
    { dx_es_invalid_listener, L"Invalid listener" },
    { dx_es_null_ptr_param, L"Internal software error" },

    { ERROR_CODE_FOOTER, ERROR_DESCR_FOOTER }
};

const dx_error_code_descr_t* event_subscription_error_roster = g_event_subscription_errors;

/* -------------------------------------------------------------------------- */
/*
 *	Internal data structures and objects
 */
/* -------------------------------------------------------------------------- */

struct dx_subscription_data_struct_t;
typedef struct dx_subscription_data_struct_t dx_subscription_data_t;
typedef dx_subscription_data_t* dx_subscription_data_ptr_t;

typedef struct {
    dx_subscription_data_ptr_t* elements;
    int size;
    int capacity;
} dx_subscription_data_array_t;

typedef struct {
    dx_const_string_t name;
    dx_int_t cipher;
    int ref_count;
    dx_subscription_data_array_t subscriptions;
    dx_event_data_t* last_events;
    dx_event_data_t* last_events_accessed;
} dx_symbol_data_t, *dx_symbol_data_ptr_t;

typedef struct {
    dx_event_listener_t* elements;
    int size;
    int capacity;
} dx_listener_array_t;

typedef struct {
    dx_symbol_data_ptr_t* elements;
    int size;
    int capacity;
} dx_symbol_data_array_t;

struct dx_subscription_data_struct_t {
    dxf_connection_t connection;
    int event_types;
    dx_symbol_data_array_t symbols;
    dx_listener_array_t listeners;
    bool is_muted;
};

/* -------------------------------------------------------------------------- */
/*
 *	Symbol map structures
 */
/* -------------------------------------------------------------------------- */

#define SYMBOL_BUCKET_COUNT    1000

/* -------------------------------------------------------------------------- */
/*
 *	Event subscription connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    pthread_mutex_t subscr_guard;
    
    dx_symbol_data_array_t ciphered_symbols[SYMBOL_BUCKET_COUNT];
    dx_symbol_data_array_t hashed_symbols[SYMBOL_BUCKET_COUNT];
    
    dx_subscription_data_array_t subscriptions;
    
    int set_fields_flags;
} dx_event_subscription_connection_context_t;

#define MUTEX_FIELD_FLAG    (0x1)

#define CONTEXT_VAL(ctx) \
    dx_event_subscription_connection_context_t* ctx
    
#define GET_CONTEXT_VAL(context, subscr_data_ptr) \
    do { \
        context = dx_get_subsystem_data(subscr_data_ptr->connection, dx_ccs_event_subscription); \
    } while(false)

/* -------------------------------------------------------------------------- */

bool dx_clear_event_subscription_connection_context (dx_event_subscription_connection_context_t* context);

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_event_subscription) {
    dx_event_subscription_connection_context_t* context = dx_calloc(1, sizeof(dx_event_subscription_connection_context_t));
    
    if (context == NULL) {
        return false;
    }
    
    if (!dx_mutex_create(&(context->subscr_guard))) {
        dx_clear_event_subscription_connection_context(context);
        
        return false;
    }
    
    context->set_fields_flags |= MUTEX_FIELD_FLAG;
    
    if (!dx_set_subsystem_data(connection, dx_ccs_event_subscription, context)) {
        dx_clear_event_subscription_connection_context(context);
        
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_event_subscription) {
    dx_event_subscription_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_event_subscription);
    
    if (context == NULL) {
        return true;
    }
    
    return dx_clear_event_subscription_connection_context(context);
}

/* -------------------------------------------------------------------------- */

bool dx_clear_event_subscription_connection_context (dx_event_subscription_connection_context_t* context) {
    bool res = true;
    int i = 0;
    
    for (; i < context->subscriptions.size; ++i) {
        res = dx_close_event_subscription((dxf_subscription_t)context->subscriptions.elements[i]) && res;
    }
    
    if (IS_FLAG_SET(context->set_fields_flags, MUTEX_FIELD_FLAG)) {
        res = dx_mutex_destroy(&(context->subscr_guard)) && res;
    }
    
    if (context->subscriptions.elements != NULL) {
        dx_free(context->subscriptions.elements);
    }
    
    dx_free(context);
    
    return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

dx_int_t dx_symbol_name_hasher (dx_const_string_t symbol_name) {
    dx_int_t h = 0;
    int len = 0;
    int i = 0;
    
    len = dx_string_length(symbol_name);
    
    for (; i < len; ++i) {
        h = 5 * h + dx_toupper(symbol_name[i]);
    }
    
    return h;
}

/* -------------------------------------------------------------------------- */

typedef int (*dx_symbol_comparator_t)(dx_symbol_data_ptr_t e1, dx_symbol_data_ptr_t e2);

int dx_ciphered_symbol_comparator (dx_symbol_data_ptr_t e1, dx_symbol_data_ptr_t e2) {
    return (e1->cipher - e2->cipher);
}

/* -------------------------------------------------------------------------- */

int dx_hashed_symbol_comparator (dx_symbol_data_ptr_t e1, dx_symbol_data_ptr_t e2) {
    if (e1->cipher != e2->cipher) {
        return (e1->cipher - e2->cipher);
    }
    
    return dx_compare_strings(e1->name, e2->name);
}

/* -------------------------------------------------------------------------- */

int dx_name_symbol_comparator (dx_symbol_data_ptr_t e1, dx_symbol_data_ptr_t e2) {
    return dx_compare_strings(e1->name, e2->name);
}

/* -------------------------------------------------------------------------- */

int dx_get_bucket_index (dx_int_t cipher) {
    dx_int_t mod = cipher % SYMBOL_BUCKET_COUNT;
    
    if (mod < 0) {
        mod += SYMBOL_BUCKET_COUNT;
    }
    
    return (int)mod;
}

/* -------------------------------------------------------------------------- */

void dx_cleanup_event_data_array (dx_event_data_t* data_array) {
    int i = dx_eid_begin;
    
    if (data_array == NULL) {
        return;
    }
    
    for (; i < dx_eid_count; ++i) {
        CHECKED_FREE(data_array[i]);
    }
    
    dx_free(data_array);
}

/* ---------------------------------- */

dx_symbol_data_ptr_t dx_cleanup_symbol_data (dx_symbol_data_ptr_t symbol_data) {
    if (symbol_data == NULL) {
        return NULL;
    }
    
    CHECKED_FREE(symbol_data->name);
    
    dx_cleanup_event_data_array(symbol_data->last_events);
    dx_cleanup_event_data_array(symbol_data->last_events_accessed);
    
    dx_free(symbol_data);
    
    return NULL;
}

/* -------------------------------------------------------------------------- */

dx_symbol_data_ptr_t dx_create_symbol_data (dx_const_string_t name, dx_int_t cipher) {
    int i = dx_eid_begin;
    dx_symbol_data_ptr_t res = (dx_symbol_data_ptr_t)dx_calloc(1, sizeof(dx_symbol_data_t));
    
    if (res == NULL) {
        return res;
    }
    
    res->name = dx_create_string_src(name);
    res->cipher = cipher;
    
    if (res->name == NULL) {
        return dx_cleanup_symbol_data(res);
    }
    
    res->last_events = dx_calloc(dx_eid_count, sizeof(dx_event_data_t));
    res->last_events_accessed = dx_calloc(dx_eid_count, sizeof(dx_event_data_t));
    
    if (res->last_events == NULL ||
        res->last_events_accessed == NULL) {
        
        return dx_cleanup_symbol_data(res);
    }
    
    for (; i < dx_eid_count; ++i) {
        res->last_events[i] = dx_calloc(1, dx_get_event_data_struct_size(i));
        res->last_events_accessed[i] = dx_calloc(1, dx_get_event_data_struct_size(i));
        
        if (res->last_events[i] == NULL ||
            res->last_events_accessed[i] == NULL) {
            
            return dx_cleanup_symbol_data(res);
        }
    }

    return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions
 */
/* -------------------------------------------------------------------------- */

dx_symbol_data_ptr_t dx_subscribe_symbol (CONTEXT_VAL(ctx),
                                          dx_const_string_t symbol_name, dx_subscription_data_ptr_t owner) {
    dx_symbol_data_ptr_t res = NULL;
    bool is_just_created = false;
    
    dx_logging_info(L"Subscribe symbol: %s", symbol_name);

    {
        dx_symbol_comparator_t comparator = dx_ciphered_symbol_comparator;
        dx_symbol_data_array_t* symbol_container = ctx->ciphered_symbols;
        dx_symbol_data_array_t* symbol_array_obj_ptr = NULL;
        dx_symbol_data_t dummy;

        bool symbol_exists = false;
        int symbol_index;
        bool failed = false;

        dummy.name = symbol_name;

        if ((dummy.cipher = dx_encode_symbol_name(symbol_name)) == 0) {
            symbol_container = ctx->hashed_symbols;
            comparator = dx_hashed_symbol_comparator;
            dummy.cipher = dx_symbol_name_hasher(symbol_name);
        }

        symbol_array_obj_ptr = &(symbol_container[dx_get_bucket_index(dummy.cipher)]);

        DX_ARRAY_SEARCH(symbol_array_obj_ptr->elements, 0, symbol_array_obj_ptr->size,
                        &dummy, comparator, true, symbol_exists, symbol_index);

        if (!symbol_exists) {
            res = dx_create_symbol_data(symbol_name, dummy.cipher);

            if (res == NULL) {
                return NULL;
            }
            
            is_just_created = true;

            DX_ARRAY_INSERT(*symbol_array_obj_ptr, dx_symbol_data_ptr_t, res, symbol_index, dx_capacity_manager_halfer, failed);

            if (failed) {
                return dx_cleanup_symbol_data(res);
            }
        } else {
            res = symbol_array_obj_ptr->elements[symbol_index];
        }
    }
    
    {
        bool subscr_exists = false;
        int subscr_index;
        bool failed = false;
        
        DX_ARRAY_SEARCH(res->subscriptions.elements, 0, res->subscriptions.size, owner, DX_NUMERIC_COMPARATOR, false,
                        subscr_exists, subscr_index);
                        
        if (subscr_exists) {
            return res;
        }
        
        DX_ARRAY_INSERT(res->subscriptions, dx_subscription_data_ptr_t, owner, subscr_index, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            return (is_just_created ? dx_cleanup_symbol_data(res) : NULL);
        }
        
        ++(res->ref_count);
    }
    
    return res;
}

/* -------------------------------------------------------------------------- */

bool dx_unsubscribe_symbol (CONTEXT_VAL(ctx), dx_symbol_data_ptr_t symbol_data, dx_subscription_data_ptr_t owner) {
    bool res = true;
    
    do {
        bool subscr_exists = false;
        int subscr_index;
        bool failed = false;

        dx_logging_info(L"Unsubscribe symbol: %s", symbol_data->name);
        
        DX_ARRAY_SEARCH(symbol_data->subscriptions.elements, 0, symbol_data->subscriptions.size, owner, DX_NUMERIC_COMPARATOR, false,
                        subscr_exists, subscr_index);

        if (!subscr_exists) {
            /* should never be here */
            
            res = false;
            
            break;
        }

        DX_ARRAY_DELETE(symbol_data->subscriptions, dx_subscription_data_ptr_t, subscr_index, dx_capacity_manager_halfer, failed);

        if (failed) {
            /* most probably the memory allocation error */
            
            res = false;
        }
    } while (false);
    
    if (--(symbol_data->ref_count) == 0) {
        dx_symbol_comparator_t comparator = dx_ciphered_symbol_comparator;
        dx_symbol_data_array_t* symbol_container = ctx->ciphered_symbols;
        dx_symbol_data_array_t* symbol_array_obj_ptr = NULL;
        
        bool symbol_exists = false;
        int symbol_index;
        bool failed = false;
        
        if (dx_encode_symbol_name(symbol_data->name) == 0) {
            symbol_container = ctx->hashed_symbols;
            comparator = dx_hashed_symbol_comparator;
        }
        
        symbol_array_obj_ptr = &(symbol_container[dx_get_bucket_index(symbol_data->cipher)]);
        
        DX_ARRAY_SEARCH(symbol_array_obj_ptr->elements, 0, symbol_array_obj_ptr->size,
                        symbol_data, comparator, true, symbol_exists, symbol_index);

        if (!symbol_exists) {
            dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_internal_structure_state);
            
            res = false;
        }
        
        DX_ARRAY_DELETE(*symbol_array_obj_ptr, dx_symbol_data_ptr_t, symbol_index, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            res = false;
        }
        
        dx_cleanup_symbol_data(symbol_data);
    }
    
    return res;
}

/* -------------------------------------------------------------------------- */

void dx_clear_listener_array (dx_listener_array_t* listeners) {
    dx_free(listeners->elements);
    
    listeners->elements = NULL;
    listeners->size = 0;
    listeners->capacity = 0;
}

/* -------------------------------------------------------------------------- */

bool dx_clear_symbol_array (CONTEXT_VAL(ctx), dx_symbol_data_array_t* symbols, dx_subscription_data_ptr_t owner) {
    int symbol_index = 0;
    bool res = true;
    
    for (; symbol_index < symbols->size; ++symbol_index) {
        dx_symbol_data_ptr_t symbol_data = symbols->elements[symbol_index];
        
        res = dx_unsubscribe_symbol(ctx, symbol_data, owner) && res;
    }
    
    dx_free(symbols->elements);
    
    symbols->elements = NULL;
    symbols->size = 0;
    symbols->capacity = 0;
    
    return res;
}

/* -------------------------------------------------------------------------- */

int dx_find_symbol_in_array (dx_symbol_data_array_t* symbols, dx_const_string_t symbol_name, OUT bool* found) {
    dx_symbol_data_t data;
    dx_symbol_comparator_t comparator;
    int symbol_index;
    
    data.name = symbol_name;
    
    if ((data.cipher = dx_encode_symbol_name(symbol_name)) != 0) {
        comparator = dx_ciphered_symbol_comparator;
    } else {
        comparator = dx_name_symbol_comparator;
    }
    
    DX_ARRAY_SEARCH(symbols->elements, 0, symbols->size, &data, comparator, false, *found, symbol_index);
    
    return symbol_index;
}

/* -------------------------------------------------------------------------- */

int dx_find_listener_in_array (dx_listener_array_t* listeners, dx_event_listener_t listener, OUT bool* found) {
    int listener_index;
    
    DX_ARRAY_SEARCH(listeners->elements, 0, listeners->size, listener, DX_NUMERIC_COMPARATOR, false, *found, listener_index);
    
    return listener_index;
}

/* -------------------------------------------------------------------------- */

dx_symbol_data_ptr_t dx_find_symbol (CONTEXT_VAL(ctx), dx_const_string_t symbol_name, dx_int_t symbol_cipher) {
    dx_symbol_data_ptr_t res = NULL;

    dx_symbol_comparator_t comparator = dx_ciphered_symbol_comparator;
    dx_symbol_data_array_t* symbol_container = ctx->ciphered_symbols;
    dx_symbol_data_array_t* symbol_array_obj_ptr = NULL;
    dx_symbol_data_t dummy;

    bool symbol_exists = false;
    int symbol_index;

    dummy.name = symbol_name;

    if ((dummy.cipher = symbol_cipher) == 0) {
        symbol_container = ctx->hashed_symbols;
        comparator = dx_hashed_symbol_comparator;
        dummy.cipher = dx_symbol_name_hasher(symbol_name);
    }

    symbol_array_obj_ptr = &(symbol_container[dx_get_bucket_index(dummy.cipher)]);

    DX_ARRAY_SEARCH(symbol_array_obj_ptr->elements, 0, symbol_array_obj_ptr->size,
                    &dummy, comparator, true, symbol_exists, symbol_index);
    
    if (!symbol_exists) {
        return NULL;
    }
    
    return symbol_array_obj_ptr->elements[symbol_index];
}

/* -------------------------------------------------------------------------- */

void dx_store_last_symbol_event (dx_symbol_data_ptr_t symbol_data, dx_event_id_t event_id,
                                 const dx_event_data_t data, int data_count) {
    dx_memcpy(symbol_data->last_events[event_id],
              dx_get_event_data_item(event_id, data, data_count - 1),
              dx_get_event_data_struct_size(event_id));
}

/* -------------------------------------------------------------------------- */

bool dx_add_subscription_to_context (dx_subscription_data_ptr_t subscr_data) {
    bool subscr_exists = false;
    int subscr_index;
    bool failed = false;
    
    CONTEXT_VAL(ctx);
    GET_CONTEXT_VAL(ctx, subscr_data);

    DX_ARRAY_SEARCH(ctx->subscriptions.elements, 0, ctx->subscriptions.size, subscr_data, DX_NUMERIC_COMPARATOR, false,
                    subscr_exists, subscr_index);

    if (subscr_exists) {
        /* should never be here */
        
        return false;
    }

    DX_ARRAY_INSERT(ctx->subscriptions, dx_subscription_data_ptr_t, subscr_data, subscr_index, dx_capacity_manager_halfer, failed);

    if (failed) {
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_remove_subscription_from_context (dx_subscription_data_ptr_t subscr_data) {
    bool subscr_exists = false;
    int subscr_index;
    bool failed = false;
    
    CONTEXT_VAL(ctx);
    GET_CONTEXT_VAL(ctx, subscr_data);

    DX_ARRAY_SEARCH(ctx->subscriptions.elements, 0, ctx->subscriptions.size, subscr_data, DX_NUMERIC_COMPARATOR, false,
                    subscr_exists, subscr_index);

    if (!subscr_exists) {
        /* should never be here */

        return false;
    }

    DX_ARRAY_DELETE(ctx->subscriptions, dx_subscription_data_ptr_t, subscr_index, dx_capacity_manager_halfer, failed);

    if (failed) {
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Subscription functions implementation
 */
/* -------------------------------------------------------------------------- */
 
const dxf_subscription_t dx_invalid_subscription = (dxf_subscription_t)NULL;

dxf_subscription_t dx_create_event_subscription (dxf_connection_t connection, int event_types) {
    dx_subscription_data_ptr_t subscr_data = NULL;
    
    if (event_types & DXF_ET_UNUSED) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_event_type);
        
        return dx_invalid_subscription;
    }
    
    subscr_data = dx_calloc(1, sizeof(dx_subscription_data_t));
    
    if (subscr_data == NULL) {
        return dx_invalid_subscription;
    }
    
    subscr_data->connection = connection;
    subscr_data->event_types = event_types;
    
    if (!dx_add_subscription_to_context(subscr_data)) {
        dx_free(subscr_data);
        
        return dx_invalid_subscription;
    }

    return (dxf_subscription_t)subscr_data;
}

/* -------------------------------------------------------------------------- */
 
bool dx_mute_event_subscription (dxf_subscription_t subscr_id) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    subscr_data->is_muted = true;
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_unmute_event_subscription (dxf_subscription_t subscr_id) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;

    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }

    subscr_data->is_muted = false;

    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_get_event_subscription_mute_state (dxf_subscription_t subscr_id, OUT bool* state) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;

    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    if (state == NULL) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_null_ptr_param);
        
        return false;
    }

    *state = subscr_data->is_muted;

    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_close_event_subscription (dxf_subscription_t subscr_id) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    bool res = true;
    dx_event_subscription_connection_context_t* context = NULL;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    GET_CONTEXT_VAL(context, subscr_data);
    
    /* locking a guard mutex */
    if (!dx_mutex_lock(&(context->subscr_guard))) {
        return false;
    }
    
    res = dx_clear_symbol_array(context, &(subscr_data->symbols), subscr_data) && res;
    dx_clear_listener_array(&(subscr_data->listeners));
    
    res = dx_mutex_unlock(&(context->subscr_guard)) && res;
    
    if (!dx_remove_subscription_from_context(subscr_data)) {
        res = false;
    }
    
    dx_free(subscr_data);
    
    return res;
}

/* -------------------------------------------------------------------------- */

bool dx_add_symbols (dxf_subscription_t subscr_id, dx_const_string_t* symbols, int symbol_count) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    int cur_symbol_index = 0;
    dx_event_subscription_connection_context_t* context = NULL;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    GET_CONTEXT_VAL(context, subscr_data);

    /* a guard mutex is required to protect the internal containers 
       from the secondary data retriever threads */
    if (!dx_mutex_lock(&(context->subscr_guard))) {
        return false;
    }
    
    for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
        dx_symbol_data_ptr_t symbol_data;
        int symbol_index;
        bool found = false;
        bool failed = false;
        
        if (symbols[cur_symbol_index] == NULL) {
            dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_symbol_name);
            
            dx_mutex_unlock(&(context->subscr_guard));

            return false;
        }
        
        symbol_index = dx_find_symbol_in_array(&(subscr_data->symbols), symbols[cur_symbol_index], &found);
        
        if (found) {
            /* symbol is already subscribed */

            continue;
        }
        
        symbol_data = dx_subscribe_symbol(context, symbols[cur_symbol_index], subscr_data);
        
        if (symbol_data == NULL) {
            dx_mutex_unlock(&(context->subscr_guard));
            
            return false;
        }
        
        DX_ARRAY_INSERT(subscr_data->symbols, dx_symbol_data_ptr_t, symbol_data, symbol_index, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            dx_mutex_unlock(&(context->subscr_guard));

            return false;
        }
    }
    
    if (!dx_mutex_unlock(&(context->subscr_guard))) {
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_remove_symbols (dxf_subscription_t subscr_id, dx_const_string_t* symbols, int symbol_count) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    int cur_symbol_index = 0;
    CONTEXT_VAL(context) = NULL;

    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    GET_CONTEXT_VAL(context, subscr_data);

    /* a guard mutex is required to protect the internal containers 
       from the secondary data retriever threads */
    if (!dx_mutex_lock(&(context->subscr_guard))) {
        return false;
    }
    
    for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
        int symbol_index;
        bool failed = false;
        bool found = false;

        if (symbols[cur_symbol_index] == NULL) {
            dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_symbol_name);

            dx_mutex_unlock(&(context->subscr_guard));

            return false;
        }

        symbol_index = dx_find_symbol_in_array(&(subscr_data->symbols), symbols[cur_symbol_index], &found);
        
        if (!found) {
            /* symbol wasn't subscribed */

            continue;
        }
        
        if (!dx_unsubscribe_symbol(context, subscr_data->symbols.elements[symbol_index], subscr_data)) {
            dx_mutex_unlock(&(context->subscr_guard));

            return false;
        }
        
        DX_ARRAY_DELETE(subscr_data->symbols, dx_symbol_data_ptr_t, symbol_index, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            dx_mutex_unlock(&(context->subscr_guard));

            return false;
        }
    }
    
    if (!dx_mutex_unlock(&(context->subscr_guard))) {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_add_listener (dxf_subscription_t subscr_id, dx_event_listener_t listener) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    int listener_index;
    bool failed;
    bool found = false;
    CONTEXT_VAL(context) = NULL;

    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    GET_CONTEXT_VAL(context, subscr_data);
    
    if (listener == NULL) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_listener);

        return false;
    }
    
    listener_index = dx_find_listener_in_array(&(subscr_data->listeners), listener, &found);
    
    if (found) {
        /* listener is already added */
        
        return true;
    }
    
    dx_logging_info(L"Add listener: %d", listener_index);

    /* a guard mutex is required to protect the internal containers 
       from the secondary data retriever threads */
    if (!dx_mutex_lock(&(context->subscr_guard))) {
        return false;
    }
    
    DX_ARRAY_INSERT(subscr_data->listeners, dx_event_listener_t, listener, listener_index, dx_capacity_manager_halfer, failed);
    
    if (!dx_mutex_unlock(&context->subscr_guard)) {
        return false;
    }

    return !failed;
}

/* -------------------------------------------------------------------------- */

bool dx_remove_listener (dxf_subscription_t subscr_id, dx_event_listener_t listener) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    int listener_index;
    bool failed;
    bool found = false;
    CONTEXT_VAL(context) = NULL;

    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    GET_CONTEXT_VAL(context, subscr_data);

    if (listener == NULL) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_listener);

        return false;
    }

    listener_index = dx_find_listener_in_array(&(subscr_data->listeners), listener, &found);
    
    if (!found) {
        /* listener isn't subscribed */

        return true;
    }

    dx_logging_info(L"Remove listener: %d", listener_index);

    /* a guard mutex is required to protect the internal containers 
       from the secondary data retriever threads */
    if (!dx_mutex_lock(&(context->subscr_guard))) {
        return false;
    }

    DX_ARRAY_DELETE(subscr_data->listeners, dx_event_listener_t, listener_index, dx_capacity_manager_halfer, failed);

    if (!dx_mutex_unlock(&(context->subscr_guard))) {
        return false;
    }

    return !failed;
}

/* -------------------------------------------------------------------------- */

bool dx_get_subscription_connection (dxf_subscription_t subscr_id, OUT dxf_connection_t* connection) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }

    *connection = subscr_data->connection;
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_get_event_subscription_event_types (dxf_subscription_t subscr_id, OUT int* event_types) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    
    if (event_types == NULL) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_event_type);

        return false;
    }
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    *event_types = subscr_data->event_types;
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_get_event_subscription_symbols (dxf_subscription_t subscr_id, OUT dx_const_string_t** symbols, OUT int* symbol_count) {
    static int array_capacity = 0;
    static dx_const_string_t* symbol_array = NULL;
    int i = 0;
    
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;

    if (symbols == NULL || symbol_count == NULL) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_null_ptr_param);

        return false;
    }
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    if (subscr_data->symbols.size > array_capacity) {
        if (symbol_array != NULL) {
            dx_free((void*)symbol_array);
            
            array_capacity = 0;
        }
        
        symbol_array = dx_calloc(subscr_data->symbols.size, sizeof(dx_const_string_t*));
        
        if (symbol_array == NULL) {
            return false;
        }
        
        array_capacity = subscr_data->symbols.size;
    }
    
    for (; i < subscr_data->symbols.size; ++i) {
        symbol_array[i] = subscr_data->symbols.elements[i]->name;
    }
    
    *symbols = symbol_array;
    *symbol_count = subscr_data->symbols.size;
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_process_event_data (dxf_connection_t connection,
                            dx_event_id_t event_id, dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                            const dx_event_data_t data, int data_count) {
    dx_symbol_data_ptr_t symbol_data = NULL;
    int cur_subscr_index = 0;
    int event_bitmask = DX_EVENT_BIT_MASK(event_id);
    CONTEXT_VAL(context) = dx_get_subsystem_data(connection, dx_ccs_event_subscription);

    if (event_id < dx_eid_begin || event_id >= dx_eid_count) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_event_type);

        return false;
    }
    
    dx_logging_info(L"Process event data. Symbol: %s, data count: %d", symbol_name, data_count);

    /* this function is supposed to be called from a different thread than the other
       interface functions */
    if (!dx_mutex_lock(&(context->subscr_guard))) {
        return false;
    }
    
    if ((symbol_data = dx_find_symbol(context, symbol_name, symbol_cipher)) == NULL) {
        /* in fact, this is most likely a correct situation occurred because
           the data is received very soon after the symbol subscription
           has been annulled */

        dx_mutex_unlock(&(context->subscr_guard));
        
        return true;
    }

    dx_store_last_symbol_event(symbol_data, event_id, data, data_count);

    // notify listeners
    for (; cur_subscr_index < symbol_data->subscriptions.size; ++cur_subscr_index) {
        dx_subscription_data_ptr_t subscr_data = symbol_data->subscriptions.elements[cur_subscr_index];
        int cur_listener_index = 0;
        
        if (!(subscr_data->event_types & event_bitmask) || /* subscription doesn't want this specific event type */
            /*subscr_data->is_muted*/false) { /* subscription is currently muted */
            
            continue;
        }
        
        for (; cur_listener_index < subscr_data->listeners.size; ++cur_listener_index) {
            subscr_data->listeners.elements[cur_listener_index](event_bitmask, symbol_name, data, data_count);
        }
    }
    
    if (!dx_mutex_unlock(&(context->subscr_guard))) {
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	event type is a one-bit mask here
 */

bool dx_get_last_symbol_event (dxf_connection_t connection, dx_const_string_t symbol_name, int event_type,
                               OUT dx_event_data_t* event_data) {
    dx_symbol_data_ptr_t symbol_data = NULL;
    dx_int_t cipher = dx_encode_symbol_name(symbol_name);
    dx_event_id_t event_id;
    CONTEXT_VAL(context) = dx_get_subsystem_data(connection, dx_ccs_event_subscription);

    if (!dx_is_only_single_bit_set(event_type)) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_event_type);

        return false;
    }
    
    event_id = dx_get_event_id_by_bitmask(event_type);
    
    dx_logging_info(L"Getting last event. Symbol: %s, event type: %s", symbol_name, dx_event_type_to_string(event_type));

    if (!dx_mutex_lock(&(context->subscr_guard))) {
        return false;
    }

    if ((symbol_data = dx_find_symbol(context, symbol_name, cipher)) == NULL) { 
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_symbol_name);

        dx_mutex_unlock(&(context->subscr_guard));

        return false;
    }

    dx_memcpy(symbol_data->last_events_accessed[event_id], symbol_data->last_events[event_id], dx_get_event_data_struct_size(event_id));
    
    *event_data = symbol_data->last_events_accessed[event_id];

    if (!dx_mutex_unlock(&(context->subscr_guard))) {
        return false;
    }

    return true;
}