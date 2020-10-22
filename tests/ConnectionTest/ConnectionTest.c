#ifdef _WIN32
#	define _CRT_STDIO_ISO_WIDE_SPECIFIERS 1
#endif

#include <stdio.h>

#include "DXErrorCodes.h"
#include "DXFeed.h"

#ifdef _WIN32
#	include <Windows.h>
void dx_sleep(int milliseconds) { Sleep((DWORD)milliseconds); }
#else
#include <time.h>
void dx_sleep(int milliseconds) {
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}
#endif

const char dxfeed_host[] = "demo.dxfeed.com:7300";

/* -------------------------------------------------------------------------- */

void process_last_error() {
	int error_code = dx_ec_success;
	dxf_const_string_t error_descr = NULL;
	int res;

	res = dxf_get_last_error(&error_code, &error_descr);

	if (res == DXF_SUCCESS) {
		if (error_code == dx_ec_success) {
			wprintf(L"No error information is stored");

			return;
		}

		wprintf(
			L"Error occurred and successfully retrieved:\n"
			L"error code = %d, description = \"%ls\"\n",
			error_code, error_descr);
		return;
	}

	wprintf(L"An error occurred but the error subsystem failed to initialize\n");
}

/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	dxf_connection_t connection;

	dxf_initialize_logger("connection-test-api.log", 1, 1, 0);
	wprintf(L"Connection test started.\n");
	wprintf(L"Connecting to host %s...\n", dxfeed_host);

	if (!dxf_create_connection(dxfeed_host, NULL, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();

		return -1;
	}

	wprintf(L"Connection successful!\n");

	dx_sleep(121000);  // 2m01s

	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return -1;
	}

	wprintf(
		L"Disconnect successful!\n"
		L"Connection test completed successfully!\n");

	return 0;
}
