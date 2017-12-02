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

/*
 *	Record transcoder transforms the records we receive from server into the event data
 *  passed to the client side.
 */

#ifndef RECORD_TRANSCODER_H_INCLUDED
#define RECORD_TRANSCODER_H_INCLUDED

#include "RecordData.h"
#include "PrimitiveTypes.h"

typedef struct {
	dx_record_id_t record_id;
	dx_record_info_id_t record_info_id;
	dxf_const_string_t suffix;
	dxf_const_string_t symbol_name;
	dxf_int_t symbol_cipher;
	dxf_event_flags_t flags;
	dxf_time_int_field_t time_int_field;
} dx_record_params_t;

/* -------------------------------------------------------------------------- */
/*
 *	Interface functions
 */
/* -------------------------------------------------------------------------- */

bool dx_transcode_record_data (dxf_connection_t connection,
							const dx_record_params_t* record_params,
							const dxf_event_params_t* event_params,
							void* record_buffer, int record_count);

#endif /* RECORD_TRANSCODER_H_INCLUDED */