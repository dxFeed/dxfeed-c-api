/*
 * The contents of this file are subject to the Mozilla Public License Version
 * 2.0 (the "License"); you may not use this file except in compliance with
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

void* dx_get_connection_impl(dxf_connection_t connection);

int dx_connection_create_outgoing_heartbeat(void* connection_impl);

int dx_connection_process_incoming_heartbeat(void* connection_impl);

#ifdef __cplusplus
}
#endif
