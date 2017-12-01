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

#include "DXMemory.h"
#include "Logger.h"
#include "DXAlgorithms.h"
#include "DXThreads.h"

typedef struct dx_callback_tag {
	void (*callback)(void*);
	void *arg;
} dx_callback_t;

typedef struct dx_callback_queue_tag {
	dx_mutex_t mutex;
	int size;
	int count;
	dx_callback_t *callbacks;
} dx_callback_queue_t;

dx_callback_queue_t g_process_destructors;

static void dx_init_cb_queue(dx_callback_queue_t *q) {
	dx_mutex_create(&q->mutex);
	q->count = 0;
	q->size = 16;
	q->callbacks = dx_calloc(q->size, sizeof(*q->callbacks));
}

static void dx_fini_cb_queue(dx_callback_queue_t *q) {
	dx_free(q->callbacks);
	dx_mutex_destroy(&q->mutex);
}

static void dx_add_cb_queue(dx_callback_queue_t *q, void (*callback)(void*), void *arg) {
	if (!dx_mutex_lock(&q->mutex))
		return;
	if (q->size == q->count) {
		dx_callback_t *cbs;
		q->size *= 2;
		cbs = dx_calloc(q->size, sizeof(*q->callbacks));
		dx_memcpy(cbs, q->callbacks, q->count * sizeof(*q->callbacks));
		dx_free(q->callbacks);
		q->callbacks = cbs;
	}
	q->callbacks[q->count].callback = callback;
	q->callbacks[q->count].arg = arg;
	q->count++;
	dx_mutex_unlock(&q->mutex);
}

static void dx_run_cb_queue(dx_callback_queue_t *q) {
	int i;
	if (!dx_mutex_lock(&q->mutex))
		return;
	for (i = 0; i < q->count; i++)
		q->callbacks[i].callback(q->callbacks[i].arg);
	dx_mutex_unlock(&q->mutex);
}

void dx_register_process_destructor(void (*destructor)(void*), void *arg) {
	dx_add_cb_queue(&g_process_destructors, destructor, arg);
}

extern void dx_init_threads();

__attribute__((constructor)) void init(void)
{
	dx_init_cb_queue(&g_process_destructors);
}

__attribute__((destructor))  void fini(void)
{
	dx_run_cb_queue(&g_process_destructors);
	dx_fini_cb_queue(&g_process_destructors);
}
