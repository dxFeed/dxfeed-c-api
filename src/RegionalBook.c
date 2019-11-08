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

#include <math.h>

#include "ConnectionContextData.h"
#include "DXAlgorithms.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXThreads.h"
#include "EventManager.h"
#include "EventSubscription.h"
#include "ClientMessageProcessor.h"
#include "Logger.h"
#include "RegionalBook.h"

#define MAX_PRICE_LEVELS    10

struct dx_regional_book;
typedef struct dx_regional_book dx_regional_book_t;

typedef enum {
    dx_rblv_default = 1,
    dx_rblv_v2 = 2
} dx_rb_listener_version_t;

typedef void* dx_rb_listener_ptr_t;

typedef struct {
    dx_rb_listener_ptr_t listener;
    dx_rb_listener_version_t version;
	void* user_data;
} dx_rb_listener_context_t;

typedef struct {
	dx_rb_listener_context_t* elements;
	size_t size;
	size_t capacity;
} dx_rb_listener_array_t;

typedef struct {
	dxf_price_level_element_t bid;
	dxf_price_level_element_t ask;
} dx_rb_regional_t;

struct dx_regional_book {
	dx_mutex_t guard;

	dxf_string_t symbol;
	dxf_subscription_t subscription;

	dx_rb_listener_array_t listeners;

	/* Always dx_all_regionals_count */
	dx_rb_regional_t *regions;
	dxf_price_level_book_data_t book;
	void *context;
};

typedef struct {
	dxf_connection_t connection;
	dx_mutex_t guard;
} dx_rb_connection_context_t;

#define CTX(context) \
	((dx_rb_connection_context_t*)context)

/************************/
/* Forward declarations */
/************************/
static bool dx_rb_clear_connection_context(dx_rb_connection_context_t* context);
static bool dx_rb_book_free(dx_regional_book_t *book);

static void rb_event_listener(int event_type, dxf_const_string_t symbol_name,
	const dxf_event_data_t* data, int data_count,
	const dxf_event_params_t* event_params, void* user_data);

/********************************/
/* Conection Context Management */
/********************************/

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_regional_book) {
	dx_rb_connection_context_t* context = NULL;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	context = dx_calloc(1, sizeof(dx_rb_connection_context_t));

	if (context == NULL) {
		return false;
	}

	context->connection = connection;

	if (!dx_mutex_create(&context->guard)) {
		dx_rb_clear_connection_context(context);
		return false;
	}

	if (!dx_set_subsystem_data(connection, dx_ccs_regional_book, context)) {
		dx_rb_clear_connection_context(context);
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_regional_book) {
	bool res = true;
	dx_rb_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_regional_book, &res);
	if (context == NULL) {
		return res;
	}
	res = dx_rb_clear_connection_context(context);
	return res;
}

/* -------------------------------------------------------------------------- */
static bool dx_rb_clear_connection_context(dx_rb_connection_context_t* context) {
	bool res = true;
	res &= dx_mutex_destroy(&(context->guard));
	dx_free(context);
	return res;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_regional_book) {
	return true;
}

/*********************************************/
/* Comparators for different data structures */
/*********************************************/

static inline int dx_rb_listener_comparator(dx_rb_listener_context_t e1, dx_rb_listener_context_t e2) {
	return DX_NUMERIC_COMPARATOR(e1.listener, e2.listener);
}

/*****************************/
/* Book management functions */
/*****************************/

static bool dx_rb_book_free(dx_regional_book_t *book) {
	CHECKED_FREE(book->symbol);
	CHECKED_FREE(book->regions);
	CHECKED_FREE(book->book.bids);
	CHECKED_FREE(book->book.asks);
	dx_mutex_destroy(&book->guard);
	CHECKED_FREE(book);
	return true;
}

/* -------------------------------------------------------------------------- */

/* This functuions must be called without guard */
static void dx_rb_book_clear(dx_regional_book_t *book) {
	/* Kill subscription */
	if (book->subscription != NULL)
		dxf_close_subscription(book->subscription);
	/* Now nothing could call book methods, kill it */
	dx_rb_book_free(book);

}

/* -------------------------------------------------------------------------- */

/* This functuions must be called with book guard taken */
static bool dx_rb_book_update_one_side(dxf_price_level_element_t *dst, size_t *dst_count, dx_regional_book_t *book, bool isBid, double startValue) {
	bool changed = false;
	size_t didx = 0;
	dxf_ulong_t rused = 0;
	dxf_price_level_element_t *reg;
	size_t i;
	dxf_price_level_element_t best;

	/* It is merge-sort of src_count sources */
	for (; didx < *dst_count; didx++) {
		/* First of all: find, do we have any input data? */
		best.price = startValue;
		best.size = 0;
		best.time = 0;
		for (i = 0; i < dx_all_regional_count; i++) {
			/* Skip seen regions and not used regions */
			if ((rused & (1ULL << i)) != 0)
				continue;

			/* Get bids or asks */
			reg = isBid ? &book->regions[i].bid : &book->regions[i].ask;
			/* Skip, if size is zero or ptice is nan */
			if (reg->size == 0 || isnan(reg->price)) {
				rused |= 1ULL << i;
				continue;
			}

			/* Ok, is price is same as best, add size */
			if (reg->price == best.price) {
				best.size += reg->size;
				rused |= 1ULL << i;
			}
			else if (isBid && best.price < reg->price) {
				best = *reg;
				rused |= 1ULL << i;
			}
			else if (!isBid && best.price > reg->price) {
				best = *reg;
				rused |= 1ULL << i;
			}
		}
		/* All sources are gone */
		if (best.size == 0)
			break;
		if (memcmp(&dst[didx], &best, sizeof(best)) != 0) {
			dst[didx] = best;
			changed = true;
		}
	}
	/* Fix size */
	*dst_count = didx;
	return changed;
}

/* -------------------------------------------------------------------------- */

/* This functuions must be called without book guard */
static void dx_rb_book_update(dx_regional_book_t *book) {
	bool changed = false;
	size_t i = 0;

	book->book.bids_count = MAX_PRICE_LEVELS;
	changed |= dx_rb_book_update_one_side((dxf_price_level_element_t*)book->book.bids, &book->book.bids_count, book, true, 0.0);
	book->book.asks_count = MAX_PRICE_LEVELS;
	changed |= dx_rb_book_update_one_side((dxf_price_level_element_t*)book->book.asks, &book->book.asks_count, book, false, 1.0E38);

	if (changed) {
		if (!dx_mutex_lock(&book->guard)) {
			return;
		}
        for (; i < book->listeners.size; i++) {
            dx_rb_listener_context_t* ctx = &book->listeners.elements[i];
            if (ctx->version == dx_rblv_default) {
                ((dxf_price_level_book_listener_t)ctx->listener)(&book->book, book->listeners.elements[i].user_data);
            }
        }
		dx_mutex_unlock(&book->guard);
	}
}

static void notify_regional_listeners(dx_regional_book_t *book, const dxf_quote_t *quotes, int count)
{
    size_t i = 0;
    if (!dx_mutex_lock(&book->guard)) {
        return;
    }    
    for (i = 0; i < book->listeners.size; ++i) {
        dx_rb_listener_context_t* ctx = &book->listeners.elements[i];
        if (ctx->version == dx_rblv_v2) {
            ((dxf_regional_quote_listener_t)ctx->listener)(book->symbol, quotes, count, ctx->user_data);
        }
    }
    dx_mutex_unlock(&book->guard);
}

/******************/
/* EVENT LISTENER */
/******************/

/*
This is called with subscription lock, so it is imposisble to delete source
when this code is executing.
 */
static void rb_event_listener(int event_type, dxf_const_string_t symbol_name,
							const dxf_event_data_t* data, int data_count,
							const dxf_event_params_t* event_params, void* user_data) {
	const dxf_quote_t *quote = (const dxf_quote_t *)data;
	dx_regional_book_t *book = (dx_regional_book_t *)user_data;
	int i = 0;

	/* Check params */
	if (event_type != DXF_ET_QUOTE) {
		dx_logging_error(L"Listener for Regional Book was called with wrong event type\n");
		return;
	}

	if (dx_compare_strings(book->symbol, symbol_name) != 0) {
		dx_logging_error(L"Listener for Regional Book was called with wrong symbol\n");
		return;
	}

    notify_regional_listeners(book, quote, data_count);

	/* Ok, process data */
	for (; i < data_count; i++) {
		if (quote[i].ask_exchange_code >= 'A' && quote[i].ask_exchange_code <= 'Z') {
			int idx = quote[i].ask_exchange_code - 'A';
			book->regions[idx].ask.price = quote[i].ask_price;
			book->regions[idx].ask.size = quote[i].ask_size;
			book->regions[idx].ask.time = quote[i].ask_time;
		}
		if (quote[i].bid_exchange_code >= 'A' && quote[i].bid_exchange_code <= 'Z') {
			int idx = quote[i].bid_exchange_code - 'A';
			book->regions[idx].bid.price = quote[i].bid_price;
			book->regions[idx].bid.size = quote[i].bid_size;
			book->regions[idx].bid.time = quote[i].bid_time;
		}
	}

	dx_rb_book_update(book);
}

/*******/
/* API */
/*******/

dxf_regional_book_t dx_create_regional_book(dxf_connection_t connection,
	dxf_const_string_t symbol) {
	bool res = true;
	dx_rb_connection_context_t *context = NULL;
	dx_regional_book_t *book = NULL;
	int i = 0;

	context = dx_get_subsystem_data(connection, dx_ccs_regional_book, &res);
	if (context == NULL) {
		return NULL;
	}

	book = dx_calloc(1, sizeof(dx_regional_book_t));
	if (book == NULL) {
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}
	if (!dx_mutex_create(&book->guard)) {
		dx_free(book);
		return NULL;
	}

	book->regions = dx_calloc(dx_all_regional_count, sizeof(*book->regions));
	if (book->regions == NULL) {
		dx_rb_book_free(book);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}

	book->context = context;

	book->symbol = dx_create_string_src(symbol);
	if (book->symbol == NULL) {
		dx_rb_book_free(book);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}

	book->book.symbol = book->symbol;
	book->book.asks = dx_calloc(MAX_PRICE_LEVELS, sizeof(*book->book.asks));
	if (book->book.asks == NULL) {
		dx_rb_book_free(book);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}
	book->book.bids = dx_calloc(MAX_PRICE_LEVELS, sizeof(*book->book.bids));
	if (book->book.bids == NULL) {
		dx_rb_book_free(book);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}

	/* Create subscription which we need, for needed regions */
	if ((book->subscription = dx_create_event_subscription(connection, DXF_ET_QUOTE, dx_esf_quotes_regional, 0)) == dx_invalid_subscription) {
		dx_rb_book_free(book);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}

	/* Attach event listener */
	if (!dxf_attach_event_listener_v2(book->subscription, &rb_event_listener, book)) {
		dx_rb_book_free(book);
		return NULL;
	}

	/* Add symbol */
	if (!dx_add_symbols(book->subscription, &symbol, 1)) {
		dx_rb_book_free(book);
		return NULL;
	}

	/* And make all these motions */
	if (!dx_load_events_for_subscription(connection, dx_get_order_source(book->subscription), DXF_ET_QUOTE, dx_esf_quotes_regional) ||
		!dx_send_record_description(connection, false) ||
		!dx_subscribe_symbols_to_events(connection, dx_get_order_source(book->subscription),
			&symbol, 1, DXF_ET_QUOTE, false, false, dx_esf_quotes_regional, 0)) {
		dx_rb_book_free(book);
		return NULL;
	}

	return book;
}

bool dx_close_regional_book(dxf_regional_book_t book) {
	dx_rb_book_clear(book);
	return true;
}

static bool dx_add_regional_book_listener_impl(dxf_regional_book_t book,
                                    dx_rb_listener_ptr_t listener,
                                    dx_rb_listener_version_t version,
									void *user_data) {
	dx_regional_book_t *b = (dx_regional_book_t *)book;
	dx_rb_connection_context_t *context = CTX(b->context);
	dx_rb_listener_context_t ctx = { listener, version, user_data };
	bool found = false;
	bool error = false;
	size_t idx;

	DX_ARRAY_SEARCH(b->listeners.elements, 0, b->listeners.size, ctx, dx_rb_listener_comparator, false, found, idx);

	CHECKED_CALL(dx_mutex_lock, &(b->guard));
	if (found) {
		b->listeners.elements[idx].user_data = user_data;
	} else {
		DX_ARRAY_INSERT(b->listeners, dx_rb_listener_context_t, ctx, idx, dx_capacity_manager_halfer, error);
	}
	return dx_mutex_unlock(&b->guard) && !error;
}

bool dx_add_regional_book_listener(dxf_regional_book_t book, dxf_price_level_book_listener_t book_listener, void *user_data)
{
    return dx_add_regional_book_listener_impl(book, (dx_rb_listener_ptr_t)book_listener, dx_rblv_default, user_data);
}

bool dx_add_regional_book_listener_v2(dxf_regional_book_t book, dxf_regional_quote_listener_t book_listener,
    void *user_data)
{
    return dx_add_regional_book_listener_impl(book, (dx_rb_listener_ptr_t)book_listener, dx_rblv_v2, user_data);
}

static bool dx_remove_regional_book_listener_impl(dxf_regional_book_t book, dx_rb_listener_ptr_t listener)
{
	dx_regional_book_t *b = (dx_regional_book_t *)book;
	dx_rb_connection_context_t *context = CTX(b->context);
	dx_rb_listener_context_t ctx = { listener, 0, NULL };
	bool found = false;
	bool error = false;
	size_t idx;

	DX_ARRAY_SEARCH(b->listeners.elements, 0, b->listeners.size, ctx, dx_rb_listener_comparator, false, found, idx);
	if (!found) {
		return true;
	}

	CHECKED_CALL(dx_mutex_lock, &(b->guard));
	DX_ARRAY_DELETE(b->listeners, dx_rb_listener_context_t, idx, dx_capacity_manager_halfer, error);
	return dx_mutex_unlock(&b->guard) && !error;
}

bool dx_remove_regional_book_listener(dxf_regional_book_t book, dxf_price_level_book_listener_t listener)
{
    return dx_remove_regional_book_listener_impl(book, (dx_rb_listener_ptr_t)listener);
}

bool dx_remove_regional_book_listener_v2(dxf_regional_book_t book, dxf_regional_quote_listener_t listener)
{
    return dx_remove_regional_book_listener_impl(book, (dx_rb_listener_ptr_t)listener);
}
