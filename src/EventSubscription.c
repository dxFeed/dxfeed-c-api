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

#include <string.h>
#include <stdlib.h>

/* -------------------------------------------------------------------------- */
/*
 *	Subscription error codes
 */
/* -------------------------------------------------------------------------- */

static struct dx_error_code_descr_t g_event_subscription_errors[] = {
    { dx_es_invalid_event_type, "Invalid event type" },
    { dx_es_invalid_subscr_id, "Invalid subscription descriptor" },
    { dx_es_invalid_internal_structure_state, "Internal software error" },
    { dx_es_invalid_symbol_name, "Invalid symbol name" },
    { dx_es_invalid_listener, "Invalid listener" },

    { ERROR_CODE_FOOTER, ERROR_DESCR_FOOTER }
};

const struct dx_error_code_descr_t* event_subscription_error_roster = g_event_subscription_errors;

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
    size_t size;
    size_t capacity;
} dx_subscription_data_array_t;

typedef struct {
    dx_const_string_t name;
    dx_int_t cipher;
    size_t ref_count;
    dx_subscription_data_array_t subscriptions;
} dx_symbol_data_t, *dx_symbol_data_ptr_t;

typedef struct {
    dx_event_listener_t* elements;
    size_t size;
    size_t capacity;
} dx_listener_array_t;

typedef struct {
    dx_symbol_data_ptr_t* elements;
    size_t size;
    size_t capacity;
} dx_symbol_data_array_t;

struct dx_subscription_data_struct_t {
    int event_types;
    dx_symbol_data_array_t symbols;
    dx_listener_array_t listeners;
};

static size_t g_subscription_count = 0;
static pthread_mutex_t g_subscr_guard;

/* -------------------------------------------------------------------------- */
/*
 *	Symbol map structures
 */
/* -------------------------------------------------------------------------- */

#define SYMBOL_BUCKET_COUNT    1000

static dx_symbol_data_array_t g_ciphered_symbols[SYMBOL_BUCKET_COUNT] = {0};
static dx_symbol_data_array_t g_hashed_symbols[SYMBOL_BUCKET_COUNT] = {0};

/* -------------------------------------------------------------------------- */
/*
 *	Array functions
 */
/* -------------------------------------------------------------------------- */
/*
 *	Binary search
 
 *  a - array object (most likely of type 'type*' or 'type[]')
 *  start_index - the index of the first element which can contain the key
 *  end_index - the index of the last element which can contain the key increased by 1
 *  elem - the key we're looking for
 *  comparator - a binary ordering predicate:
        comparator(a, b) > 0 => a > b
        comparator(a, b) = 0 => a = b
        comparator(a, b) < 0 => a < b
 *  found - a boolean return value, if it's non-zero then the key was found in the array
 *  res_index - an integer return value, contains the index of an element containing the key,
                or the index of an element where the key might be inserted without breaking
                the ordering, if the element wasn't found
 */
/* -------------------------------------------------------------------------- */

#define DX_ARRAY_BINARY_SEARCH(a, start_index, end_index, elem, comparator, found, res_index) \
    do { \
        size_t _begin = start_index; \
        size_t _end = end_index; \
        size_t _mid; \
        int _comp_res; \
        \
        found = false; \
        \
        while (_begin < _end && !found) { \
            _mid = (_begin + _end) / 2; \
            _comp_res = comparator((a)[_mid], (elem)); \
            \
            if (_comp_res == 0) { \
                res_index = _mid; \
                found = true; \
            } else if (_comp_res < 0) { \
                _begin = _mid + 1; \
            } else { \
                _end = _mid; \
            } \
        } \
        \
        if (!found) { \
            res_index = _end; \
        } \
    } while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Search
 
 *  a - array object (most likely of type 'type*' or 'type[]')
 *  start_index - the index of the first element which can contain the key
 *  end_index - the index of the last element which can contain the key increased by 1
 *  elem - the key we're looking for
 *  comparator - a binary ordering predicate:
        comparator(a, b) > 0 => a > b
        comparator(a, b) = 0 => a = b
        comparator(a, b) < 0 => a < b
 *  is_binary - a boolean value: if it's non-zero then the the array is sorted by the order
                set by comparator
 *  found - a boolean return value, if it's non-zero then the key was found in the array
 *  res_index - an integer return value, contains the index of an element containing the key,
                or the index of an element where the key might be inserted without breaking
                the ordering, if the element wasn't found
 */
/* -------------------------------------------------------------------------- */

#define DX_ARRAY_SEARCH(a, start_index, end_index, elem, comparator, is_binary, found, res_index) \
    do { \
        found = false; \
        \
        if (is_binary) { \
            DX_ARRAY_BINARY_SEARCH(a, start_index, end_index, elem, comparator, found, res_index); \
        } else { \
            size_t _index = start_index; \
            \
            for (; _index < end_index; ++_index) { \
                if (comparator((a)[_index], (elem)) == 0) { \
                    found = true; \
                    \
                    break; \
                } \
            } \
            \
            res_index = _index; \
        } \
    } while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Insertion
 
 *  array_obj - an array descriptor structure. this structure must contain
                at least three fields:
        elements - a pointer to the array buffer (of type 'elem_type*')
        size - an integer value containing the number of elements in the buffer
        capacity - the current buffer capacity, may be greater than size
 *  elem_type - type of elements in the buffer
 *  new_elem - an element to insert
 *  position - an index of a position where the new element is to be inserted,
               is determined by a previous call of DX_ARRAY_SEARCH
 *  capacity_mgr - a function with two parameters:
        new_size - an integer parameter passed by value, contains the desired
                   new size of the array
        capacity - an integer parameter passed by pointer, contains the current
                   array capacity
                   
        Return value:
            true - reallocation is required, in this case the new capacity
                   value is stored in the capacity parameter
            false - no reallocation is required
 *  error - a boolean return value, is true if some error occurred during insertion,
            is false otherwise
 */
/* -------------------------------------------------------------------------- */

#define DX_ARRAY_INSERT(array_obj, elem_type, new_elem, position, capacity_mgr, error) \
    do { \
        void* _buffer_to_free = NULL; \
        elem_type* _new_elem_buffer = (array_obj).elements; \
        \
        error = false; \
        \
        if (capacity_mgr((array_obj).size + 1, &((array_obj).capacity))) { \
            _new_elem_buffer = (elem_type*)dx_calloc((array_obj).capacity, sizeof(elem_type)); \
            \
            if (_new_elem_buffer == NULL) { \
                error = true; \
                \
                break; \
            } \
            \
            memcpy((void*)_new_elem_buffer, (const void*)(array_obj).elements, position * sizeof(elem_type)); \
            \
            _buffer_to_free = (void*)(array_obj).elements; \
        } \
        \
        _new_elem_buffer[position] = new_elem; \
        \
        if (_buffer_to_free == NULL) { \
            memmove((void*)(_new_elem_buffer + position + 1), (const void*)((array_obj).elements + position), \
                    ((array_obj).size - position) * sizeof(elem_type)); \
        } else { \
            memcpy((void*)(_new_elem_buffer + position + 1), (const void*)((array_obj).elements + position), \
                   ((array_obj).size - position) * sizeof(elem_type)); \
            dx_free(_buffer_to_free); \
        } \
        \
        (array_obj).elements = _new_elem_buffer; \
        ++((array_obj).size); \
    } while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Deletion
 
 *  array_obj - an array descriptor structure. this structure must contain
                at least three fields:
        elements - a pointer to the array buffer (of type 'elem_type*')
        size - an integer value containing the number of elements in the buffer
        capacity - the current buffer capacity, may be greater than size
 *  elem_type - type of elements in the buffer
 *  position - an index of a position where the new element is to be inserted,
               is determined by a previous call of DX_ARRAY_SEARCH
 *  capacity_mgr - a function with two parameters:
        new_size - an integer parameter passed by value, contains the desired
                   new size of the array
        capacity - an integer parameter passed by pointer, contains the current
                   array capacity

        Return value:
            true - reallocation is required, in this case the new capacity
                   value is stored in the capacity parameter
            false - no reallocation is required
 *  error - a boolean return value, is true if some error occurred during insertion,
            is false otherwise
 */
/* -------------------------------------------------------------------------- */

#define DX_ARRAY_DELETE(array_obj, elem_type, position, capacity_mgr, error) \
    do { \
        void* _buffer_to_free = NULL; \
        elem_type* _new_elem_buffer = (array_obj).elements; \
        \
        error = false; \
        \
        if (capacity_mgr((array_obj).size - 1, &((array_obj).capacity))) { \
            _new_elem_buffer = (elem_type*)dx_calloc((array_obj).capacity, sizeof(elem_type)); \
            \
            if (_new_elem_buffer == NULL) { \
                error = true; \
                \
                break; \
            } \
            \
            memcpy((void*)_new_elem_buffer, (const void*)(array_obj).elements, position * sizeof(elem_type)); \
            \
            _buffer_to_free = (void*)(array_obj).elements; \
        } \
        \
        if (_buffer_to_free == NULL) { \
            memmove((void*)(_new_elem_buffer + position), (const void*)((array_obj).elements + position + 1), \
                    ((array_obj).size - position - 1) * sizeof(elem_type)); \
        } else { \
            memcpy((void*)(_new_elem_buffer + position), (const void*)((array_obj).elements + position + 1), \
                   ((array_obj).size - position - 1) * sizeof(elem_type)); \
            dx_free(_buffer_to_free); \
        } \
        \
        (array_obj).elements = _new_elem_buffer; \
        --((array_obj).size); \
    } while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Functions and predicates used in array operations
 */
/* -------------------------------------------------------------------------- */

#define DX_NUMERIC_COMPARATOR(l, r) (((l)>(r)?1:((l)<(r)?-1:0)))

/* -------------------------------------------------------------------------- */

bool dx_capacity_manager_halfer (size_t new_size, size_t* capacity) {
    if (new_size > *capacity) {
        *capacity = (size_t)((double)*capacity * 1.5) + 1;

        return true;
    }

    if (new_size < *capacity && new_size / *capacity < 1.5) {
        *capacity = new_size;

        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */

dx_int_t dx_symbol_name_hasher (dx_const_string_t symbol_name) {
    dx_int_t h = 0;
    size_t len = 0;
    size_t i = 0;
    
    len = wcslen(symbol_name);
    
    for (; i < len; ++i) {
        h = 5 * h + towupper(symbol_name[i]);
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
    
    return wcscmp(e1->name, e2->name);
}

/* -------------------------------------------------------------------------- */

int dx_name_symbol_comparator (dx_symbol_data_ptr_t e1, dx_symbol_data_ptr_t e2) {
    return wcscmp(e1->name, e2->name);
}

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions
 */
/* -------------------------------------------------------------------------- */

dx_symbol_data_ptr_t dx_subscribe_symbol (dx_const_string_t symbol_name, dx_subscription_data_ptr_t owner) {
    dx_symbol_data_ptr_t res = NULL;
    
    {
        dx_symbol_comparator_t comparator = dx_ciphered_symbol_comparator;
        dx_symbol_data_array_t* symbol_container = g_ciphered_symbols;
        dx_symbol_data_array_t* symbol_array_obj_ptr = NULL;
        dx_symbol_data_t dummy;

        bool symbol_exists = false;
        size_t symbol_index;
        bool failed = false;

        dummy.name = symbol_name;

        if ((dummy.cipher = dx_encode_symbol_name(symbol_name)) == 0) {
            symbol_container = g_hashed_symbols;
            comparator = dx_hashed_symbol_comparator;
            dummy.cipher = dx_symbol_name_hasher(symbol_name);
        }

        symbol_array_obj_ptr = &(symbol_container[((unsigned)dummy.cipher) % SYMBOL_BUCKET_COUNT]);

        DX_ARRAY_SEARCH(symbol_array_obj_ptr->elements, 0, symbol_array_obj_ptr->size,
                        &dummy, comparator, true, symbol_exists, symbol_index);

        if (!symbol_exists) {
            res = (dx_symbol_data_ptr_t)dx_calloc(1, sizeof(dx_symbol_data_t));

            if (res == NULL) {
                return NULL;
            }

            res->name = symbol_name;
            res->cipher = dummy.cipher;

            DX_ARRAY_INSERT(*symbol_array_obj_ptr, dx_symbol_data_ptr_t, res, symbol_index, dx_capacity_manager_halfer, failed);

            if (failed) {
                dx_free(res);
                
                return NULL;
            }
        } else {
            res = symbol_array_obj_ptr->elements[symbol_index];
        }
    }
    
    {
        bool subscr_exists = false;
        size_t subscr_index;
        bool failed = false;
        
        DX_ARRAY_SEARCH(res->subscriptions.elements, 0, res->subscriptions.size, owner, DX_NUMERIC_COMPARATOR, false,
                        subscr_exists, subscr_index);
                        
        if (subscr_exists) {
            return res;
        }
        
        DX_ARRAY_INSERT(res->subscriptions, dx_subscription_data_ptr_t, owner, subscr_index, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            return NULL;
        }
        
        ++(res->ref_count);
    }
    
    return res;
}

/* -------------------------------------------------------------------------- */

bool dx_unsubscribe_symbol (dx_symbol_data_ptr_t symbol_data, dx_subscription_data_ptr_t owner) {
    {
        bool subscr_exists = false;
        size_t subscr_index;
        bool failed = false;

        DX_ARRAY_SEARCH(symbol_data->subscriptions.elements, 0, symbol_data->subscriptions.size, owner, DX_NUMERIC_COMPARATOR, false,
                        subscr_exists, subscr_index);

        if (!subscr_exists) {
            return true;
        }

        DX_ARRAY_DELETE(symbol_data->subscriptions, dx_subscription_data_ptr_t, subscr_index, dx_capacity_manager_halfer, failed);

        if (failed) {
            return false;
        }
    }
    
    if (--(symbol_data->ref_count) == 0) {
        dx_symbol_comparator_t comparator = dx_ciphered_symbol_comparator;
        dx_symbol_data_array_t* symbol_container = g_ciphered_symbols;
        dx_symbol_data_array_t* symbol_array_obj_ptr = NULL;
        
        bool symbol_exists = false;
        size_t symbol_index;
        bool failed = false;
        
        if (dx_encode_symbol_name(symbol_data->name) == 0) {
            symbol_container = g_hashed_symbols;
            comparator = dx_hashed_symbol_comparator;
        }
        
        symbol_array_obj_ptr = &(symbol_container[((unsigned)symbol_data->cipher) % SYMBOL_BUCKET_COUNT]);
        
        DX_ARRAY_SEARCH(symbol_array_obj_ptr->elements, 0, symbol_array_obj_ptr->size,
                        symbol_data, comparator, true, symbol_exists, symbol_index);

        if (!symbol_exists) {
            dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_internal_structure_state);
            
            return false;
        }
        
        DX_ARRAY_DELETE(*symbol_array_obj_ptr, dx_symbol_data_ptr_t, symbol_index, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            return false;
        }
        
        dx_free(symbol_data);
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_listener_array (dx_listener_array_t* listeners) {
    dx_free(listeners->elements);
    
    listeners->elements = NULL;
    listeners->size = 0;
    listeners->capacity = 0;
}

/* -------------------------------------------------------------------------- */

bool dx_clear_symbol_array (dx_symbol_data_array_t* symbols, dx_subscription_data_ptr_t owner) {
    size_t symbol_index = 0;
    bool res = true;
    
    for (; symbol_index < symbols->size; ++symbol_index) {
        dx_symbol_data_ptr_t symbol_data = symbols->elements[symbol_index];
        
        res = dx_unsubscribe_symbol(symbol_data, owner) && res;
    }
    
    dx_free(symbols->elements);
    
    symbols->elements = NULL;
    symbols->size = 0;
    symbols->capacity = 0;
    
    return res;
}

/* -------------------------------------------------------------------------- */

size_t dx_find_symbol_in_array (dx_symbol_data_array_t* symbols, dx_const_string_t symbol_name, OUT bool* found) {
    dx_symbol_data_t data;
    dx_symbol_comparator_t comparator;
    size_t symbol_index;
    
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

size_t dx_find_listener_in_array (dx_listener_array_t* listeners, dx_event_listener_t listener, OUT bool* found) {
    size_t listener_index;
    
    DX_ARRAY_SEARCH(listeners->elements, 0, listeners->size, listener, DX_NUMERIC_COMPARATOR, false, *found, listener_index);
    
    return listener_index;
}

/* -------------------------------------------------------------------------- */

dx_symbol_data_ptr_t dx_find_symbol (dx_const_string_t symbol_name, dx_int_t symbol_cipher) {
    dx_symbol_data_ptr_t res = NULL;

    dx_symbol_comparator_t comparator = dx_ciphered_symbol_comparator;
    dx_symbol_data_array_t* symbol_container = g_ciphered_symbols;
    dx_symbol_data_array_t* symbol_array_obj_ptr = NULL;
    dx_symbol_data_t dummy;

    bool symbol_exists = false;
    size_t symbol_index;

    dummy.name = symbol_name;

    if ((dummy.cipher = symbol_cipher) == 0) {
        symbol_container = g_hashed_symbols;
        comparator = dx_hashed_symbol_comparator;
        dummy.cipher = dx_symbol_name_hasher(symbol_name);
    }

    symbol_array_obj_ptr = &(symbol_container[((unsigned)dummy.cipher) % SYMBOL_BUCKET_COUNT]);

    DX_ARRAY_SEARCH(symbol_array_obj_ptr->elements, 0, symbol_array_obj_ptr->size,
                    &dummy, comparator, true, symbol_exists, symbol_index);
    
    if (!symbol_exists) {
        return NULL;
    }
    
    return symbol_array_obj_ptr->elements[symbol_index];
}

/* -------------------------------------------------------------------------- */
/*
 *	Subscription functions implementation
 */
/* -------------------------------------------------------------------------- */
 
const dxf_subscription_t dx_invalid_subscription = (dxf_subscription_t)NULL;

dxf_subscription_t dx_create_event_subscription (int event_types) {
    dx_subscription_data_ptr_t subscr_data = NULL;
    
    if (event_types & DX_ET_UNUSED) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_event_type);
        
        return dx_invalid_subscription;
    }
    
    subscr_data = dx_calloc(1, sizeof(dx_subscription_data_t));
    
    if (subscr_data == NULL) {
        return dx_invalid_subscription;
    }
    
    if (g_subscription_count == 0) {
        if (!dx_mutex_create(&g_subscr_guard)) {
            dx_free(subscr_data);
            
            return dx_invalid_subscription;
        }
    }
    
    ++g_subscription_count;
    
    subscr_data->event_types = event_types;

    return (dxf_subscription_t)subscr_data;
}

/* -------------------------------------------------------------------------- */
 
bool dx_close_event_subscription (dxf_subscription_t subscr_id) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    bool res = true;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    /* locking a guard mutex */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }
    
    res = dx_clear_symbol_array(&(subscr_data->symbols), subscr_data) && res;
    dx_clear_listener_array(&(subscr_data->listeners));
    
    res = dx_mutex_unlock(&g_subscr_guard) && res;
    
    dx_free(subscr_data);
    
    if (--g_subscription_count == 0) {
        res = dx_mutex_destroy(&g_subscr_guard) && res;
    }
    
    return res;
}

/* -------------------------------------------------------------------------- */

bool dx_add_symbols (dxf_subscription_t subscr_id, dx_const_string_t* symbols, size_t symbol_count) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    size_t cur_symbol_index = 0;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }

    /* a guard mutex is required to protect the internal containers 
       from the secondary data retriever threads */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }
    
    for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
        dx_symbol_data_ptr_t symbol_data;
        size_t symbol_index;
        bool found = false;
        bool failed = false;
        
        if (symbols[cur_symbol_index] == NULL) {
            dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_symbol_name);
            
            dx_mutex_unlock(&g_subscr_guard);

            return false;
        }
        
        symbol_index = dx_find_symbol_in_array(&(subscr_data->symbols), symbols[cur_symbol_index], &found);
        
        if (found) {
            /* symbol is already subscribed */

            continue;
        }
        
        symbol_data = dx_subscribe_symbol(symbols[cur_symbol_index], subscr_data);
        
        if (symbol_data == NULL) {
            dx_mutex_unlock(&g_subscr_guard);
            
            return false;
        }
        
        DX_ARRAY_INSERT(subscr_data->symbols, dx_symbol_data_ptr_t, symbol_data, symbol_index, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            dx_mutex_unlock(&g_subscr_guard);

            return false;
        }
    }
    
    if (!dx_mutex_unlock(&g_subscr_guard)) {
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_remove_symbols (dxf_subscription_t subscr_id, dx_const_string_t* symbols, size_t symbol_count) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    size_t cur_symbol_index = 0;

    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }

    /* a guard mutex is required to protect the internal containers 
       from the secondary data retriever threads */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }
    
    for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
        size_t symbol_index;
        bool failed = false;
        bool found = false;

        if (symbols[cur_symbol_index] == NULL) {
            dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_symbol_name);

            dx_mutex_unlock(&g_subscr_guard);

            return false;
        }

        symbol_index = dx_find_symbol_in_array(&(subscr_data->symbols), symbols[cur_symbol_index], &found);
        
        if (!found) {
            /* symbol wasn't subscribed */

            continue;
        }
        
        if (!dx_unsubscribe_symbol(subscr_data->symbols.elements[symbol_index], subscr_data)) {
            dx_mutex_unlock(&g_subscr_guard);

            return false;
        }
        
        DX_ARRAY_DELETE(subscr_data->symbols, dx_symbol_data_ptr_t, symbol_index, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            dx_mutex_unlock(&g_subscr_guard);

            return false;
        }
    }
    
    if (!dx_mutex_unlock(&g_subscr_guard)) {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_add_listener (dxf_subscription_t subscr_id, dx_event_listener_t listener) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    size_t listener_index;
    bool failed;
    bool found = false;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }
    
    if (listener == NULL) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_listener);

        return false;
    }
    
    listener_index = dx_find_listener_in_array(&(subscr_data->listeners), listener, &found);
    
    if (found) {
        /* listener is already added */
        
        return true;
    }
    
    /* a guard mutex is required to protect the internal containers 
       from the secondary data retriever threads */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }
    
    DX_ARRAY_INSERT(subscr_data->listeners, dx_event_listener_t, listener, listener_index, dx_capacity_manager_halfer, failed);
    
    if (!dx_mutex_unlock(&g_subscr_guard)) {
        return false;
    }

    return !failed;
}

/* -------------------------------------------------------------------------- */

bool dx_remove_listener (dxf_subscription_t subscr_id, dx_event_listener_t listener) {
    dx_subscription_data_ptr_t subscr_data = (dx_subscription_data_ptr_t)subscr_id;
    size_t listener_index;
    bool failed;
    bool found = false;

    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_subscr_id);

        return false;
    }

    if (listener == NULL) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_listener);

        return false;
    }

    listener_index = dx_find_listener_in_array(&(subscr_data->listeners), listener, &found);
    
    if (!found) {
        /* listener isn't subscribed */

        return true;
    }

    /* a guard mutex is required to protect the internal containers 
       from the secondary data retriever threads */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }

    DX_ARRAY_DELETE(subscr_data->listeners, dx_event_listener_t, listener_index, dx_capacity_manager_halfer, failed);

    if (!dx_mutex_unlock(&g_subscr_guard)) {
        return false;
    }

    return !failed;
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

bool dx_process_event_data (int event_type, dx_const_string_t symbol_name, dx_int_t symbol_cipher,
                            const dx_event_data_t* data, int data_count) {
    dx_symbol_data_ptr_t symbol_data = NULL;
    size_t cur_subscr_index = 0;
    
    /* this function is supposed to be called from a different thread than the other
       interface functions */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }
    
    if ((symbol_data = dx_find_symbol(symbol_name, symbol_cipher)) == NULL) {
        dx_set_last_error(dx_sc_event_subscription, dx_es_invalid_internal_structure_state);
        
        dx_mutex_unlock(&g_subscr_guard);
        
        return false;
    }
    
    for (; cur_subscr_index < symbol_data->subscriptions.size; ++cur_subscr_index) {
        dx_subscription_data_ptr_t subscr_data = symbol_data->subscriptions.elements[cur_subscr_index];
        size_t cur_listener_index = 0;
        
        if (!(subscr_data->event_types & event_type)) {
            /* subscription doesn't want this specific event type */
            
            continue;
        }
        
        for (; cur_listener_index < subscr_data->listeners.size; ++cur_listener_index) {
            subscr_data->listeners.elements[cur_listener_index](event_type, symbol_name, data, data_count);
        }
    }
    
    if (!dx_mutex_unlock(&g_subscr_guard)) {
        return false;
    }
    
    return true;
}