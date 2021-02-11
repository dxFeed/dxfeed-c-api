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

#pragma once

#include "DXTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

void* dx_create_binary_qtp_composer();

int dx_destroy_binary_qtp_composer(void* binary_qtp_composer);

int dx_set_composer_context(void* binary_qtp_composer, void* buffered_output_connection_context);

int dx_compose_empty_heartbeat(void* binary_qtp_composer);

int dx_compose_heartbeat(void* binary_qtp_composer, const void* heartbeat_payload);

void* dx_get_binary_qtp_composer(dxf_connection_t connection);

#ifdef __cplusplus
}
#endif