

#include "DXFeed.h"
#include "DXErrorCodes.h"
#include <stdio.h>
#include <Windows.h>

const char dxfeed_host[] = "demo.dxfeed.com:7300";

/* -------------------------------------------------------------------------- */

void process_last_error () {
	int error_code = dx_ec_success;
	dxf_const_string_t error_descr = NULL;
	int res;

	res = dxf_get_last_error(&error_code, &error_descr);

	if (res == DXF_SUCCESS) {
		if (error_code == dx_ec_success) {
			printf("WTF - no error information is stored");

			return;
		}

		wprintf(L"Error occurred and successfully retrieved:\n"
			L"error code = %d, description = \"%ls\"\n",
			error_code, error_descr);
		return;
	}

	printf("An error occurred but the error subsystem failed to initialize\n");
}

/* -------------------------------------------------------------------------- */

int main (int argc, char* argv[]) {
	dxf_connection_t connection;

	printf("Connection test started.\n");
	printf("Connecting to host %s...\n", dxfeed_host);

	if (!dxf_create_connection(dxfeed_host, NULL, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();

		return -1;
	}

	printf("Connection successful!\n");

	Sleep(121000);//2m01s

	printf("Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return -1;
	}

	printf("Disconnect successful!\n"
			"Connection test completed successfully!\n");

	return 0;
}

