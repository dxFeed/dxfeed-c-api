#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "Logger.h"
#include <stdio.h>
#include <Windows.h>

static char dxfeed_host_default[] = "demo.dxfeed.com:7300";

HANDLE g_out_console;

dxf_const_string_t dx_event_type_to_string (int event_type) {
	switch (event_type) {
	case DXF_ET_TRADE: return L"Trade";
	case DXF_ET_QUOTE: return L"Quote";
	case DXF_ET_SUMMARY: return L"Summary";
	case DXF_ET_PROFILE: return L"Profile";
	case DXF_ET_ORDER: return L"Order";
	case DXF_ET_TIME_AND_SALE: return L"Time&Sale";
	case DXF_ET_CANDLE: return L"Candle";
	case DXF_ET_TRADE_ETH: return L"TradeETH";
	case DXF_ET_SPREAD_ORDER: return L"SpreadOrder";
	case DXF_ET_GREEKS: return L"Greeks";
	case DXF_ET_THEO_PRICE: return L"THEO_PRICE";
	case DXF_ET_UNDERLYING: return L"Underlying";
	case DXF_ET_SERIES: return L"Series";
	case DXF_ET_CONFIGURATION: return L"Configuration";
	default: return L"";
	}
}

struct event_info_t {
	int                 event_type;
	dxf_event_listener_t listener;
};

#define SYMBOLS_COUNT 63
//static const dx_const_string_t g_symbols[] = { {L"IBM"}, {L"MSFT"}, {L"YHOO"}, {L"C"} };
static dxf_const_string_t g_symbols[] = { L"BP", L"AMD", L"BAC", L"BP", L"C", L"EMC", L"F", L"GE", L"HPQ", L"IBM", L"JPM", L"LCC", L"LSI", L"MGM", L"MO", L"MOT", L"MU",
L"NOK", L"PFE", L"RBS", L"S", L"SAP", L"T", L"VMW", L"WFC", L"XTO", L"BXD", L"BXM", L"BXN", L"BXR", L"BXY", L"CEX", L"CYX",
L"DDA", L"DDB", L"EVZ", L"EXQ", L"FVX", L"GOX", L"GVZ", L"INX", L"IRX", L"OIX", L"OVX", L"RVX", L"TNX", L"TXX", L"TYX", L"VIO",
L"VIX", L"VWA", L"VWB", L"VXB", L"VXD", L"VXN", L"VXO", L"VXV", L"ZIO", L"ZOC", L"OEX", L"SPX", L"XEO", L"XSP" };

static const dxf_int_t g_symbols_size = sizeof (g_symbols) / sizeof (g_symbols[0]);

#define EVENTS_COUNT 4

static dx_trade_t*       trades[SYMBOLS_COUNT] = {0};
static dx_quote_t*       quotes[SYMBOLS_COUNT] = {0};
static dx_summary_t* summaries[SYMBOLS_COUNT] = {0};
static dx_profile_t*     profiles[SYMBOLS_COUNT] = {0};

/* -------------------------------------------------------------------------- */
void trade_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data);
void quote_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data);
void summary_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data);
void profile_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data);

static struct event_info_t event_info[EVENTS_COUNT] = { {DXF_ET_TRADE, trade_listener},
{DXF_ET_QUOTE, quote_listener},
{DXF_ET_SUMMARY, summary_listener},
{DXF_ET_PROFILE, profile_listener}};

/* -------------------------------------------------------------------------- */

static bool is_listener_thread_terminated = false;
CRITICAL_SECTION listener_thread_guard;

bool is_thread_terminate() {
	bool res;
	EnterCriticalSection(&listener_thread_guard);
	res = is_listener_thread_terminated;
	LeaveCriticalSection(&listener_thread_guard);

	return res;
}

/* -------------------------------------------------------------------------- */

void on_reader_thread_terminate (const char* host, void* user_data) {
	EnterCriticalSection(&listener_thread_guard);
	is_listener_thread_terminated = true;
	LeaveCriticalSection(&listener_thread_guard);

	printf("\nTerminating listener thread, host: %s\n", host);
}

/* -------------------------------------------------------------------------- */

int get_symbol_index(dxf_const_string_t symbol_name) {
	int i = 0;
	for (; i < g_symbols_size; ++i) {
		if (wcsicmp(symbol_name, g_symbols[i]) == 0) {
			return i;
		}
	}

	return -1;
}

/* -------------------------------------------------------------------------- */

void output_data(int i) {
	dx_trade_t* trade = trades[i];
	dx_quote_t* quote = quotes[i];
	dx_summary_t* summary = summaries[i];
	dx_profile_t* profile = profiles[i];
	wchar_t dummy[2] = L"-";

	wprintf(L"\nSymbol: %ls, Last: %f, LastEx: %c, Change: %+f, Bid: %f, BidEx: %c, \
			Ask: %f, AskEx: %c, High: %f, Low: %f, Open: %f, BidSize: %i, AskSize: %i, \
			LastSize: %i, LastTick: %i, LastChange: %f, Volume: %i, Description: %ls\n",
			g_symbols[i],
			trade ? trade->price : 0.0,
			trade ? trade->exchange_code : dummy[0],
			(trade && summary) ? (trade->price - summary->prev_day_close_price) : 0.0,
			quote ? quote->bid_price : 0.0,
			quote ? quote->bid_exchange_code : dummy[0],
			quote ? quote->ask_price : 0.0,
			quote ? quote->ask_exchange_code : dummy[0],
			summary ? summary->day_high_price : 0.0,
			summary ? summary->day_low_price : 0.0,
			summary ? summary->day_open_price : 0.0,
			quote ? (int)quote->bid_size : 0,
			quote ? (int)quote->ask_size : 0,
			trade ? (int)trade->size : 0,
			trade ? (int)trade->tick : 0,
			trade ? trade->change : 0.0,
			trade ? (int)trade->day_volume : 0,
			profile ? profile->description : dummy);
}

/* -------------------------------------------------------------------------- */
dxf_event_data_t getData(int event_type, int i) {
	if (event_type == DXF_ET_TRADE) {
		if (trades[i] == NULL) {
			trades[i] = (dx_trade_t*)calloc(1, sizeof(dx_trade_t));
		}

		return trades[i];
	}
	else if (event_type == DXF_ET_QUOTE) {
		if (quotes[i] == NULL) {
			quotes[i] = (dx_quote_t*)calloc(1, sizeof(dx_quote_t));
		}

		return quotes[i];
	}
	else if (event_type == DXF_ET_SUMMARY) {
		if (summaries[i] == NULL) {
			summaries[i] = (dx_summary_t*)calloc(1, sizeof(dx_summary_t));
		}

		return summaries[i];
	}
	else if (event_type == DXF_ET_PROFILE) {
		if (profiles[i] == NULL) {
			profiles[i] = (dx_profile_t*)calloc(1, sizeof(dx_profile_t));
		}

		return profiles[i];
	}
	else return NULL;
}

void trade_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data) {
	dx_trade_t* trades_data = (dx_trade_t*)data;
	dx_trade_t* dst_trade;
	int i = 0;
	int symb_ind = get_symbol_index(symbol_name);
	if (symb_ind == -1) {
		wprintf(L"Error: unexpected symbol: %ls\n", symbol_name);
		return;
	}

	if (event_type != DXF_ET_TRADE) {
		wprintf(L"Error: event: %ls Symbol: %ls, expected event: Trade\n",dx_event_type_to_string(event_type), symbol_name);
		return;
	}

	dst_trade = (dx_trade_t*)getData(event_type, symb_ind);
	if (dst_trade == NULL) {
		wprintf(L"Internal error");
		return;
	}

	for (; i < data_count; ++i) {
		*dst_trade = trades_data[i];
		output_data(symb_ind);
	}
}

/* -------------------------------------------------------------------------- */

void quote_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data) {
	dx_quote_t* quotes_data = (dx_quote_t*)data;
	dx_quote_t* dst_quote;
	int i = 0;
	int symb_ind = get_symbol_index(symbol_name);
	if (symb_ind == -1) {
		wprintf(L"Error: unexpected symbol: %ls\n", symbol_name);
		return;
	}

	if (event_type != DXF_ET_QUOTE) {
		wprintf(L"Error: event: %ls Symbol: %ls, expected event: Trade\n",dx_event_type_to_string(event_type), symbol_name);
		return;
	}

	dst_quote = (dx_quote_t*)getData(event_type, symb_ind);
	if (dst_quote == NULL) {
		wprintf(L"Internal error");
		return;
	}

	for (; i < data_count; ++i) {
		*dst_quote = quotes_data[i];
		output_data(symb_ind);
	}
}

/* -------------------------------------------------------------------------- */

void summary_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data) {
	dx_summary_t* summaries_data = (dx_summary_t*)data;
	dx_summary_t* dst_summary;
	int i = 0;
	int symb_ind = get_symbol_index(symbol_name);
	if (symb_ind == -1) {
		wprintf(L"Error: unexpected symbol: %ls\n", symbol_name);
		return;
	}

	if (event_type != DXF_ET_SUMMARY) {
		wprintf(L"Error: event: %ls Symbol: %ls, expected event: Trade\n",dx_event_type_to_string(event_type), symbol_name);
		return;
	}

	dst_summary = (dx_summary_t*)getData(event_type, symb_ind);
	if (dst_summary == NULL) {
		wprintf(L"Internal error");
		return;
	}

	for (; i < data_count; ++i) {
		*dst_summary = summaries_data[i];
		output_data(symb_ind);
	}
}

/* -------------------------------------------------------------------------- */

void profile_listener(int event_type, dxf_const_string_t symbol_name,
					const dxf_event_data_t* data, int data_count, void* user_data) {
	dx_profile_t* profiles_data = (dx_profile_t*)data;
	dx_profile_t* dst_profile;
	int i = 0;
	int symb_ind = get_symbol_index(symbol_name);
	if (symb_ind == -1) {
		wprintf(L"Error: unexpected symbol: %ls\n", symbol_name);
		return;
	}

	if (event_type != DXF_ET_PROFILE) {
		wprintf(L"Error: event: %ls Symbol: %ls, expected event: Trade\n",dx_event_type_to_string(event_type), symbol_name);
		return;
	}

	dst_profile = (dx_profile_t*)getData(event_type, symb_ind);
	if (dst_profile == NULL) {
		wprintf(L"Internal error");
		return;
	}

	for (; i < data_count; ++i) {
		*dst_profile = profiles_data[i];
		output_data(symb_ind);
	}
}

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

dxf_subscription_t create_subscription(dxf_connection_t connection, int event_id) {
	dxf_subscription_t subscription;
	const int event_type = event_info[event_id].event_type;

	if (!dxf_create_subscription(connection, event_type, &subscription)) {
		process_last_error();

		return NULL;
	};

	if (dxf_add_symbols(subscription, g_symbols, g_symbols_size) != DXF_SUCCESS) {
		return NULL;
	}

	if (!dxf_attach_event_listener(subscription, event_info[event_id].listener, NULL)) {
		process_last_error();

		return NULL;
	};

	return subscription;
}
/* -------------------------------------------------------------------------- */

bool initialize() {
	return true;
}

int main (int argc, char* argv[]) {
	dxf_connection_t connection;
	dxf_subscription_t subscriptions[EVENTS_COUNT];
	int loop_counter = 10000;
	int i;
	char* dxfeed_host = NULL;

	if (argc < 2) {
		dxfeed_host = dxfeed_host_default;
	} else {
		dxfeed_host = argv[1];
	}

	dxf_initialize_logger("quote-table-api.log", true, true, true );

	if (!initialize()) {
		return -1;
	}

	if (!dxf_create_connection(dxfeed_host, on_reader_thread_terminate, NULL, NULL, NULL, NULL, &connection)) {
		process_last_error();
		return -1;
	}

	// create subscriptions
	for (i = 0; i < EVENTS_COUNT; ++i) {
		subscriptions[i] = create_subscription(connection, i);
		if (subscriptions[i] == NULL) {
			return -1;
		}
	}

	// main loop
	InitializeCriticalSection(&listener_thread_guard);
	while (!is_thread_terminate() && loop_counter--) {
		Sleep(100);
	}
	DeleteCriticalSection(&listener_thread_guard);

	printf("Disconnecting from host...\n");

	if (!dxf_close_connection(connection)) {
		process_last_error();

		return -1;
	}

	printf("Disconnect successful!\n"
		"Connection test completed successfully!\n");

	return 0;
}

