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

#ifdef _WIN32
#	ifndef _CRT_STDIO_ISO_WIDE_SPECIFIERS
#		define _CRT_STDIO_ISO_WIDE_SPECIFIERS 1
#	endif
#endif
#include <math.h>
#include <stdio.h>

#include "DXErrorCodes.h"
#include "DXFeed.h"
#include "EventData.h"
#include "Logger.h"

#ifdef _WIN32
#	include <Windows.h>
void dxs_sleep(int milliseconds) { Sleep((DWORD)milliseconds); }
#else
#	include <time.h>
void dxs_sleep(int milliseconds) {
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}
#endif

const char dxfeed_host[] = "demo.dxfeed.com:7300";

static dxf_const_string_t g_symbols[] = {L"IBM", L"MSFT", L"YHOO", L"C"};
static const dxf_int_t g_symbols_size = sizeof(g_symbols) / sizeof(g_symbols[0]);
static const int g_event_type = DXF_ET_TRADE;
static int g_iteration_count = 10;

/* -------------------------------------------------------------------------- */

dxf_const_string_t dx_event_type_to_string(int event_type) {
	switch (event_type) {
		case DXF_ET_TRADE:
			return L"Trade";
		case DXF_ET_QUOTE:
			return L"Quote";
		case DXF_ET_SUMMARY:
			return L"Summary";
		case DXF_ET_PROFILE:
			return L"Profile";
		case DXF_ET_ORDER:
			return L"Order";
		case DXF_ET_TIME_AND_SALE:
			return L"Time&Sale";
		case DXF_ET_CANDLE:
			return L"Candle";
		case DXF_ET_TRADE_ETH:
			return L"TradeETH";
		case DXF_ET_SPREAD_ORDER:
			return L"SpreadOrder";
		case DXF_ET_GREEKS:
			return L"Greeks";
		case DXF_ET_THEO_PRICE:
			return L"THEO_PRICE";
		case DXF_ET_UNDERLYING:
			return L"Underlying";
		case DXF_ET_SERIES:
			return L"Series";
		case DXF_ET_CONFIGURATION:
			return L"Configuration";
		default:
			return L"";
	}
}

/* -------------------------------------------------------------------------- */

void process_last_error() {
	int error_code = dx_ec_success;
	dxf_const_string_t error_descr = NULL;
	int res;

	res = dxf_get_last_error(&error_code, &error_descr);

	if (res == DXF_SUCCESS) {
		if (error_code == dx_ec_success) {
			wprintf(L"WTF - no error information is stored");

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

void listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
			  void* user_data) {
	dxf_int_t i = 0;
	dx_event_id_t eid = dx_eid_begin;

	for (; (DX_EVENT_BIT_MASK(eid) & event_type) == 0; ++eid)
		;

	wprintf(L"First listener. Event: %ls Symbol: %ls\n", dx_event_type_to_string(event_type), symbol_name);

	if (event_type == DXF_ET_TRADE) {
		dxf_trade_t* trades = (dxf_trade_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"time=%lld, exchange code=%C, price=%f, size=%ld, tick=%ld, change=%f, day volume=%f, scope=%d\n",
					trades[i].time, trades[i].exchange_code, trades[i].price, trades[i].size, trades[i].tick,
					trades[i].change, trades[i].day_volume, (int)trades[i].scope);
		}
	}
}

/* -------------------------------------------------------------------------- */

int main(int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_subscription_t subscription;

	dxf_initialize_logger("last-event-test-api.log", true, true, true);

	wprintf(L"LastEvent test started.\n");
	wprintf(L"Connecting to host %s...\n", dxfeed_host);

	if (!dxf_create_connection(dxfeed_host, NULL, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		return -1;
	}

	wprintf(L"Connection successful!\n");

	wprintf(L"Creating subscription to: Trade\n");
	if (!dxf_create_subscription(connection, g_event_type, &subscription)) {
		process_last_error();

		return -1;
	};

	wprintf(L"Adding symbols:");
	for (int i = 0; i < g_symbols_size; ++i) {
		wprintf(L" %ls", g_symbols[i]);
	}
	wprintf(L"\n");

	if (!dxf_add_symbols(subscription, g_symbols, g_symbols_size)) {
		process_last_error();

		return -1;
	};

	wprintf(L"Listener attached\n");
	if (!dxf_attach_event_listener(subscription, listener, NULL)) {
		process_last_error();

		return -1;
	};

	while (g_iteration_count--) {
		wprintf(L"\nLast event data:");
		for (int i = 0; i < g_symbols_size; ++i) {
			dxf_event_data_t data;
			dxf_trade_t* trade;
			if (!dxf_get_last_event(connection, g_event_type, g_symbols[i], &data)) {
				return -1;
			}

			if (data) {
				trade = (dxf_trade_t*)data;

				wprintf(
					L"\nSymbol: %ls; time=%lld, exchange code=%c, price=%f, size=%ld, tick=%ld, change=%f, day "
					L"volume=%f, scope=%d\n",
					g_symbols[i], trade->time, trade->exchange_code, trade->price, trade->size, trade->tick,
					trade->change, trade->day_volume, (int)trade->scope);
			}
		}
		wprintf(L"\n");
		dxs_sleep(5000);
	}

	wprintf(L"\n Test complete successfully \n");
	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return -1;
	}

	wprintf(
		L"Disconnect successful!\n"
		L"Connection test completed successfully!\n");

	dxs_sleep(5000);

	return 0;
}
