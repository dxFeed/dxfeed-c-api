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
#include <string.h>

#include "ConnectionContextData.h"
#include "DXAlgorithms.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXThreads.h"
#include "EventManager.h"
#include "EventSubscription.h"
#include "ClientMessageProcessor.h"
#include "Logger.h"
#include "PriceLevelBook.h"

#define MAX_PRICE_LEVELS	10

struct dx_price_level_book;
typedef struct dx_price_level_book dx_price_level_book_t;

typedef enum {
	dx_status_unknown = 0,
	dx_status_begin,
	dx_status_full,
	dx_status_pending
} dx_plb_status_t;

typedef struct {
	dxf_price_level_book_listener_t listener;
	void* user_data;
} dx_plb_listener_context_t;

typedef struct {
	dx_plb_listener_context_t* elements;
	size_t size;
	size_t capacity;
} dx_plb_listener_array_t;

typedef struct {
	dxf_order_t **elements;
	size_t size;
	size_t capacity;
} dx_plb_records_array_t;

typedef struct dx_plb_source_consumer {
	struct dx_plb_source_consumer *next;

	dx_price_level_book_t *consumer;
	int sourceidx;
} dx_plb_source_consumer_t;

typedef struct {
	size_t count;
	dxf_price_level_element_t levels[MAX_PRICE_LEVELS];
	int (*cmp)(dxf_price_level_element_t p1, dxf_price_level_element_t p2);
	bool rebuild;
	bool updated;
} dx_plb_price_level_side_t;

typedef struct {
	dx_mutex_t guard;

	dxf_ulong_t hash;
	dxf_string_t symbol;
	dxf_char_t source[DXF_RECORD_SUFFIX_SIZE];

	dxf_subscription_t subscription;

	dx_plb_records_array_t snapshot;
	dx_plb_status_t snapshot_status;

	dx_plb_source_consumer_t *consumers;

	dx_plb_price_level_side_t bids;
	dx_plb_price_level_side_t final_bids;
	dx_plb_price_level_side_t asks;
	dx_plb_price_level_side_t final_asks;
} dx_plb_source_t;

struct dx_price_level_book {
	dx_mutex_t guard;

	dxf_string_t symbol;

	size_t sources_count;
	dx_plb_source_t **sources;
	dx_plb_listener_array_t listeners;

	/* Aliases for source's data structures */
	dx_plb_price_level_side_t **src_bids;
	dx_plb_price_level_side_t **src_asks;

	dx_plb_price_level_side_t bids;
	dx_plb_price_level_side_t asks;

	/* Public alias for our own data structure */
	dxf_price_level_book_data_t book;

	void *context;
};

typedef struct {
	dxf_connection_t connection;
	dx_mutex_t guard;

	/* Hash table of all sources by symbol & source */
	dx_plb_source_t **sources;
	dxf_int_t src_size;
	dxf_int_t src_capacity;
} dx_plb_connection_context_t;

#define CTX(context) \
	((dx_plb_connection_context_t*)context)

/************************/
/* Forward declarations */
/************************/
static bool dx_plb_clear_connection_context(dx_plb_connection_context_t* context);
static bool dx_plb_source_clear(dx_plb_source_t *source, bool force);
static size_t dx_plb_source_find_pos(dx_plb_connection_context_t* context, dxf_const_string_t symbol, dxf_const_string_t src);
static dxf_ulong_t dx_plb_source_hash(dxf_const_string_t symbol, dxf_const_string_t src);
static bool dx_plb_book_free(dx_price_level_book_t *book);

static void plb_event_listener(int event_type, dxf_const_string_t symbol_name,
	const dxf_event_data_t* data, int data_count,
	const dxf_event_params_t* event_params, void* user_data);

/********************************/
/* Conection Context Management */
/********************************/

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_price_level_book) {
	dx_plb_connection_context_t* context = NULL;

	CHECKED_CALL_2(dx_validate_connection_handle, connection, true);

	context = dx_calloc(1, sizeof(dx_plb_connection_context_t));

	if (context == NULL) {
		return false;
	}

	context->src_capacity = 1024;
	context->sources = dx_calloc(context->src_capacity, sizeof(context->sources[0]));
	if (context->sources == NULL) {
		dx_plb_clear_connection_context(context);
		return false;
	}

	context->connection = connection;

	if (!dx_mutex_create(&context->guard)) {
		dx_plb_clear_connection_context(context);
		return false;
	}

	if (!dx_set_subsystem_data(connection, dx_ccs_price_level_book, context)) {
		dx_plb_clear_connection_context(context);
		return false;
	}

	return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_price_level_book) {
	bool res = true;
	dx_plb_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_price_level_book, &res);
	if (context == NULL) {
		return res;
	}
	res = dx_plb_clear_connection_context(context);
	return res;
}

/* -------------------------------------------------------------------------- */
static bool dx_plb_clear_connection_context(dx_plb_connection_context_t* context) {
	bool res = true;
	int i = 0;
	if (context->sources != NULL) {
		for (; i < context->src_size; i++) {
			if (context->sources[i] != NULL)
				res &= dx_plb_source_clear(context->sources[i], true);
		}
		dx_free(context->sources);
	}
	res &= dx_mutex_destroy(&(context->guard));
	dx_free(context);
	return res;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_price_level_book) {
	return true;
}

/* -------------------------------------------------------------------------- */

/* This function must be called with context guard taken */
static bool dx_plb_ctx_grow_sources_hashtable(dx_plb_connection_context_t* context) {
	dx_plb_source_t **newArray;
	dx_plb_source_t **oldArray;
	size_t oldSize;
	size_t pos;
	size_t i = 0;

	newArray = dx_calloc(context->src_capacity * 2, sizeof(newArray[0]));
	if (newArray == NULL) {
		dx_set_error_code(dx_mec_insufficient_memory);
		return false;
	}
	oldSize = context->src_capacity;
	oldArray = context->sources;

	context->src_capacity *= 2;
	context->sources = newArray;

	/* Add all sources */
	for (i = 0; i < oldSize; i++) {
		pos = dx_plb_source_find_pos(context, oldArray[i]->symbol, oldArray[i]->source);
		context->sources[pos] = oldArray[i];
	}

	return true;
}

/* -------------------------------------------------------------------------- */

/* This function must be called with context guard taken */
static dx_plb_source_t *dx_plb_ctx_find_source(dx_plb_connection_context_t* context, dxf_const_string_t symbol, dxf_const_string_t src) {
	size_t pos = dx_plb_source_find_pos(context, symbol, src);
	return context->sources[pos];
}

/* -------------------------------------------------------------------------- */

/* This function must be called with context guard taken */
static bool dx_plb_ctx_add_source(dx_plb_connection_context_t* context, dx_plb_source_t *source) {
	/* Resize hash table if it is half-full */
	size_t pos = dx_plb_source_find_pos(context, source->symbol, source->source);
	if (context->src_size > context->src_capacity / 2) {
		if (!dx_plb_ctx_grow_sources_hashtable(context)) {
			return false;
		}
		pos = dx_plb_source_find_pos(context, source->symbol, source->source);
	}
	context->sources[pos] = source;
	context->src_size++;
	return true;
}

/* -------------------------------------------------------------------------- */

/* This function must be called with context guard taken */
static bool dx_plb_ctx_remove_source(dx_plb_connection_context_t* context, dx_plb_source_t *source) {
	size_t pos = dx_plb_source_find_pos(context, source->symbol, source->source);
	size_t npos;
	size_t tpos;

	if (context->sources[pos] != source) {
		dx_logging_error(L"PLB Internal error: source could not be found on removal\n");
		return false;
	}

	/* Open Addressing Hash removal */
	npos = pos;
	while (true) {
		npos = (npos + 1) % context->src_capacity;
		if (context->sources[npos] == NULL)
			break;
		tpos = dx_plb_source_hash(context->sources[npos]->symbol, context->sources[npos]->source) % context->src_capacity;
		if ((npos > pos && (tpos <= pos || tpos > npos))
			|| (npos < pos && (tpos <= pos && tpos > npos))) {
			context->sources[pos] = context->sources[npos];
			pos = npos;
		}
	}
	context->sources[pos] = NULL;
	context->src_size--;
	return true;
}

/* -------------------------------------------------------------------------- */

/* This function must be called with context guard taken */
static void dx_plb_ctx_cleanup_sources(dx_plb_connection_context_t* context) {
	int i = 0;
	dx_plb_source_t *source;
	while (i < context->src_capacity) {
		if (context->sources[i] == NULL) {
			i++;
			continue;
		}
		if (context->sources[i]->consumers == NULL) {
			source = context->sources[i];
			dx_plb_ctx_remove_source(context, source);
			dx_plb_source_clear(source, false);
		} else {
			i++;
		}
	}
}

/*********************************************/
/* Comparators for different data structures */
/*********************************************/

static inline int dx_plb_order_comparator(const dxf_order_t *o1, const dxf_order_t *o2) {
	return DX_NUMERIC_COMPARATOR(o1->index, o2->index);
}

static inline int dx_plb_pricelvel_comparator_ask(const dxf_price_level_element_t p1, dxf_price_level_element_t p2) {
	return DX_NUMERIC_COMPARATOR(p1.price, p2.price);
}

static inline int dx_plb_pricelvel_comparator_bid(const dxf_price_level_element_t p1, dxf_price_level_element_t p2) {
	return DX_NUMERIC_COMPARATOR(p2.price, p1.price);
}

static inline int dx_plb_listener_comparator(dx_plb_listener_context_t e1, dx_plb_listener_context_t e2) {
	return DX_NUMERIC_COMPARATOR(e1.listener, e2.listener);
}

/*******************************/
/* Source management functions */
/*******************************/

/* This function should be called without source guard */
static void dx_plb_source_free(dx_plb_source_t *source) {
	if (source->subscription != NULL)
		dxf_close_subscription(source->subscription);
	CHECKED_FREE(source->symbol);
	FREE_ARRAY(source->snapshot.elements, source->snapshot.size);
	dx_mutex_destroy(&source->guard);
	CHECKED_FREE(source);
}

/* -------------------------------------------------------------------------- */

/* This function should be called without source guard */
static bool dx_plb_source_clear(dx_plb_source_t *source, bool force) {
	ERRORCODE err = DXF_SUCCESS;
	dx_plb_source_consumer_t *c, *n;
	bool res = true;

	/* Check death condition */
	if (!force && source->consumers) {
		return false;
	}

	/*
	Close subscription. It will detach listener, under subscription lock,
	so it is guarenteed, that listener with this source will not be run
	*/
	err = dxf_close_subscription(source->subscription);
	source->subscription = NULL;

	/* Free up all consumers, it is force exit */
	for (c = source->consumers; c != NULL; c = n) {
		n = c->next;
		/* Free consumer only if we zeroth source of it, to avoid double-free */
		if (c->sourceidx == 0)
			res &= dx_plb_book_free(c->consumer);
		dx_free(c);
	}

	/* Free all data */
	dx_plb_source_free(source);
	return res && err == DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

/* This function should be called without source guard, but with context guard taken */
static void dx_plb_source_cleanup(dx_plb_connection_context_t* context, dx_plb_source_t *source) {
	/* Adding of book to source is always under context taken, so we are safe here */
	if (source->consumers != NULL) {
		return;
	}
	dx_plb_ctx_remove_source(context, source);
	/* Subscription lock in this call guarantee absence of race in event listener */
	dx_plb_source_clear(source, false);
}

/* -------------------------------------------------------------------------- */

static dxf_ulong_t dx_plb_source_hash(dxf_const_string_t symbol, dxf_const_string_t src) {
	dxf_int_t h1 = dx_symbol_name_hasher(symbol);
	dxf_int_t h2 = dx_symbol_name_hasher(src);
	return ((dxf_ulong_t)h1) << 32 | (dxf_ulong_t)h2;
}

/* -------------------------------------------------------------------------- */

/* This function must be called with context guard taken */
static size_t dx_plb_source_find_pos(dx_plb_connection_context_t* context, dxf_const_string_t symbol, dxf_const_string_t src) {
	dxf_ulong_t hash = dx_plb_source_hash(symbol, src);
	size_t i = (size_t)(hash % context->src_capacity);
	while (context->sources[i] != NULL) {
		/* Check, do we find it? */
		if (context->sources[i]->hash == hash
			&& dx_compare_strings(context->sources[i]->symbol, symbol) == 0
			&& dx_compare_strings(context->sources[i]->source, src) == 0)
			return i;
		i = (i + 1) % context->src_capacity;
	}
	return i;
}

/* -------------------------------------------------------------------------- */

static dx_plb_source_t *dx_plb_source_create(dxf_connection_t connection, dxf_const_string_t symbol, dxf_const_string_t src) {
	const static dx_event_subscr_flag subscr_flags = dx_esf_single_record | dx_esf_time_series;
	dx_plb_source_t *source = NULL;

	/* Create new source */
	source = dx_calloc(1, sizeof(*source));
	if (source == NULL) {
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}

	if (!dx_mutex_create(&source->guard)) {
		dx_free(source);
		return NULL;
	}

	source->hash = dx_plb_source_hash(symbol, src);
	source->symbol = dx_create_string_src(symbol);
	if (source->symbol == NULL) {
		dx_plb_source_free(source);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}
	dx_copy_string_len(source->source, src, DXF_RECORD_SUFFIX_SIZE);
	source->source[DXF_RECORD_SUFFIX_SIZE - 1] = L'\0';

	/* Add some place for snapshot */
	source->snapshot.capacity = 16;
	source->snapshot.elements = dx_calloc(source->snapshot.capacity, sizeof(source->snapshot.elements[0]));

	/* Prepare comparators */
	source->bids.cmp = &dx_plb_pricelvel_comparator_bid;
	source->asks.cmp = &dx_plb_pricelvel_comparator_ask;

	source->snapshot_status = dx_status_full;

	/* Create subscription */
	if ((source->subscription = dx_create_event_subscription(connection, DXF_ET_ORDER, subscr_flags, 0)) == dx_invalid_subscription) {
		dx_plb_source_free(source);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}

	/* Attach event listener */
	if (!dxf_attach_event_listener_v2(source->subscription, &plb_event_listener, source)) {
		dx_plb_source_free(source);
		return NULL;
	}

	/* Set source */
	dx_clear_order_source(source->subscription);
	if (!dx_add_order_source(source->subscription, source->source)) {
		dx_plb_source_free(source);
		return NULL;
	}

	/* Add symbol */
	if (!dx_add_symbols(source->subscription, &symbol, 1)) {
		dx_plb_source_free(source);
		return NULL;
	}

	/* And make all these motions */
	if (!dx_load_events_for_subscription(connection, dx_get_order_source(source->subscription), DXF_ET_ORDER, subscr_flags) ||
		!dx_send_record_description(connection, false) ||
		!dx_subscribe_symbols_to_events(connection, dx_get_order_source(source->subscription),
			&symbol, 1, DXF_ET_ORDER, false, false, subscr_flags, 0)) {
		dx_plb_source_free(source);
		return NULL;
	}

	return source;
}

/* -------------------------------------------------------------------------- */
/* This must be called without source guard taken */
static void dx_plb_source_remove_book(dx_plb_source_t *source, dx_price_level_book_t *book, size_t idx) {
	dx_plb_source_consumer_t *c, **p;
	if (!dx_mutex_lock(&source->guard)) {
		return;
	}
	p = &source->consumers;
	for (c = source->consumers; c != NULL; c = c->next) {
		if (c->consumer == book) {
			/* found, add checks */
			if (c->sourceidx != idx) {
				dx_logging_error(L"PLB Internal error: mixed up source indexies\n");
			}
			(*p) = c->next;
			dx_free(c);
			dx_mutex_unlock(&source->guard);
			return;
		}
		p = &c->next;
	}
	/* Problems! */
	dx_mutex_unlock(&source->guard);
	dx_logging_error(L"PLB Internal error: can not find consumer for source\n");
}

/* -------------------------------------------------------------------------- */
/* This must be called without source guard takenm but with context guard taken (!) */
static bool dx_plb_source_add_book(dx_plb_source_t *source, dx_price_level_book_t *book, int idx) {
	dx_plb_source_consumer_t *c;

	c = dx_calloc(1, sizeof(*c));
	if (c == NULL) {
		dx_set_error_code(dx_mec_insufficient_memory);
		return false;
	}

	c->consumer = book;
	c->sourceidx = idx;

	if (!dx_mutex_lock(&source->guard)) {
		dx_free(c);
		return false;
	}
	c->next = source->consumers;
	source->consumers = c;
	dx_mutex_unlock(&source->guard);
	return true;
}

/* -------------------------------------------------------------------------- */

static void dx_plb_source_reset_snapshot(dx_plb_source_t *source) {
	size_t i = 0;
	for (i = 0; i < source->snapshot.size; i++) {
		dx_free(source->snapshot.elements[i]);
		source->snapshot.elements[i] = NULL;
	}
	source->snapshot.size = 0;

	dx_memset(source->bids.levels, 0, sizeof(source->bids.levels));
	source->bids.count = 0;
	for (i = 0; i < sizeof(source->bids.levels) / sizeof(source->bids.levels[0]); i++)
		source->bids.levels[i].price = NAN;
	/* Clean up final book */
	source->final_bids = source->bids;

	dx_memset(source->asks.levels, 0, sizeof(source->asks.levels));
	source->asks.count = 0;
	for (i = 0; i < sizeof(source->asks.levels) / sizeof(source->asks.levels[0]); i++)
		source->asks.levels[i].price = NAN;
	/* Clean up final book */
	source->final_asks = source->asks;
}

/* -------------------------------------------------------------------------- */

static void dx_plb_source_remove_order_from_levels(dx_plb_price_level_side_t *ob, const dxf_order_t *order) {
	size_t pos;
	bool found;
	dxf_price_level_element_t key = { order->price, 0, 0 };
	DX_ARRAY_BINARY_SEARCH(ob->levels, 0, ob->count, key, ob->cmp, found, pos);
	if (!found)
		return;
	ob->levels[pos].size -= order->size;
	ob->levels[pos].time = order->time;
	/* If size is zero, we should rebuild whole order book (maybe, other zeroes was in same tx) */
	ob->rebuild |= ob->levels[pos].size <= 0;
	ob->updated = true;
}

/* -------------------------------------------------------------------------- */

static void dx_plb_source_add_order_to_levels(dx_plb_price_level_side_t *ob, const dxf_order_t *order) {
	size_t pos;
	bool found;
	dxf_price_level_element_t key = { order->price, 0, 0 };
	DX_ARRAY_BINARY_SEARCH(ob->levels, 0, ob->count, key, ob->cmp, found, pos);
	if (found) {
		ob->levels[pos].size += order->size;
		ob->levels[pos].time = order->time;
		ob->updated = true;
	}
	else if (pos < ob->count && ob->count == MAX_PRICE_LEVELS) {
		dx_memmove(&ob->levels[pos + 1], &ob->levels[pos], sizeof(ob->levels[0]) * (ob->count - pos - 1));
		ob->levels[pos].price = order->price;
		ob->levels[pos].size = order->size;
		ob->levels[pos].time = order->time;
		ob->updated = true;
	}
	else if (ob->count < MAX_PRICE_LEVELS) {
		/* Need to insert here */
		if (pos < ob->count) {
			dx_memmove(&ob->levels[pos + 1], &ob->levels[pos], sizeof(ob->levels[0]) * (ob->count - pos));
		}
		ob->levels[pos].price = order->price;
		ob->levels[pos].size = order->size;
		ob->levels[pos].time = order->time;
		ob->count++;
		ob->updated = true;
	}
}

/* -------------------------------------------------------------------------- */

static void dx_plb_source_rebuild_levels(dx_plb_records_array_t *snapshot, dx_plb_price_level_side_t *ob, dxf_order_side_t side) {
	size_t i = 0;
	dx_memset(ob->levels, 0, sizeof(ob->levels));
	ob->count = 0;
	for (; i < snapshot->size; i++) {
		if (snapshot->elements[i]->side != side || snapshot->elements[i]->size == 0)
			continue;
		dx_plb_source_add_order_to_levels(ob, snapshot->elements[i]);
	}
	ob->rebuild = false;
	ob->updated = true;
}

/* -------------------------------------------------------------------------- */

static void dx_plb_source_process_order(dx_plb_source_t *source, const dxf_order_t *order, bool rm) {
	size_t orderIdx;
	size_t top, bottom;
	bool found = true;
	bool error;
	dxf_order_t *oo;

	/* Fast path: maybe, order could be found directly from it index? */
	orderIdx = (dxf_uint_t)(order->index & 0xFFFFFFFF);
	if (orderIdx >= source->snapshot.size || source->snapshot.elements[orderIdx]->index != order->index) {
		/* Long way: binary search.  */
		if (orderIdx < source->snapshot.size) {
			if (source->snapshot.elements[orderIdx]->index < order->index) {
				top = orderIdx;
				bottom = source->snapshot.size;
			}
			else {
				top = 0;
				bottom = orderIdx;
			}
		}
		else {
			top = 0;
			bottom = source->snapshot.size;
		}
		DX_ARRAY_BINARY_SEARCH(source->snapshot.elements, top, bottom, order, dx_plb_order_comparator, found, orderIdx);
	}
	if (found) {
		oo = source->snapshot.elements[orderIdx];
		/* It is removal or update? */
		if (rm || order->size == 0) {
			dx_plb_source_remove_order_from_levels(oo->side == dxf_osd_buy ? &source->bids : &source->asks, oo);
			if (rm) {
				DX_ARRAY_DELETE(source->snapshot, dxf_order_t*, orderIdx, dx_capacity_manager_halfer, error);
				dx_free(oo);
			}
			else {
				dx_memset(oo, 0, sizeof(*oo));
				oo->index = order->index;
			}
		}
		else {
			/* Update order */
			/* Add first, to minimize chances to hit zero size */
			dx_plb_source_add_order_to_levels(order->side == dxf_osd_buy ? &source->bids : &source->asks, order);
			/* Remove old order, which is replaced by this one */
			dx_plb_source_remove_order_from_levels(oo->side == dxf_osd_buy ? &source->bids : &source->asks, oo);
			/* And replace order */
			*oo = *order;
		}
	}
	else if (!rm && order->size != 0) {
		/* Not found: Add it */
		oo = dx_calloc(1, sizeof(*oo));
		if (oo == NULL) {
			/* Baaaad! */
			return;
		}
		dx_memcpy(oo, order, sizeof(*oo));
		DX_ARRAY_INSERT(source->snapshot, dxf_order_t *, oo, orderIdx, dx_capacity_manager_halfer, error);
		dx_plb_source_add_order_to_levels(oo->side == dxf_osd_buy ? &source->bids : &source->asks, oo);
	}
}

/*****************************/
/* Book management functions */
/*****************************/

static bool dx_plb_book_free(dx_price_level_book_t *book) {
	CHECKED_FREE(book->symbol);
	CHECKED_FREE(book->src_bids);
	CHECKED_FREE(book->src_asks);
	CHECKED_FREE(book->sources);
	dx_mutex_destroy(&book->guard);
	CHECKED_FREE(book);
	return true;
}

/* -------------------------------------------------------------------------- */

/* This functuions must be called without guard */
static void dx_plb_book_clear(dx_price_level_book_t *book) {
	size_t i = 0;

	/*
	Remove this book from all sources
	Each removal is under source guard.
	Nothing is changing in book itself, so it could be called
	by other sources without problems
	*/
	if (book->sources != NULL) {
		/* Check sources, they could be not fully initialized */
		for (; i < book->sources_count && book->sources[i] != NULL; i++) {
			dx_plb_source_remove_book(book->sources[i], book, i);
		}
		/* After this point, no source could call this book, so it is safe to simply remove it */

		/*
		And cleanup all "dead" sources
		It could not be done in loop above to avoid situation when next source call book and book
		uses deleted source.
		*/
		dx_mutex_lock(&CTX(book->context)->guard);
		for (i = 0; i < book->sources_count && book->sources[i] != NULL; i++) {
			dx_plb_source_cleanup(CTX(book->context), book->sources[i]);
		}
		dx_mutex_unlock(&CTX(book->context)->guard);
	}
	/* Now nothing could call book methods, kill it */
	dx_plb_book_free(book);

}


/* -------------------------------------------------------------------------- */

/* This functuions must be called with book guard taken */
static bool dx_plb_book_update_one_side(dx_plb_price_level_side_t *dst, dx_plb_price_level_side_t **srcs, size_t src_count, double startValue) {
	bool changed = false;
	size_t didx = 0;
	size_t *sidx = dx_calloc(src_count, sizeof(size_t));
	size_t i;
	int cmp;
	dxf_price_level_element_t best;

	/* It is merge-sort of src_count sources */
	for (; didx < MAX_PRICE_LEVELS; didx++) {
		/* First of all: find, do we have any input data? */
		best.price = startValue;
		best.size = 0;
		best.time = 0;
		for (i = 0; i < src_count; i++) {
			/* If we have data from this source and it is better than current best, replace */
			if (sidx[i] < srcs[i]->count && !isnan(srcs[i]->levels[sidx[i]].price)) {
				cmp = dst->cmp(best, srcs[i]->levels[sidx[i]]);
				if (cmp == 0) {
					best.size += srcs[i]->levels[sidx[i]].size;
					best.time = MAX(best.time, srcs[i]->levels[sidx[i]].time);
				} else if (cmp > 0) {
					/* Best is "greater" than current, replace (it may be smaller, if comarator is reversed) */
					best = srcs[i]->levels[sidx[i]];
				}
			}
		}
		/* All sources are gone */
		if (best.size == 0)
			break;
		if (memcmp(&dst->levels[didx], &best, sizeof(best)) != 0) {
			dst->levels[didx] = best;
			changed = true;
		}
		/* And increase all sources which was used tis time */
		for (i = 0; i < src_count; i++) {
			if (sidx[i] < srcs[i]->count && srcs[i]->levels[sidx[i]].price == best.price) {
				sidx[i]++;
			}
		}
	}
	/* Fix size */
	dst->count = didx;
	dx_free(sidx);
	return changed;
}

/* -------------------------------------------------------------------------- */

/* This functuions must be called without book guard */
static void dx_plb_book_update(dx_price_level_book_t *book, dx_plb_source_t *src) {
	bool changed = false;
	size_t i = 0;

	if (src->bids.updated) {
		changed |= dx_plb_book_update_one_side(&book->bids, book->src_bids, book->sources_count, 0.0);
	}
	if (src->asks.updated) {
		changed |= dx_plb_book_update_one_side(&book->asks, book->src_asks, book->sources_count, 1.0E38);
	}
	if (changed) {
		book->book.bids_count = book->bids.count;
		book->book.asks_count = book->asks.count;
		if (!dx_mutex_lock(&book->guard)) {
			return;
		}
		for (; i < book->listeners.size; i++)
			book->listeners.elements[i].listener(&book->book, book->listeners.elements[i].user_data);
		dx_mutex_unlock(&book->guard);
	}
}

/******************/
/* EVENT LISTENER */
/******************/

/*
This is called with subscription lock, so it is imposisble to delete source
when this code is executing.
 */
static void plb_event_listener(int event_type, dxf_const_string_t symbol_name,
							const dxf_event_data_t* data, int data_count,
							const dxf_event_params_t* event_params, void* user_data) {
	const dxf_order_t *orders = (const dxf_order_t *)data;
	dx_plb_source_t *source = (dx_plb_source_t *)user_data;
	dx_plb_source_consumer_t *c;
	int i = 0;
	bool sb, se, rm, tx;

	/* Check params */
	if (event_type != DXF_ET_ORDER) {
		dx_logging_error(L"Listener for Price Level Book was called with wrong event type\n");
		return;
	}

	if (dx_compare_strings(source->symbol, symbol_name) != 0) {
		dx_logging_error(L"Listener for Price Level Book was called with wrong symbol\n");
		return;
	}

	sb = IS_FLAG_SET(event_params->flags, dxf_ef_snapshot_begin);
	se = IS_FLAG_SET(event_params->flags, dxf_ef_snapshot_end) || IS_FLAG_SET(event_params->flags, dxf_ef_snapshot_snip);
	tx = IS_FLAG_SET(event_params->flags, dxf_ef_tx_pending);

	/* Ok, process data */
	for (; i < data_count; i++) {
		/* Special check */
		if (wcscmp(source->source, orders[i].source)) {
			continue;
		}

		rm = IS_FLAG_SET(event_params->flags, dxf_ef_remove_event) || orders[i].size == 0;

		/* Ok, process this event */
		if (sb) {
			/* Clear snapshot */
			dx_plb_source_reset_snapshot(source);
			source->snapshot_status = dx_status_begin;
		}
		if (source->snapshot_status == dx_status_unknown) {
			/* If we in unknown state, skip */
			continue;
		}

		/* Now process this order */
		dx_plb_source_process_order(source, &orders[i], rm);

		/* Set end-of-snapshot */
		if (se) {
			source->snapshot_status = dx_status_full;
		}
		if (tx && source->snapshot_status == dx_status_full) {
			source->snapshot_status = dx_status_pending;
		} else if (!tx && source->snapshot_status == dx_status_pending) {
			source->snapshot_status = dx_status_full;
		}

		/* And call all consumers  if it is end of transaction */
		if ((source->asks.updated || source->bids.updated) && source->snapshot_status == dx_status_full) {
			if (source->bids.rebuild) {
				dx_plb_source_rebuild_levels(&source->snapshot, &source->bids, dxf_osd_buy);
			}
			if (source->bids.updated) {
				source->final_bids = source->bids;
			}

			if (source->asks.rebuild) {
				dx_plb_source_rebuild_levels(&source->snapshot, &source->asks, dxf_osd_sell);
			}
			if (source->asks.updated) {
				source->final_asks = source->asks;
			}

			/* Lock guard to be sure that source list is intact */
			if (!dx_mutex_lock(&source->guard))
				return;
			for (c = source->consumers; c != NULL; c = c->next) {
				dx_plb_book_update(c->consumer, source);
			}
			dx_mutex_unlock(&source->guard);
		}
	}
}

/*******/
/* API */
/*******/

dxf_price_level_book_t dx_create_price_level_book(dxf_connection_t connection,
												dxf_const_string_t symbol,
												size_t srccount, dxf_ulong_t srcflags) {
	bool res = true;
	dx_plb_connection_context_t *context = NULL;
	dx_price_level_book_t *book = NULL;
	dx_plb_source_t *source;
	int i = 0;

	context = dx_get_subsystem_data(connection, dx_ccs_price_level_book, &res);
	if (context == NULL) {
		return NULL;
	}

	book = dx_calloc(1, sizeof(dx_price_level_book_t));
	if (book == NULL) {
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}
	if (!dx_mutex_create(&book->guard)) {
		dx_free(book);
		return NULL;
	}

	book->context = context;
	book->bids.cmp = &dx_plb_pricelvel_comparator_bid;
	book->asks.cmp = &dx_plb_pricelvel_comparator_ask;

	book->symbol = dx_create_string_src(symbol);
	if (book->symbol == NULL) {
		dx_plb_book_free(book);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}

	book->book.symbol = book->symbol;
	book->book.bids = &book->bids.levels[0];
	book->book.asks = &book->asks.levels[0];

	book->sources = dx_calloc(srccount, sizeof(book->sources[0]));
	if (book->sources == NULL) {
		dx_plb_book_free(book);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}
	book->src_bids = dx_calloc(srccount, sizeof(book->src_bids[0]));
	if (book->src_bids == NULL) {
		dx_plb_book_free(book);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}
	book->src_asks = dx_calloc(srccount, sizeof(book->src_asks[0]));
	if (book->src_asks == NULL) {
		dx_plb_book_free(book);
		dx_set_error_code(dx_mec_insufficient_memory);
		return NULL;
	}

	/* Get mutex to prepare all sources, all ctx functions must be called under guard */
	CHECKED_CALL(dx_mutex_lock, &(context->guard));

	/* Add or create sources */
	for (i = 0; dx_all_order_sources[i] != NULL; i++) {
		if ((srcflags & (1ULL << i)) == 0)
			continue;

		/* We have src & symbol */
		source = dx_plb_ctx_find_source(context, symbol, dx_all_order_sources[i]);
		if (source == NULL) {
			source = dx_plb_source_create(connection, symbol, dx_all_order_sources[i]);
			if (source == NULL || !dx_plb_ctx_add_source(context, source)) {
				dx_plb_book_clear(book);
				/* Remove all new sources from hash */
				dx_plb_ctx_cleanup_sources(context);
				dx_mutex_unlock(&context->guard);
				return NULL;
			}
		}
		book->sources[book->sources_count] = source;
		book->src_bids[book->sources_count] = &source->final_bids;
		book->src_asks[book->sources_count] = &source->final_asks;
		book->sources_count++;
		/*
		If this source call callback before full sources initialization, it is not a problem
		as book knows only added sources!
		*/
		if (!dx_plb_source_add_book(source, book, (int)book->sources_count - 1)) {
			dx_plb_book_clear(book);
			dx_plb_ctx_cleanup_sources(context);
			dx_mutex_unlock(&context->guard);
			return NULL;
		}
	}
	dx_mutex_unlock(&context->guard);
	return book;
}

bool dx_close_price_level_book(dxf_price_level_book_t book) {
	dx_plb_book_clear(book);
	return true;
}

bool dx_add_price_level_book_listener(dxf_price_level_book_t book,
									dxf_price_level_book_listener_t book_listener,
									void *user_data) {
	dx_price_level_book_t *b = (dx_price_level_book_t *)book;
	dx_plb_connection_context_t *context = CTX(b->context);
	dx_plb_listener_context_t ctx = { book_listener, user_data };
	bool found = false;
	bool error = false;
	size_t idx;

	DX_ARRAY_SEARCH(b->listeners.elements, 0, b->listeners.size, ctx, dx_plb_listener_comparator, false, found, idx);

	CHECKED_CALL(dx_mutex_lock, &(b->guard));
	if (found) {
		b->listeners.elements[idx].user_data = user_data;
	} else {
		DX_ARRAY_INSERT(b->listeners, dx_plb_listener_context_t, ctx, idx, dx_capacity_manager_halfer, error);
	}
	return dx_mutex_unlock(&b->guard) && !error;
}

bool dx_remove_price_level_book_listener(dxf_price_level_book_t book,
										dxf_price_level_book_listener_t book_listener) {
	dx_price_level_book_t *b = (dx_price_level_book_t *)book;
	dx_plb_connection_context_t *context = CTX(b->context);
	dx_plb_listener_context_t ctx = { book_listener, NULL };
	bool found = false;
	bool error = false;
	size_t idx;

	DX_ARRAY_SEARCH(b->listeners.elements, 0, b->listeners.size, ctx, dx_plb_listener_comparator, false, found, idx);
	if (!found) {
		return true;
	}

	CHECKED_CALL(dx_mutex_lock, &(b->guard));
	DX_ARRAY_DELETE(b->listeners, dx_plb_listener_context_t, idx, dx_capacity_manager_halfer, error);
	return dx_mutex_unlock(&b->guard) && !error;
}
