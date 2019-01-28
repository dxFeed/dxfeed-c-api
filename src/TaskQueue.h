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

#ifndef TASK_QUEUE_H_INCLUDED
#define TASK_QUEUE_H_INCLUDED

#include "PrimitiveTypes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Task data
 */
/* -------------------------------------------------------------------------- */

typedef enum {
	dx_tes_success = (1 << 0), /* if this flag is not set then some error occurred */
	dx_tes_dont_advance = (1 << 1), /* if this flag is set then the next tasks in queue are not processed */
	dx_tes_pop_me = (1 << 2) /* if this flag is set then the task must be popped from the queue */
} dx_task_execution_status_t;

typedef enum {
	dx_tc_free_resources = (1 << 0) /* if this flag is set then the task must free its resources and return
									without doing its job */
} dx_task_command_t;

typedef int (*dx_task_processor_t) (void* data, int command);

typedef void* dx_task_queue_t;

/* -------------------------------------------------------------------------- */
/*
 *	Task queue functions
 */
/* -------------------------------------------------------------------------- */

bool dx_create_task_queue (OUT dx_task_queue_t* tq);
bool dx_cleanup_task_queue (dx_task_queue_t tq);
bool dx_destroy_task_queue (dx_task_queue_t tq);
bool dx_add_task_to_queue (dx_task_queue_t tq, dx_task_processor_t processor, void* data);
bool dx_execute_task_queue (dx_task_queue_t tq);
bool dx_is_queue_empty (dx_task_queue_t tq, OUT bool* res);

#endif /* TASK_QUEUE_H_INCLUDED */