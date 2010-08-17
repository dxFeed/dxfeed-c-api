

#include "DXNetwork.h"
#include "DXErrorHandling.h"
#include "DXSubsystemRoster.h"
#include <stdio.h>
#include <Windows.h>

const char dxfeed_host[] = "demo.dxfeed.com:7300";

void data_receiver (const void* buffer, unsigned buflen) {
    printf("Data received: %d bytes\n", buflen);
}

void process_last_error () {
    int subsystem_id = sc_invalid_subsystem;
    int error_code = 0;
    const char* error_descr = NULL;
    enum dx_error_function_result_t res;
    
    res = dx_get_last_error(&subsystem_id, &error_code, &error_descr);
    
    switch (res) {
    case efr_success:
        printf("Error occured and successfully retrieved:\n"
               "subsystem code = %d, error code = %d, description = \"%s\"\n",
               subsystem_id, error_code, error_descr);
        return;
    case efr_error_subsys_init_failure:
        printf("An error occured but the error subsystem failed to initialize\n");
        return;
    default:
        printf("WTF");        
    }    
}

int main(int argc, char* argv[]) {
    struct dx_connection_context_t ctx;
    
    printf("Connection test started.\n");
    
    ctx.receiver = data_receiver;
    
    printf("Connecting to host...\n");
    
    if (!dx_create_connection(dxfeed_host, &ctx)) {
        process_last_error();
        
        return;
    }
    
    printf("Connection successful!\n");
    
    Sleep(5000);
    
    printf("Disconnecting from host...\n");
    
    if (!dx_close_connection()) {
        process_last_error();
        
        return;
    }
    
    printf("Disconnect successful!\n"
           "Connection test completed successfully!\n");

	return 0;
}

