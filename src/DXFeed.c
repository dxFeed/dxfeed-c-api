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

#include "DXFeed.h"
#include "DXNetwork.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"
#include <Windows.h>
#include <stdio.h>

BOOL APIENTRY DllMain( HANDLE hModule, DWORD  ul_reason_for_call, LPVOID lpReserved )
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	case DLL_THREAD_ATTACH:
	case DLL_THREAD_DETACH:
	case DLL_PROCESS_DETACH:
		break;
	}
    return TRUE;
}

/* -------------------------------------------------------------------------- */
/*
 *	Temporary internal stuff
 */
/* -------------------------------------------------------------------------- */

void data_receiver (const void* buffer, unsigned buflen) {
    printf("Internal data receiver stub. Data received: %d bytes\n", buflen);
}

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed API implementation
 */
/* -------------------------------------------------------------------------- */

DXFEED_API int dxf_connect_feed (const char* host) {
    struct dx_connection_context_t cc;
    
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
    
    cc.receiver = data_receiver;
    
    return dx_create_connection(host, &cc);
}

/* -------------------------------------------------------------------------- */

DXFEED_API int dxf_disconnect_feed () {
    if (!dx_pop_last_error()) {
        return DXF_FAILURE;
    }
    
    return dx_close_connection();
}

/* -------------------------------------------------------------------------- */

DXFEED_API int dxf_get_last_error (int* subsystem_id, int* error_code, const char** error_descr) {
    int res = dx_get_last_error(subsystem_id, error_code, error_descr);
    
    switch (res) {
    case efr_no_error_stored:
        if (subsystem_id != NULL) {
            *subsystem_id = dx_sc_invalid_subsystem;
        }
        
        if (error_code != NULL) {
            *error_code = DX_INVALID_ERROR_CODE;
        }
        
        if (error_descr != NULL) {
            *error_descr = "No error occurred";
        }
    case efr_success:
        return DXF_SUCCESS;
    }
    
    return DXF_FAILURE;
}
