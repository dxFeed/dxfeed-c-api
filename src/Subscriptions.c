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
 
#include "Subscriptions.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include "DXMemory.h"
#include "DXThreads.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*
 *	Subscription error codes
 */
/* -------------------------------------------------------------------------- */

const dx_subscription_t dx_invalid_subscription = (dx_subscription_t)NULL;

/* -------------------------------------------------------------------------- */
/*
 *	Internal data structures and objects
 */
/* -------------------------------------------------------------------------- */

struct dx_subscription_data_t;

struct dx_subscr_list_item_t {
    struct dx_subscription_data_t* data;
    struct dx_subscr_list_item_t* next;
};

struct dx_symbol_data_t {
    const char* name;
    int code;
    size_t ref_count;
    struct dx_subscr_list_item_t* subscriptions;
};

struct dx_listener_list_item_t {
    dx_event_listener_t listener;
    struct dx_listener_list_item_t* next;
};

struct dx_symbol_list_item_t {
    struct dx_symbol_data_t* data;
    struct dx_symbol_list_item_t* next;
};

struct dx_subscription_data_t {
    int event_types;
    struct dx_symbol_list_item_t* symbols;
    struct dx_listener_list_item_t* listeners;
    struct dx_listener_list_item_t* generic_listeners;
};

static size_t g_subscription_count = 0;
static pthread_mutex_t g_subscr_guard;

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions
 */
/* -------------------------------------------------------------------------- */

struct dx_symbol_data_t* dx_subscribe_symbol (const char* symbol_name, struct dx_subscription_data_t* owner) {
    return NULL;
}

/* -------------------------------------------------------------------------- */

bool dx_unsubscribe_symbol (struct dx_symbol_data_t* symbol_data, struct dx_subscription_data_t* owner) {
    return false;
}

/* -------------------------------------------------------------------------- */

void dx_delete_listener_list (struct dx_listener_list_item_t* listeners) {
    struct dx_listener_list_item_t* cur_list_item = listeners;
    
    while (cur_list_item != NULL) {
        struct dx_listener_list_item_t* next_list_item = cur_list_item->next;
        
        dx_free((void*)cur_list_item);
        
        cur_list_item = next_list_item;
    }
}

/* -------------------------------------------------------------------------- */

bool dx_delete_symbol_list (struct dx_symbol_list_item_t* symbols, struct dx_subscription_data_t* owner) {
    struct dx_symbol_list_item_t* cur_list_item = symbols;
    bool res = true;
    
    while (cur_list_item != NULL) {
        struct dx_symbol_list_item_t* next_list_item = cur_list_item->next;
        
        res = dx_unsubscribe_symbol(cur_list_item->data, owner) && res;
        dx_free(cur_list_item);
        
        cur_list_item = next_list_item;
    }
    
    return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	dx_symbol_list_item_t* - the list tail - object
 *  dx_symbol_list_item_t** - a pointer to the list tail - object too - never NULL
 *  dx_symbol_list_item_t*** - address of the list tail pointer - not object - never NULL
 */

bool dx_add_symbol_data_to_list (struct dx_symbol_list_item_t*** cur_list_end, struct dx_symbol_data_t* symbol_data) {
    struct dx_symbol_list_item_t* new_list_item;
    
    if (cur_list_end == NULL || /* nonsense */
        *cur_list_end == NULL || /* nonsense */
        **cur_list_end != NULL && (**cur_list_end)->next != NULL) { /* the given object is not a list tail */
        
        dx_set_last_error(dx_sc_subscription, dx_sub_invalid_internal_structure_state);
        
        return false;
    }
    
    new_list_item = (struct dx_symbol_list_item_t*)dx_calloc(1, sizeof(struct dx_symbol_list_item_t));
    
    if (new_list_item == NULL) {
        return false;
    }
    
    new_list_item->data = symbol_data;
    new_list_item->next = NULL;
    
    if (**cur_list_end == NULL) {
        /* the case of previously empty list */
        
        **cur_list_end = new_list_item;
    } else {
        (**cur_list_end)->next = new_list_item;
        *cur_list_end = &((**cur_list_end)->next);
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

extern int dx_encode_symbol_name (const char* symbol_name);

bool dx_symbol_code_comparator (const struct dx_symbol_data_t* t1, const struct dx_symbol_data_t* t2) {
    return (t1->code == t2->code);
}

bool dx_symbol_name_comparator (const struct dx_symbol_data_t* t1, const struct dx_symbol_data_t* t2) {
    return (strcmp(t1->name, t2->name) == 0);
}

typedef bool (*dx_symbol_comparator_t)(const struct dx_symbol_data_t*, const struct dx_symbol_data_t*);

static struct dx_symbol_list_item_t* const g_invalid_symbol_list_item = (struct dx_symbol_list_item_t*)-1;

struct dx_symbol_list_item_t* dx_find_symbol_in_list (struct dx_symbol_list_item_t* symbol_list,
                                                      const char* symbol_name) {
    struct dx_symbol_list_item_t* cur_item = symbol_list;
    struct dx_symbol_list_item_t* prev_item = NULL;
    dx_symbol_comparator_t comparator;
    struct dx_symbol_data_t data;
    
    data.name = symbol_name;
    
    if ((data.code = dx_encode_symbol_name(symbol_name)) != 0) {
        comparator = dx_symbol_code_comparator;
    } else {
        comparator = dx_symbol_name_comparator;
    }
    
    for (; cur_item != NULL; cur_item = cur_item->next) {
        if (comparator(cur_item->data, &data)) {
            return prev_item;
        }
        
        prev_item = cur_item;
    }
    
    return g_invalid_symbol_list_item;
}

/* -------------------------------------------------------------------------- */

static struct dx_listener_list_item_t* const g_invalid_listener_list_item = (struct dx_listener_list_item_t*)-1;

struct dx_listener_list_item_t* dx_find_listener_in_list (struct dx_listener_list_item_t* listener_list,
                                                          dx_event_listener_t listener) {
    struct dx_listener_list_item_t* cur_item = listener_list;
    struct dx_listener_list_item_t* prev_item = NULL;
    
    for (; cur_item != NULL; cur_item = cur_item->next) {
        if (cur_item->listener == listener) {
            return prev_item;
        }
        
        prev_item = cur_item;
    }

    return g_invalid_listener_list_item;
}

/* -------------------------------------------------------------------------- */
/*
 *	Subscription functions implementation
 */
/* -------------------------------------------------------------------------- */
 
dx_subscription_t dx_create_subscription (int event_types) {
    struct dx_subscription_data_t* subscr_data = NULL;
    
    if (event_types != dx_et_all && event_types & dx_et_unused) {
        dx_set_last_error(dx_sc_subscription, dx_sub_invalid_event_type);
        
        return dx_invalid_subscription;
    }
    
    subscr_data = dx_calloc(1, sizeof(struct dx_subscription_data_t));
    
    if (subscr_data == NULL) {
        return dx_invalid_subscription;
    }
    
    if (g_subscription_count == 0) {
        if (!dx_mutex_create(&g_subscr_guard)) {
            dx_free(subscr_data);
            
            return dx_invalid_subscription;
        }
        
        ++g_subscription_count;
    }
    
    subscr_data->event_types = event_types;
    subscr_data->symbols = NULL;
    subscr_data->listeners = NULL;
    subscr_data->generic_listeners = NULL;
    
    return (dx_subscription_t)subscr_data;
}

/* -------------------------------------------------------------------------- */
 
bool dx_close_subscription (dx_subscription_t subscr_id) {
    struct dx_subscription_data_t* subscr_data = (struct dx_subscription_data_t*)subscr_id;
    bool res = true;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_subscription, dx_sub_invalid_subscr_id);

        return false;
    }
    
    /* locking a guard mutex */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }
    
    res = dx_delete_symbol_list(subscr_data->symbols, subscr_data) && res;
    dx_delete_listener_list(subscr_data->listeners);
    dx_delete_listener_list(subscr_data->generic_listeners);
    
    res = dx_mutex_unlock(&g_subscr_guard) && res;
    
    dx_free(subscr_data);
    
    if (--g_subscription_count == 0) {
        res = dx_mutex_destroy(&g_subscr_guard) && res;
    }
    
    return res;
}

/* -------------------------------------------------------------------------- */

bool dx_add_symbols (dx_subscription_t subscr_id, const char** symbols, size_t symbol_count) {
    struct dx_subscription_data_t* subscr_data = (struct dx_subscription_data_t*)subscr_id;
    size_t cur_symbol_index = 0;
    struct dx_symbol_list_item_t** cur_list_end = NULL;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_subscription, dx_sub_invalid_subscr_id);

        return false;
    }

    /* a guard mutex is required to protect the internal containers 
       from the secondary data retriever threads */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }
    
    /* traversing to the end of the list */
    for (cur_list_end = &(subscr_data->symbols);
         *cur_list_end != NULL && (*cur_list_end)->next != NULL;
         cur_list_end = &((*cur_list_end)->next));
    
    for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
        struct dx_symbol_data_t* symbol_data;
        
        if (symbols[cur_symbol_index] == NULL) {
            dx_set_last_error(dx_sc_subscription, dx_sub_invalid_symbol_name);
            
            dx_mutex_unlock(&g_subscr_guard);

            return false;
        }
        
        if (dx_find_symbol_in_list(subscr_data->symbols, symbols[cur_symbol_index]) != g_invalid_symbol_list_item) {
            /* symbol is already subscribed */
            
            continue;
        }
        
        symbol_data = dx_subscribe_symbol(symbols[cur_symbol_index], subscr_data);
        
        if (symbol_data == NULL) {
            dx_mutex_unlock(&g_subscr_guard);
            
            return false;
        }
        
        if (!dx_add_symbol_data_to_list(&cur_list_end, symbol_data)) {
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

bool dx_remove_symbols (dx_subscription_t subscr_id, const char** symbols, size_t symbol_count) {
    struct dx_subscription_data_t* subscr_data = (struct dx_subscription_data_t*)subscr_id;
    size_t cur_symbol_index = 0;
    struct dx_symbol_list_item_t* cur_list_end = NULL;

    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_subscription, dx_sub_invalid_subscr_id);

        return false;
    }

    /* a guard mutex is required to protect the internal containers 
    from the secondary data retriever threads */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }
    
    for (; cur_symbol_index < symbol_count; ++cur_symbol_index) {
        struct dx_symbol_list_item_t* cur_list_item = NULL;
        struct dx_symbol_list_item_t* prev_list_item = NULL;
        
        if (symbols[cur_symbol_index] == NULL) {
            dx_set_last_error(dx_sc_subscription, dx_sub_invalid_symbol_name);
            
            dx_mutex_unlock(&g_subscr_guard);
            
            return false;
        }
        
        if ((prev_list_item = dx_find_symbol_in_list(subscr_data->symbols, symbols[cur_symbol_index])) == g_invalid_symbol_list_item) {
            /* symbol wasn't found, simply skipping it */
                           
            continue;
        }
        
        if (prev_list_item == NULL) {
            cur_list_item = subscr_data->symbols;
        } else {
            cur_list_item = prev_list_item->next;
        }
        
        if (!dx_unsubscribe_symbol(cur_list_item->data, subscr_data)) {
            dx_mutex_unlock(&g_subscr_guard);

            return false;
        }
        
        if (prev_list_item == NULL) {
            subscr_data->symbols = cur_list_item->next;
        } else {
            prev_list_item->next = cur_list_item->next;
        }
        
        dx_free(cur_list_item);
    }
    
    if (!dx_mutex_unlock(&g_subscr_guard)) {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_add_listener (dx_subscription_t subscr_id, dx_event_listener_t listener, enum dx_event_type_t event_type) {
    struct dx_subscription_data_t* subscr_data = (struct dx_subscription_data_t*)subscr_id;
    struct dx_listener_list_item_t** cur_list_item = NULL;
    struct dx_listener_list_item_t* new_list_item = NULL;
    
    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_subscription, dx_sub_invalid_subscr_id);

        return false;
    }
    
    if (listener == NULL) {
        dx_set_last_error(dx_sc_subscription, dx_sub_invalid_listener);

        return false;
    }
    
    if (event_type == dx_et_all) {
        cur_list_item = &(subscr_data->generic_listeners);
    } else {
        if (event_type & dx_et_unused || event_type != subscr_data->event_types) {
            dx_set_last_error(dx_sc_subscription, dx_sub_listener_event_type_incompatible);
            
            return false;
        }
        
        cur_list_item = &(subscr_data->listeners);
    }
    
    if (*cur_list_item != NULL) {
        for (;;) {
            if ((*cur_list_item)->listener == listener) {
                /* the listener already added, finishing... */

                return true;
            }
            
            if ((*cur_list_item)->next == NULL) {
                /* the last item in list, and it didn't match */
                
                break;
            }
            
            cur_list_item = &((*cur_list_item)->next);
        }
    }
    
    new_list_item = (struct dx_listener_list_item_t*)dx_calloc(1, sizeof(struct dx_listener_list_item_t));
    
    if (new_list_item == NULL) {
        return false;
    }
    
    new_list_item->listener = listener;
    new_list_item->next = NULL;
    
    /* a guard mutex is required to protect the internal containers 
    from the secondary data retriever threads */
    if (!dx_mutex_lock(&g_subscr_guard)) {
        dx_free(new_list_item);
        
        return false;
    }
    
    if (*cur_list_item == NULL) {
        *cur_list_item = new_list_item;
    } else {
        (*cur_list_item)->next = new_list_item;
        cur_list_item = &((*cur_list_item)->next);
    }
    
    if (!dx_mutex_unlock(&g_subscr_guard)) {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */

bool dx_remove_listener (dx_subscription_t subscr_id, dx_event_listener_t listener) {
    struct dx_subscription_data_t* subscr_data = (struct dx_subscription_data_t*)subscr_id;
    struct dx_listener_list_item_t** listener_list[2];
    size_t listener_index = 0;
    bool found = false;

    if (subscr_id == dx_invalid_subscription) {
        dx_set_last_error(dx_sc_subscription, dx_sub_invalid_subscr_id);

        return false;
    }

    if (listener == NULL) {
        dx_set_last_error(dx_sc_subscription, dx_sub_invalid_listener);

        return false;
    }
    
    listener_list[0] = &(subscr_data->listeners);
    listener_list[1] = &(subscr_data->generic_listeners);
    
    if (!dx_mutex_lock(&g_subscr_guard)) {
        return false;
    }
    
    for (; listener_index < 2; ++listener_index) {
        struct dx_listener_list_item_t* cur_list_item = NULL;
        struct dx_listener_list_item_t* prev_list_item = NULL;
        
        prev_list_item = dx_find_listener_in_list(*listener_list[listener_index], listener);
        
        if (prev_list_item == g_invalid_listener_list_item) {
            /* listener wasn't found */
            
            continue;
        }
        
        found = true;
        
        if (prev_list_item == NULL) {
            /* the listener is the list's head */
            
            cur_list_item = *listener_list[listener_index];
            *listener_list[listener_index] = cur_list_item->next;
        } else {
            cur_list_item = prev_list_item->next;
            prev_list_item->next = cur_list_item->next;
        }
        
        dx_free(cur_list_item);
    }
    
    if (!dx_mutex_unlock(&g_subscr_guard)) {
        return false;
    }
    
    if (!found) {
        dx_set_last_error(dx_sc_subscription, dx_sub_listener_not_subscribed);
        
        return false;
    }
    
    return true;
}