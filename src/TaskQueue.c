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

#include "TaskQueue.h"
#include "DXThreads.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "DXErrorHandling.h"
#include "Logger.h"

/* -------------------------------------------------------------------------- */
/*
 *	Task queue data
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dx_task_processor_t processor;
	void* data;
} dx_task_data_t;

typedef struct {
	dx_mutex_t guard;

	dx_task_data_t* elements;
	size_t size;
	size_t capacity;

	int set_fields_flags;
} dx_task_queue_data_t;

#define MUTEX_FIELD_FLAG (1 << 0)

/* -------------------------------------------------------------------------- */
/*
 *	Helper functions
 */
/* -------------------------------------------------------------------------- */

bool dx_clear_task_queue_data (dx_task_queue_data_t* tqd) {
	bool res = true;

	if (tqd == NULL) {
		return true;
	}

	if (IS_FLAG_SET(tqd->set_fields_flags, MUTEX_FIELD_FLAG)) {
		res = dx_mutex_destroy(&tqd->guard) && res;
	}

	CHECKED_FREE(tqd->elements);
	dx_free(tqd);

	return res;
}

/* -------------------------------------------------------------------------- */
/*
 *	Task queue functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_create_task_queue (OUT dx_task_queue_t* tq) {
	dx_task_queue_data_t* tqd = NULL;

	if (tq == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	tqd = dx_calloc(1, sizeof(dx_task_queue_data_t));

	if (tqd == NULL) {
		return false;
	}

	if (!(dx_mutex_create(&tqd->guard) && (tqd->set_fields_flags |= MUTEX_FIELD_FLAG))) { /* setting the flag if the function succeeded, not setting otherwise */
		dx_clear_task_queue_data(tqd);

		return false;
	}

	*tq = tqd;

	return true;
}

/* -------------------------------------------------------------------------- */

bool dx_cleanup_task_queue (dx_task_queue_t tq) {
	dx_task_queue_data_t* tqd = tq;
	size_t i = 0;
	bool res = true;

	if (tq == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	CHECKED_CALL(dx_mutex_lock, &(tqd->guard));
	for (; i < tqd->size; ++i) {
		int task_res = tqd->elements[i].processor(tqd->elements[i].data, dx_tc_free_resources);
		res = IS_FLAG_SET(task_res, dx_tes_success) && res;
	}
	tqd->size = 0;

	return dx_mutex_unlock(&(tqd->guard)) && res;
}

/* -------------------------------------------------------------------------- */

bool dx_destroy_task_queue (dx_task_queue_t tq) {
	bool res = true;

	if (tq == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	res = dx_cleanup_task_queue(tq) && res;
	res = dx_clear_task_queue_data(tq) && res;

	return res;
}

/* -------------------------------------------------------------------------- */

bool dx_add_task_to_queue (dx_task_queue_t tq, dx_task_processor_t processor, void* data) {
	dx_task_queue_data_t* tqd = tq;
	dx_task_data_t task;
	bool failed = false;

	if (tq == NULL || processor == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	CHECKED_CALL(dx_mutex_lock, &(tqd->guard));

	task.processor = processor;
	task.data = data;

#ifdef _DEBUG_TQ
	dx_logging_dbg_lock();
	dx_logging_dbg(L"NEWTASK Submit [0x%016p] %ls(0x%016p) at %u", processor, dx_logging_dbg_sym(processor), data, tqd->size);
	dx_logging_dbg_stack();
	dx_logging_dbg_unlock();
#endif

	DX_ARRAY_INSERT(*tqd, dx_task_data_t, task, tqd->size, dx_capacity_manager_halfer, failed);

	return dx_mutex_unlock(&(tqd->guard)) && !failed;
}

/* -------------------------------------------------------------------------- */

bool dx_execute_task_queue (dx_task_queue_t tq) {
	dx_task_queue_data_t* tqd = tq;
	size_t i = 0;
	bool res = true;

	if (tq == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	CHECKED_CALL(dx_mutex_lock, &(tqd->guard));

	while (i < tqd->size) {
		dx_task_data_t* task = &(tqd->elements[i]);
#ifdef _DEBUG_TQ
		dx_logging_dbg_lock();
		dx_logging_dbg(L"RUNTASK Execute [0x%016p] %ls(0x%016p) at %u", task->processor, dx_logging_dbg_sym(task->processor), task->data, i);
		dx_logging_dbg_stack();
		dx_logging_dbg_unlock();
#endif
		int task_res = task->processor(task->data, 0);

		res = IS_FLAG_SET(task_res, dx_tes_success) && res;

		if (IS_FLAG_SET(task_res, dx_tes_pop_me)) {
			bool failed = false;

			DX_ARRAY_DELETE(*tqd, dx_task_data_t, i, dx_capacity_manager_halfer, failed);

			res = res && !failed;
		}

		if (IS_FLAG_SET(task_res, dx_tes_dont_advance) || !res) {
			break;
		}

		if (IS_FLAG_SET(task_res, dx_tes_pop_me)) {
			/* the current element is deleted, now the next element has its index, no need to increase it */
			continue;
		}

		++i;
	}

	return dx_mutex_unlock(&(tqd->guard)) && res;
}

/* -------------------------------------------------------------------------- */

bool dx_is_queue_empty (dx_task_queue_t tq, OUT bool* res) {
	dx_task_queue_data_t* tqd = tq;

	if (tq == NULL || res == NULL) {
		return dx_set_error_code(dx_ec_invalid_func_param_internal);
	}

	*res = (tqd->size == 0);

	return true;
}
