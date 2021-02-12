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

#include <stdio.h>
#include <time.h>

#include "DXErrorCodes.h"
#include "DXFeed.h"
#include "Logger.h"

#ifdef _WIN32
#	pragma warning(push)
#	pragma warning(disable : 5105)
#	include <Windows.h>
#	pragma warning(pop)
void dxs_sleep(int milliseconds) { Sleep((DWORD)milliseconds); }
#else
void dxs_sleep(int milliseconds) {
	struct timespec ts;
	ts.tv_sec = milliseconds / 1000;
	ts.tv_nsec = (milliseconds % 1000) * 1000000;
	nanosleep(&ts, NULL);
}
#endif

const char dxfeed_host[] = "demo.dxfeed.com:7300";

/* -------------------------------------------------------------------------- */

void print_timestamp(dxf_long_t timestamp) {
	wchar_t timefmt[80];

	struct tm* timeinfo;
	time_t tmpint = (time_t)(timestamp / 1000);
	timeinfo = localtime(&tmpint);
	wcsftime(timefmt, 80, L"%Y%m%d-%H%M%S", timeinfo);
	wprintf(L"%ls", timefmt);
}

/* -------------------------------------------------------------------------- */

void first_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					void* user_data) {
	dxf_int_t i = 0;

	wprintf(L"First listener. Event: %ls Symbol: %ls\n", dx_event_type_to_string(event_type), symbol_name);

	if (event_type == DXF_ET_QUOTE) {
		dxf_quote_t* quotes = (dxf_quote_t*)data;

		for (; i < data_count; ++i) {
			wprintf(
				L"bid time=%i, bid exchange code=%c, bid price=%f, bid size=%i; "
				L"ask time=%i, ask exchange code=%c, ask price=%f, ask size=%i, scope=%d\n",
				(int)quotes[i].bid_time, quotes[i].bid_exchange_code, quotes[i].bid_price, (int)quotes[i].bid_size,
				(int)quotes[i].ask_time, quotes[i].ask_exchange_code, quotes[i].ask_price, (int)quotes[i].ask_size,
				(int)quotes[i].scope);
		}
	}

	if (event_type == DXF_ET_ORDER) {
		dxf_order_t* orders = (dxf_order_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"index=%i, side=%i, scope=%i, time=%i, exchange code=%c, market maker=%ls, price=%f, size=%i\n",
					(int)orders[i].index, (int)orders[i].side, (int)orders[i].scope, (int)orders[i].time,
					orders[i].exchange_code, orders[i].market_maker, orders[i].price, (int)orders[i].size);
		}
	}

	if (event_type == DXF_ET_TRADE) {
		dxf_trade_t* trades = (dxf_trade_t*)data;

		for (; i < data_count; ++i) {
			wprintf(L"time=%i, exchange code=%c, price=%f, size=%i, tick=%d, change=%f, day id=%d, day volume=%f, scope=%d\n",
					(int)trades[i].time, trades[i].exchange_code, trades[i].price, (int)trades[i].size, trades[i].tick,
					trades[i].change, trades[i].day_id, trades[i].day_volume, (int)trades[i].scope);
		}
	}

	if (event_type == DXF_ET_SUMMARY) {
		dxf_summary_t* s = (dxf_summary_t*)data;

		for (; i < data_count; ++i) {
			wprintf(
				L"day high price=%f, day low price=%f, day open price=%f, prev day close price=%f, open interest=%i\n",
				s[i].day_high_price, s[i].day_low_price, s[i].day_open_price, s[i].prev_day_close_price,
				(int)s[i].open_interest);
		}
	}

	if (event_type == DXF_ET_PROFILE) {
		dxf_profile_t* p = (dxf_profile_t*)data;

		for (; i < data_count; ++i) {
			wprintf(
				L"Beta=%f, eps=%f, div freq=%i, exd div amount=%f, exd div date=%i, 52 high price=%f, "
				L"52 low price=%f, shares=%f, Description=%ls, flags=%i, status_reason=%ls, halt start time=",
				p[i].beta, p[i].eps, p[i].div_freq, p[i].exd_div_amount, p[i].exd_div_date, p[i].high_52_week_price,
				p[i].low_52_week_price, p[i].shares, p[i].description, p[i].raw_flags, p[i].status_reason);
			print_timestamp(p[i].halt_start_time);
			wprintf(L", halt end time=");
			print_timestamp(p[i].halt_end_time);
			wprintf(L", high limit price=%f, low limit price=%f}\n", p[i].high_limit_price, p[i].low_limit_price);
		}
	}

	if (event_type == DXF_ET_TIME_AND_SALE) {
		dxf_time_and_sale_t* tns = (dxf_time_and_sale_t*)data;

		for (; i < data_count; ++i) {
			wprintf(
				L"event id=%lld, time=%lld, exchange code=%c, price=%f, size=%li, bid price=%f, ask price=%f, "
				L"exchange sale conditions=%ls, is ETH trade=%ls, type=%i\n",
				tns[i].index, tns[i].time, tns[i].exchange_code, tns[i].price, (int)tns[i].size, tns[i].bid_price,
				tns[i].ask_price, tns[i].exchange_sale_conditions, tns[i].is_eth_trade ? L"True" : L"False",
				(int)tns[i].type);
		}
	}
}

/* -------------------------------------------------------------------------- */

void second_listener(int event_type, dxf_const_string_t symbol_name, const dxf_event_data_t* data, int data_count,
					 void* user_data) {
	wprintf(L"Second listener. Event: %ls Symbol: %ls\n", dx_event_type_to_string(event_type), symbol_name);
}

/* -------------------------------------------------------------------------- */

void process_last_error();

void conn_termination_notifier(dxf_connection_t conn, void* user_data) {
	wprintf(L"Asynchronous error occurred, the connection will be reset!\n");

	process_last_error();
}

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
	dxf_subscription_t subscription;

	dxf_string_t symbols_to_add[] = {L"MSFT", L"YHOO", L"C"};
	dxf_int_t symbols_to_add_size = sizeof(symbols_to_add) / sizeof(symbols_to_add[0]);
	dxf_string_t symbols_to_add_2[] = {L"C"};
	dxf_int_t symbols_to_add_2_size = sizeof(symbols_to_add_2) / sizeof(symbols_to_add_2[0]);

	dxf_string_t symbols_to_remove[] = {L"MSFT", L"YHOO", L"IBM"};
	dxf_int_t symbols_to_remove_size = sizeof(symbols_to_remove) / sizeof(symbols_to_remove[0]);

	dxf_string_t symbols_to_set[] = {L"MSFT", L"YHOO"};
	dxf_int_t symbols_to_set_size = sizeof(symbols_to_set) / sizeof(symbols_to_set[0]);

	dxf_int_t get_event_types;

	dxf_initialize_logger("api-test.log", true, true, false);

	wprintf(L"API test started.\n");
	wprintf(L"Connecting to host %hs...\n", dxfeed_host);

	if (!dxf_create_connection(dxfeed_host, conn_termination_notifier, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();

		return 1;
	}

	wprintf(L"Connected\n");
	wprintf(L"Creating subscription to: Trade...\n");

	if (!dxf_create_subscription(connection,
								 DXF_ET_TRADE /* | DXF_ET_QUOTE | DXF_ET_ORDER | DXF_ET_SUMMARY | DXF_ET_PROFILE */,
								 &subscription)) {
		process_last_error();
		dxf_close_connection(connection);

		return 10;
	}

	wprintf(L"Subscribed\n");
	wprintf(L"Attaching the first listener...\n");

	if (!dxf_attach_event_listener(subscription, first_listener, NULL)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 11;
	}

	wprintf(L"First listener has been attached\n");
	wprintf(L"Attaching the second listener...\n");

	if (!dxf_attach_event_listener(subscription, second_listener, NULL)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 12;
	}

	wprintf(L"Second listener has been attached\n");
	wprintf(L"Adding symbols: %ls...\n", L"IBM");

	if (!dxf_add_symbol(subscription, L"IBM")) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 13;
	}

	wprintf(L"Symbol %ls has been added\n", L"IBM");
	wprintf(L"Master thread sleeping for %d ms...\n", 5000);

	dxs_sleep(5000);

	wprintf(L"Master thread woke up\n");
	wprintf(L"Adding symbols: %ls %ls %ls...\n", symbols_to_add[0], symbols_to_add[1], symbols_to_add[2]);

	if (!dxf_add_symbols(subscription, (dxf_const_string_t*)symbols_to_add, symbols_to_add_size)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 14;
	}

	wprintf(L"Symbols have been added\n");
	wprintf(L"Master thread sleeping for %d ms...\n", 5000);

	dxs_sleep(5000);

	wprintf(L"Master thread woke up\n");
	wprintf(L"Detaching second listener...\n");

	if (!dxf_detach_event_listener(subscription, second_listener)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 15;
	}

	wprintf(L"Second listener has been detached\n");
	wprintf(L"Master thread sleeping for %d ms...\n", 5000);

	dxs_sleep(5000);

	wprintf(L"Master thread woke up\n");
	wprintf(L"Clearing symbols...\n");

	if (!dxf_clear_symbols(subscription)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 16;
	}

	wprintf(L"Symbols have been cleared\n");
	wprintf(L"Master thread sleeping for %d ms...\n", 5000);

	dxs_sleep(5000);

	wprintf(L"Master thread woke up\n");
	wprintf(L"Removing symbols: %ls %ls...\n", symbols_to_remove[0], symbols_to_remove[1]);

	if (!dxf_remove_symbols(subscription, (dxf_const_string_t*)symbols_to_remove, symbols_to_remove_size)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 17;
	}

	wprintf(L"Symbols have been removed\n");
	wprintf(L"Master thread sleeping for %d ms...\n", 5000);

	dxs_sleep(5000);

	wprintf(L"Master thread woke up\n");
	wprintf(L"Setting symbols: %ls %ls...\n", symbols_to_set[0], symbols_to_set[1]);

	if (!dxf_set_symbols(subscription, (dxf_const_string_t*)symbols_to_set, symbols_to_set_size)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 18;
	}

	wprintf(L"Symbols have been set\n");
	wprintf(L"Master thread sleeping for %d ms...\n", 5000);

	dxs_sleep(5000);

	wprintf(L"Master thread woke up\n");
	wprintf(L"Retrieving the subscription events...\n");

	if (!dxf_get_subscription_event_types(subscription, &get_event_types)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 19;
	}

	{
		dx_event_id_t eid = dx_eid_begin;

		wprintf(L"\tSubscription events are: ");

		for (; eid < dx_eid_count; ++eid) {
			if (get_event_types & DX_EVENT_BIT_MASK(eid)) {
				wprintf(L"%ls ", dx_event_type_to_string(DX_EVENT_BIT_MASK(eid)));
			}
		}

		wprintf(L"\n");
	}

	wprintf(L"Subscription events have been retrieved\n");
	wprintf(L"Master thread sleeping for %d ms...\n", 5000);

	dxs_sleep(5000);

	wprintf(L"Master thread woke up\n");
	wprintf(L"Adding the new symbols: %ls...\n", symbols_to_add_2[0]);

	if (!dxf_add_symbols(subscription, (dxf_const_string_t*)symbols_to_add_2, symbols_to_add_2_size)) {
		process_last_error();
		dxf_close_subscription(subscription);
		dxf_close_connection(connection);

		return 20;
	}

	wprintf(L"The new symbols have been added\n");
	wprintf(L"Master thread sleeping for %d ms...\n", 1000000);

	dxs_sleep(1000000);

	wprintf(L"Master thread woke up\n");
	wprintf(L"Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return 2;
	}

	wprintf(L"Disconnected\nAPI test completed\n");

	return 0;
}
