

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include <stdio.h>
#include <Windows.h>

const char dxfeed_host[] = "demo.dxfeed.com:7300";

/* -------------------------------------------------------------------------- */

void process_last_error () {
    int subsystem_id = dx_sc_invalid_subsystem;
    int error_code = DX_INVALID_ERROR_CODE;
    const char* error_descr = NULL;
    int res;
    
    res = dxf_get_last_error(&subsystem_id, &error_code, &error_descr);
    
    if (res == DXF_SUCCESS) {
        if (subsystem_id == dx_sc_invalid_subsystem && error_code == DX_INVALID_ERROR_CODE) {
            printf("WTF - no error information is stored");
            
            return;
        }
        
        printf("Error occurred and successfully retrieved:\n"
               "subsystem code = %d, error code = %d, description = \"%s\"\n",
               subsystem_id, error_code, error_descr);
        return;
    }
    
    printf("An error occurred but the error subsystem failed to initialize\n");
}

/* -------------------------------------------------------------------------- */

int main (int argc, char* argv[]) {
    printf("Connection test started.\n");    
    printf("Connecting to host %s...\n", dxfeed_host);
    
    if (!dxf_connect_feed(dxfeed_host)) {
        process_last_error();

        return -1;
    }
    
    printf("Connection successful!\n");
    
    Sleep(100000);
    
    printf("Disconnecting from host...\n");
    
    if (!dxf_disconnect_feed()) {
        process_last_error();
        
        return -1;
    }
    
    printf("Disconnect successful!\n"
           "Connection test completed successfully!\n");
           
    return 0;
}

