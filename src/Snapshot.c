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

#include "Snapshot.h"
#include "DXAlgorithms.h"
#include "DXErrorCodes.h"
#include "EventSubscription.h"

/* -------------------------------------------------------------------------- */
/*
*	Internal data structures and defines
*/
/* -------------------------------------------------------------------------- */

typedef struct {
    dxf_snapshot_listener_t listener;
    void* user_data;
} dx_listener_context_t;

typedef struct {
    dx_listener_context_t* elements;
    int size;
    int capacity;
} dx_listener_array_t;

typedef struct {

} dx_snapshot_record_t;

typedef struct {
    dx_snapshot_record_t* elements;
    int size;
    int capacity;
} dx_snapshot_records_array_t;

typedef struct {
    dxf_subscription_t subscription;
    dx_event_id_t event_id;
    dxf_const_string_t symbol;
    dx_listener_array_t listeners;

    dx_snapshot_records_array_t records;
} dx_snapshot_data_t, *dx_snapshot_data_ptr_t;

//TODO: need to map (key(s)->value) implementation
typedef struct {
    dx_snapshot_data_ptr_t* elements;
    int size;
    int capacity;
} dx_snapshots_array_t;

#define DX_SNAPSHOTS_COMPARATOR(l, r) (l->event_id == r->event_id ? dx_compare_strings(l->symbol, r->symbol) : (l->event_id < r->event_id ? -1 : 1))

/* -------------------------------------------------------------------------- */
/*
*	Subscription functions implementation
*/
/* -------------------------------------------------------------------------- */

const dxf_snapshot_t dx_invalid_snapshot = (dxf_snapshot_t)NULL;
dx_snapshots_array_t snapshots_array = {NULL, 0, 0};

void dx_free_snapshot_data(dx_snapshot_data_t* snapshot_data) {
    if (snapshot_data == NULL) {
        return;
    }
    if (snapshot_data->subscription != dx_invalid_subscription) {
        dx_close_event_subscription((dxf_subscription_t)snapshot_data->subscription);
    }
    //TODO: remove listeners
    //TODO: remove records
    if (snapshot_data->symbol != NULL)
        dx_free(snapshot_data->symbol);
    dx_free(snapshot_data);
}

dxf_snapshot_t dx_create_snapshot(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol) {
    dx_snapshot_data_t *snapshot_data = NULL;
    bool failed = false;
    bool found = false;
    int position = 0;

    if (!dx_validate_connection_handle(connection, false)) {
        return dx_invalid_snapshot;
    }

    if (event_id < dx_eid_begin || event_id >= dx_eid_count) {
        dx_set_error_code(dx_ssec_invalid_event_id);
        return dx_invalid_snapshot;
    }

    if (symbol == NULL || dx_string_length(symbol) == 0) {
        dx_set_error_code(dx_ssec_invalid_symbol);
        return dx_invalid_snapshot;
    }

    snapshot_data = dx_calloc(1, sizeof(dx_snapshot_data_t));
    if (snapshot_data == NULL) {
        return dx_invalid_snapshot;
    }
    snapshot_data->event_id = event_id;
    snapshot_data->symbol = dx_create_string_src(symbol);

    if (snapshots_array.size > 0) {
        //TODO: improve search algorithms
        DX_ARRAY_SEARCH(snapshots_array.elements, 0, snapshots_array.size, snapshot_data, DX_SNAPSHOTS_COMPARATOR, false, found, position);
    }

    snapshot_data->subscription = dx_create_event_subscription(connection, DX_EVENT_BIT_MASK(event_id));
    if (snapshot_data->subscription == dx_invalid_subscription) {
        dx_free_snapshot_data(snapshot_data);
        return dx_invalid_snapshot;
    }
    if (dx_add_symbols(snapshot_data->subscription, &symbol, 1) == false) {
        dx_free_snapshot_data(snapshot_data);
        return dx_invalid_snapshot;
    }

    //TODO: add event listener

    DX_ARRAY_INSERT(snapshots_array, dx_snapshot_data_ptr_t, snapshot_data, position, dx_capacity_manager_halfer, failed);
    if (failed) {
        dx_free_snapshot_data(snapshot_data);
        return dx_invalid_snapshot;
    }

    return (dxf_snapshot_t)snapshot_data;
}

bool dx_close_snapshot(dxf_snapshot_t snapshot) {
    dx_snapshot_data_ptr_t snapshot_data_ptr = NULL;
    bool found = false;
    bool failed = false;
    int position = 0;

    if (snapshot == dx_invalid_snapshot) {
        return dx_set_error_code(dx_ssec_invalid_snapshot_id);
    }

    snapshot_data_ptr = (dx_snapshot_data_ptr_t)snapshot;

    //remove item from snapshots_array
    DX_ARRAY_SEARCH(snapshots_array.elements, 0, snapshots_array.size, snapshot_data_ptr, DX_SNAPSHOTS_COMPARATOR, false, found, position);
    if (!found) {
        return dx_set_error_code(dx_ssec_invalid_snapshot_id);
    }
    DX_ARRAY_DELETE(snapshots_array, dx_snapshot_data_ptr_t, position, DX_SNAPSHOTS_COMPARATOR, failed);
    if (failed) {
        return dx_set_error_code(dx_mec_insufficient_memory);
    }

    dx_free_snapshot_data(snapshot_data_ptr);
}

bool dx_add_listener(dxf_snapshot_t snapshot, dxf_snapshot_listener_t listener, void* user_data) {

}

bool dx_remove_listener(dxf_snapshot_t snapshot, dxf_snapshot_listener_t listener) {

}
