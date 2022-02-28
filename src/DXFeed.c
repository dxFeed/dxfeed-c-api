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
#include "DXFeed.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "Candle.h"
#include "ClientMessageProcessor.h"
#include "Configuration.h"
#include "ConnectionContextData.h"
#include "DXAlgorithms.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "DXMemory.h"
#include "DXNetwork.h"
#include "DXThreads.h"
#include "DataStructures.h"
#include "EventSubscription.h"
#include "Logger.h"
#include "PriceLevelBook.h"
#include "PriceLevelBookNG.h"
#include "RegionalBook.h"
#include "ServerMessageProcessor.h"
#include "Snapshot.h"
#include "SymbolCodec.h"

#define DX_KEEP_ERROR  false
#define DX_RESET_ERROR true

#define LS(s)  LS2(s)
#define LS2(s) L##s

static dxf_const_string_t wildcard_symbol = L"*";

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary internal functions
 */
/* -------------------------------------------------------------------------- */

int dx_subscribe(dxf_connection_t connection, dx_order_source_array_ptr_t order_source, dxf_const_string_t *symbols,
				 size_t symbols_count, int *symbols_indices_to_subscribe, int symbols_indices_count, int event_types,
				 dxf_uint_t subscr_flags, dxf_long_t time) {
	return dx_subscribe_symbols_to_events(connection, order_source, symbols, symbols_count,
										  symbols_indices_to_subscribe, symbols_indices_count, event_types, false,
										  false, subscr_flags, time);
}

/* -------------------------------------------------------------------------- */

int dx_unsubscribe(dxf_connection_t connection, dx_order_source_array_ptr_t order_source, dxf_const_string_t *symbols,
				   size_t symbols_count, int event_types, dxf_uint_t subscr_flags, dxf_long_t time) {
	return dx_subscribe_symbols_to_events(connection, order_source, symbols, symbols_count, NULL, 0, event_types, true,
										  false, subscr_flags, time);
}

/* -------------------------------------------------------------------------- */
/*
 *	Delayed tasks data
 */
/* -------------------------------------------------------------------------- */

typedef struct {
	dxf_connection_t *elements;
	size_t size;
	size_t capacity;

	dx_mutex_t guard;
	int guard_initialized;
} dx_connection_array_t;

static dx_connection_array_t g_connection_queue = {0};

/* -------------------------------------------------------------------------- */

int dx_queue_connection_for_close(dxf_connection_t connection) {
	int conn_exists = false;
	size_t conn_index;
	int failed = false;

	if (!g_connection_queue.guard_initialized) {
		return false;
	}

	if (!dx_mutex_lock(&g_connection_queue.guard)) {
		return false;
	}

	DX_ARRAY_SEARCH(g_connection_queue.elements, 0, g_connection_queue.size, connection, DX_NUMERIC_COMPARATOR, false,
					conn_exists, conn_index);

	if (conn_exists) {
		return dx_mutex_unlock(&g_connection_queue.guard);
	}

	DX_ARRAY_INSERT(g_connection_queue, dxf_connection_t, connection, conn_index, dx_capacity_manager_halfer, failed);

	if (failed) {
		dx_mutex_unlock(&g_connection_queue.guard);

		return false;
	}

	return dx_mutex_unlock(&g_connection_queue.guard);
}

/* -------------------------------------------------------------------------- */

void dx_close_queued_connections(void) {
	size_t i = 0;

	if (g_connection_queue.size == 0) {
		return;
	}

	if (!dx_mutex_lock(&g_connection_queue.guard)) {
		return;
	}

	for (; i < g_connection_queue.size; ++i) {
		/* we don't check the result of the operation because this function
		is called implicitly and the user doesn't expect any resource clean-up
		to take place */

		dxf_close_connection(g_connection_queue.elements[i]);
	}

	g_connection_queue.size = 0;
	// TODO: g_connection_queue is not free

	dx_mutex_unlock(&g_connection_queue.guard);
}

/* -------------------------------------------------------------------------- */

void dx_init_connection_queue(void) {
	static int initialized = false;

	if (!initialized) {
		initialized = true;

		g_connection_queue.guard_initialized = dx_mutex_create(&g_connection_queue.guard);
	}
}

/* -------------------------------------------------------------------------- */

void dx_perform_implicit_tasks(void) {
	dx_init_connection_queue();
	dx_close_queued_connections();
}

/* -------------------------------------------------------------------------- */

ERRORCODE dx_perform_common_actions(int resetError) {
	dx_perform_implicit_tasks();

	if (resetError && !dx_pop_last_error()) {
		return DXF_FAILURE;
	}

	return DXF_SUCCESS;
}

static ERRORCODE dx_init_codec() {
	static int symbol_codec_initialized = false;
	if (!symbol_codec_initialized) {
		symbol_codec_initialized = true;
		if (dx_init_symbol_codec() != true) {
			return DXF_FAILURE;
		}
	}
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

ERRORCODE dx_close_subscription(dxf_subscription_t subscription, int resetError) {
	dxf_connection_t connection;
	unsigned events;

	dxf_const_string_t *symbols;
	size_t symbol_count;
	dx_event_subscr_flag subscr_flags;
	dxf_long_t time;

	dx_perform_common_actions(resetError);

	if (subscription == dx_invalid_subscription) {
		dx_set_error_code(dx_ec_invalid_func_param);

		return DXF_FAILURE;
	}

	if (!dx_get_subscription_connection(subscription, &connection) ||
		!dx_get_event_subscription_event_types(subscription, &events) ||
		!dx_get_event_subscription_symbols(subscription, &symbols, &symbol_count) ||
		!dx_get_event_subscription_flags(subscription, &subscr_flags) ||
		!dx_get_event_subscription_time(subscription, &time) ||
		!dx_unsubscribe(connection, dx_get_order_sources(subscription), symbols, symbol_count, (int)events,
						subscr_flags, time) ||
		!dx_close_event_subscription(subscription)) {
		return DXF_FAILURE;
	}

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed API constants
 */
/* -------------------------------------------------------------------------- */

#define DEFAULT_SUBSCRIPTION_TIME 0

/* -------------------------------------------------------------------------- */
/*
 *	DXFeed API implementation
 */
/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_connection_impl(const char *address, const char *authscheme, const char *authdata,
												dxf_conn_termination_notifier_t notifier,
												dxf_conn_status_notifier_t conn_status_notifier,
												dxf_socket_thread_creation_notifier_t stcn,
												dxf_socket_thread_destruction_notifier_t stdn, void *user_data,
												OUT dxf_connection_t *connection) {
	dx_connection_context_data_t ccd;

	dx_perform_common_actions(DX_RESET_ERROR);

	if (connection == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);

		return DXF_FAILURE;
	}

	if (!dx_validate_connection_handle(*connection = dx_init_connection(), false)) {
		return DXF_FAILURE;
	}

	if (authscheme != NULL && authdata != NULL) {
		if (!dx_protocol_configure_custom_auth(*connection, authscheme, authdata)) {
			dx_deinit_connection(*connection);
			*connection = NULL;
			return DXF_FAILURE;
		}
	}

	{
		dxf_string_t w_host = dx_ansi_to_unicode(address);

		if (w_host != NULL) {
			dx_free(w_host);
		}
	}

	ccd.receiver = dx_socket_data_receiver;
	ccd.notifier = notifier;
	ccd.conn_status_notifier = conn_status_notifier;
	ccd.stcn = stcn;
	ccd.stdn = stdn;
	ccd.notifier_user_data = user_data;
	ccd.on_server_heartbeat_notifier = NULL;
	ccd.on_server_heartbeat_notifier_user_data = NULL;

	if (!dx_bind_to_address(*connection, address, &ccd) || !dx_send_protocol_description(*connection, false) ||
		!dx_send_record_description(*connection, false)) {
		dx_deinit_connection(*connection);

		*connection = NULL;

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_info, L"Return connection at %p", *connection);
	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_connection(const char *address, dxf_conn_termination_notifier_t notifier,
										   dxf_conn_status_notifier_t conn_status_notifier,
										   dxf_socket_thread_creation_notifier_t stcn,
										   dxf_socket_thread_destruction_notifier_t stdn, void *user_data,
										   OUT dxf_connection_t *connection) {
	dx_logging_verbose(dx_ll_debug, L"dxf_create_connection(address = '%hs', ...)", address);

	ERRORCODE result = dxf_create_connection_impl(address, NULL, NULL, notifier, conn_status_notifier, stcn, stdn,
												  user_data, OUT connection);

	dx_logging_verbose(dx_ll_debug, L"dxf_create_connection(address = '%hs', ...) -> %d, %p", address, result,
					   (connection != NULL) ? *connection : NULL);

	return result;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_connection_auth_basic(const char *address, const char *user, const char *password,
													  dxf_conn_termination_notifier_t notifier,
													  dxf_conn_status_notifier_t conn_status_notifier,
													  dxf_socket_thread_creation_notifier_t stcn,
													  dxf_socket_thread_destruction_notifier_t stdn, void *user_data,
													  OUT dxf_connection_t *connection) {
	dx_logging_verbose(dx_ll_debug, L"dxf_create_connection_auth_basic(address = '%hs', user = '%hs'...)", address,
					   user);

	char *base64_buf;

	if (user == NULL || password == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_connection_auth_basic(address = '%hs', user = '%hs'...) -> %d, 'The login or "
						   L"password is NULL'",
						   address, user, DXF_FAILURE);

		return DXF_FAILURE;
	}

	base64_buf = dx_protocol_get_basic_auth_data(user, password);
	if (base64_buf == NULL) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_connection_auth_basic(address = '%hs', user = '%hs'...) -> %d, 'Error creating "
						   L"base64 basic authentication pair'",
						   address, user, DXF_FAILURE);

		return DXF_FAILURE;
	}

	ERRORCODE res = dxf_create_connection_auth_custom(address, DX_AUTH_BASIC_KEY, base64_buf, notifier,
													  conn_status_notifier, stcn, stdn, user_data, connection);

	dx_free(base64_buf);

	dx_logging_verbose(dx_ll_debug, L"dxf_create_connection_auth_basic(address = '%hs', user = '%hs'...) -> %d, %p",
					   address, user, res, (connection != NULL) ? *connection : NULL);
	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_connection_auth_bearer(const char *address, const char *token,
													   dxf_conn_termination_notifier_t notifier,
													   dxf_conn_status_notifier_t conn_status_notifier,
													   dxf_socket_thread_creation_notifier_t stcn,
													   dxf_socket_thread_destruction_notifier_t stdn, void *user_data,
													   OUT dxf_connection_t *connection) {
	dx_logging_verbose(dx_ll_debug, L"dxf_create_connection_auth_bearer(address = '%hs', token = '%hs'...)", address,
					   token);

	if (token == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_create_connection_auth_bearer(address = '%hs', token = '%hs'...) -> %d, 'The token is NULL'", address,
			token, DXF_FAILURE);

		return DXF_FAILURE;
	}

	ERRORCODE res = dxf_create_connection_auth_custom(address, DX_AUTH_BEARER_KEY, token, notifier,
													  conn_status_notifier, stcn, stdn, user_data, connection);
	dx_logging_verbose(dx_ll_debug, L"dxf_create_connection_auth_bearer(address = '%hs', token = '%hs'...) -> %d, %p",
					   address, token, res, (connection != NULL) ? *connection : NULL);

	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_connection_auth_custom(const char *address, const char *authscheme,
													   const char *authdata, dxf_conn_termination_notifier_t notifier,
													   dxf_conn_status_notifier_t conn_status_notifier,
													   dxf_socket_thread_creation_notifier_t stcn,
													   dxf_socket_thread_destruction_notifier_t stdn, void *user_data,
													   OUT dxf_connection_t *connection) {
	dx_logging_verbose(dx_ll_debug, L"dxf_create_connection_auth_custom(address = '%hs', auth scheme = '%hs'...)",
					   address, authscheme);

	if (authscheme == NULL || authdata == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);

		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_connection_auth_custom(address = '%hs', auth scheme = '%hs'...) -> %d, 'The "
						   L"auth scheme or auth data is NULL'",
						   address, authscheme, DXF_FAILURE);
		return DXF_FAILURE;
	}

	ERRORCODE res = dxf_create_connection_impl(address, authscheme, authdata, notifier, conn_status_notifier, stcn,
											   stdn, user_data, OUT connection);
	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_connection_auth_custom(address = '%hs', auth scheme = '%hs'...) -> %d, %p", address,
					   authscheme, res, (connection != NULL) ? *connection : NULL);

	return res;
}

DXFEED_API ERRORCODE dxf_set_on_server_heartbeat_notifier_impl(dxf_connection_t connection,
															   dxf_conn_on_server_heartbeat_notifier_t notifier,
															   void *user_data) {
	dx_perform_common_actions(DX_RESET_ERROR);
	if (!dx_validate_connection_handle(connection, false)) {
		return DXF_FAILURE;
	}

	return dx_set_on_server_heartbeat_notifier(connection, notifier, user_data);
}

DXFEED_API ERRORCODE dxf_set_on_server_heartbeat_notifier(dxf_connection_t connection,
														  dxf_conn_on_server_heartbeat_notifier_t notifier,
														  void *user_data) {
	dx_logging_verbose(dx_ll_debug, L"dxf_set_on_server_heartbeat_notifier(con = %p...)", connection);

	ERRORCODE res = dxf_set_on_server_heartbeat_notifier_impl(connection, notifier, user_data);

	dx_logging_verbose(dx_ll_debug, L"dxf_set_on_server_heartbeat_notifier(con = %p...) -> %d", connection, res);

	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_close_connection(dxf_connection_t connection) {
	dx_logging_verbose(dx_ll_debug, L"dxf_close_connection(con = %p)", connection);

	if (!dx_validate_connection_handle(connection, false)) {
		dx_logging_verbose(dx_ll_debug, L"dxf_close_connection(con = %p) -> %d", connection, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_can_deinit_connection(connection)) {
		dx_queue_connection_for_close(connection);
		dx_logging_verbose(dx_ll_debug, L"dxf_close_connection(con = %p) -> %d", connection, DXF_SUCCESS);

		return DXF_SUCCESS;
	}

	dx_set_is_closing(connection);
	int res = dx_deinit_connection(connection);
	dx_logging_verbose(dx_ll_debug, L"dxf_close_connection(con = %p) -> %d", connection,
					   (res ? DXF_SUCCESS : DXF_FAILURE));

	return (res ? DXF_SUCCESS : DXF_FAILURE);
}

/* -------------------------------------------------------------------------- */

ERRORCODE dxf_create_subscription_impl(dxf_connection_t connection, int event_types, dx_event_subscr_flag subscr_flags,
									   dxf_long_t time, OUT dxf_subscription_t *subscription) {
	dx_perform_common_actions(DX_RESET_ERROR);

	if (!dx_init_codec()) {
		return DXF_FAILURE;
	}

	if (!dx_validate_connection_handle(connection, false)) {
		return DXF_FAILURE;
	}

	if (subscription == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		return DXF_FAILURE;
	}

	if ((*subscription = dx_create_event_subscription(connection, event_types, subscr_flags, time)) ==
		dx_invalid_subscription) {
		return DXF_FAILURE;
	}

	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_create_subscription(dxf_connection_t connection, int event_types,
											 OUT dxf_subscription_t *subscription) {
	dx_logging_verbose(dx_ll_debug, L"dxf_create_subscription(con = %p, event types = 0x%X ...)", connection,
					   event_types);

	ERRORCODE res =
		dxf_create_subscription_impl(connection, event_types, dx_esf_default, DEFAULT_SUBSCRIPTION_TIME, subscription);
	dx_logging_verbose(dx_ll_debug, L"dxf_create_subscription(con = %p, event types = 0x%X ...) -> %d, %p", connection,
					   event_types, res, (subscription != NULL) ? *subscription : NULL);

	return res;
}

DXFEED_API ERRORCODE dxf_create_subscription_with_flags(dxf_connection_t connection, int event_types,
														dx_event_subscr_flag subscr_flags,
														OUT dxf_subscription_t *subscription) {
	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_subscription_with_flags(con = %p, event types = 0x%X, subscr flags = 0x%X ...)",
					   connection, event_types, subscr_flags);

	ERRORCODE res =
		dxf_create_subscription_impl(connection, event_types, subscr_flags, DEFAULT_SUBSCRIPTION_TIME, subscription);

	dx_logging_verbose(
		dx_ll_debug,
		L"dxf_create_subscription_with_flags(con = %p, event types = 0x%X, subscr flags = 0x%X ...) -> %d, %p",
		connection, event_types, subscr_flags, res, (subscription != NULL) ? *subscription : NULL);

	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_subscription_timed(dxf_connection_t connection, int event_types, dxf_long_t time,
												   OUT dxf_subscription_t *subscription) {
	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_subscription_timed(con = %p, event types = 0x%X, time = %" LS(PRId64) L" ...)",
					   connection, event_types, time);

	ERRORCODE res = dxf_create_subscription_impl(connection, event_types, dx_esf_time_series, time, subscription);

	dx_logging_verbose(
		dx_ll_debug,
		L"dxf_create_subscription_timed(con = %p, event types = 0x%X, time = %" LS(PRId64) L" ...) -> %d, %p",
		connection, event_types, time, res, (subscription != NULL) ? *subscription : NULL);

	return res;
}

DXFEED_API ERRORCODE dxf_create_subscription_timed_with_flags(dxf_connection_t connection, int event_types,
															  dxf_long_t time, dx_event_subscr_flag subscr_flags,
															  OUT dxf_subscription_t *subscription) {
	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_subscription_timed_with_flags(con = %p, event types = 0x%X, time = %" LS(
						   PRId64) L", subscr flags = 0x%X ...)",
					   connection, event_types, time, subscr_flags);

	ERRORCODE res =
		dxf_create_subscription_impl(connection, event_types, dx_esf_time_series | subscr_flags, time, subscription);

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_subscription_timed_with_flags(con = %p, event types = 0x%X, time = %" LS(
						   PRId64) L", subscr flags = 0x%X ...) -> %d, %p",
					   connection, event_types, time, subscr_flags, res, (subscription != NULL) ? *subscription : NULL);

	return res;
}

DXFEED_API ERRORCODE dxf_close_subscription(dxf_subscription_t subscription) {
	dx_logging_verbose(dx_ll_debug, L"dxf_close_subscription(sub = %p)", subscription);

	ERRORCODE res = dx_close_subscription(subscription, DX_RESET_ERROR);

	dx_logging_verbose(dx_ll_debug, L"dxf_close_subscription(sub = %p) -> %d", subscription, res);

	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_add_symbol(dxf_subscription_t subscription, dxf_const_string_t symbol) {
	dx_logging_verbose(dx_ll_debug, L"dxf_add_symbol(sub = %p, symbol = '%ls')", subscription,
					   (symbol == NULL) ? L"NULL" : symbol);

	ERRORCODE res = dxf_add_symbols(subscription, &symbol, 1);

	dx_logging_verbose(dx_ll_debug, L"dxf_add_symbol(sub = %p, symbol = '%ls') -> %d", subscription,
					   (symbol == NULL) ? L"NULL" : symbol, res);

	return res;
}

/* -------------------------------------------------------------------------- */

#define DX_WILDCARD_COMPARATOR(l, r) (dx_compare_strings_num(l, r, 1))

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_add_symbols(dxf_subscription_t subscription, dxf_const_string_t *symbols, int symbol_count) {
	dx_perform_common_actions(DX_RESET_ERROR);

	dx_logging_verbose(dx_ll_debug, L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...)", subscription, symbol_count,
					   (symbols == NULL) ? L"NULL" : (symbol_count > 0 ? symbols[0] : L""));

	if (subscription == dx_invalid_subscription || symbols == NULL || symbol_count < 0) {
		dx_set_error_code(dx_ec_invalid_func_param);

		dx_logging_verbose(dx_ll_debug,
						   L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'The subscription is invalid or "
						   L"symbols is NULL or symbols count < 0'",
						   subscription, symbol_count,
						   (symbols == NULL) ? L"NULL" : (symbol_count > 0 ? symbols[0] : L""), DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (symbol_count == 0) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'The symbols count == 0'",
						   subscription, symbol_count, L"", DXF_SUCCESS);
		return DXF_SUCCESS;
	}

	dxf_uint_t events;
	dxf_connection_t connection;
	dx_event_subscr_flag subscr_flags;

	if (!dx_get_subscription_connection(subscription, &connection) ||
		!dx_get_event_subscription_event_types(subscription, &events) ||
		!dx_get_event_subscription_flags(subscription, &subscr_flags)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Unable to get connection "
						   L"information or event types or subscription flags'",
						   subscription, symbol_count, symbols[0], DXF_FAILURE);
		return DXF_FAILURE;
	}

	if (IS_FLAG_SET(subscr_flags, dx_esf_wildcard)) {
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Already subscribed to the wildcard (*)'",
			subscription, symbol_count, symbols[0], DXF_SUCCESS);
		return DXF_SUCCESS;
	}

	dxf_const_string_t *subscr_symbols = symbols;
	int subscr_symbol_count = symbol_count;
	int found_wildcard;
	size_t index;
	(void)index;

	DX_ARRAY_SEARCH(symbols, 0, symbol_count, wildcard_symbol, DX_WILDCARD_COMPARATOR, false, found_wildcard, index);

	if (found_wildcard) {
		subscr_symbols = &wildcard_symbol;
		subscr_symbol_count = 1;

		if (!dxf_clear_symbols(subscription)) {
			dx_logging_verbose(dx_ll_debug,
							   L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Tried unsuccessfully to "
							   L"clear symbols to subscribe to wildcard (*) '",
							   subscription, symbol_count, symbols[0], DXF_FAILURE);

			return DXF_FAILURE;
		}

		subscr_flags ^= dx_esf_wildcard;
	}

	dxf_long_t time;

	int *added_symbols_indices = dx_calloc(subscr_symbol_count, sizeof(int));
	int added_symbols_count = 0;

	if (!dx_get_event_subscription_time(subscription, &time) ||
		!dx_add_symbols_v2(subscription, subscr_symbols, subscr_symbol_count, added_symbols_indices,
						   &added_symbols_count)) {
		dx_free(added_symbols_indices);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Unsuccessfully attempted to add "
						   L"symbols to the subscription'",
						   subscription, symbol_count, symbols[0], DXF_FAILURE);
		return DXF_FAILURE;
	}

	if (added_symbols_count == 0) {
		dx_free(added_symbols_indices);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'The added symbols are already "
						   L"in the subscription'",
						   subscription, symbol_count, symbols[0], DXF_SUCCESS);
		return DXF_SUCCESS;
	}

	if (!dx_load_events_for_subscription(connection, dx_get_order_sources(subscription), (int)events, subscr_flags) ||
		!dx_send_record_description(connection, false) ||
		!dx_subscribe(connection, dx_get_order_sources(subscription), subscr_symbols, subscr_symbol_count,
					  added_symbols_indices, added_symbols_count, (int)events, subscr_flags, time)) {
		dx_free(added_symbols_indices);
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Tried unsuccessfully to subscribe to symbols'",
			subscription, symbol_count, symbols[0], DXF_FAILURE);
		return DXF_FAILURE;
	}

	dx_free(added_symbols_indices);
	dx_logging_verbose(dx_ll_debug, L"dxf_add_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d", subscription,
					   symbol_count, symbols[0], DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_remove_symbol(dxf_subscription_t subscription, dxf_const_string_t symbol) {
	dx_logging_verbose(dx_ll_debug, L"dxf_remove_symbol(sub = %p, symbol = '%ls')", subscription, symbol);

	ERRORCODE res = dxf_remove_symbols(subscription, &symbol, 1);

	dx_logging_verbose(dx_ll_debug, L"dxf_remove_symbol(sub = %p, symbol = '%ls') -> %d", subscription, symbol, res);

	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_remove_symbols(dxf_subscription_t subscription, dxf_const_string_t *symbols,
										int symbol_count) {
	dx_perform_common_actions(DX_RESET_ERROR);

	dx_logging_verbose(dx_ll_debug, L"dxf_remove_symbols(sub = %p, symbols[%d] = '%ls'...)", subscription, symbol_count,
					   (symbols == NULL) ? L"NULL" : (symbol_count > 0 ? symbols[0] : L""));

	if (subscription == dx_invalid_subscription || symbols == NULL || symbol_count < 0) {
		dx_set_error_code(dx_ec_invalid_func_param);

		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_remove_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'The subscription is invalid or "
			L"symbols is NULL or symbols count < 0'",
			subscription, symbol_count, (symbols == NULL) ? L"NULL" : (symbol_count > 0 ? symbols[0] : L""),
			DXF_FAILURE);

		return DXF_FAILURE;
	}

	size_t current_symbol_count;

	if (!dx_get_event_subscription_symbols_count(subscription, &current_symbol_count)) {
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_remove_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Unable to get current symbols count'",
			subscription, symbol_count, symbols[0], DXF_FAILURE);

		return DXF_FAILURE;
	}

	// The QD API does not allow us to unsubscribe from symbols if we have not made any subscriptions in this session.
	// Let's use a simple check to remove symbols so that we don't use a flag for each subscription.
	if (current_symbol_count == 0u) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_remove_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Current symbols count == 0'",
						   subscription, symbol_count, L"", DXF_SUCCESS);

		return DXF_SUCCESS;
	}

	int found_wildcard;
	size_t index;
	(void)index;

	DX_ARRAY_SEARCH(symbols, 0, symbol_count, wildcard_symbol, DX_WILDCARD_COMPARATOR, false, found_wildcard, index);

	if (found_wildcard) {
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_remove_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'An attempt to unsubscribe from "
			L"a wildcard (*), unsubscribing from all symbols. '",
			subscription, symbol_count, symbols[0], DXF_SUCCESS);

		return dxf_clear_symbols(subscription);
	}

	dxf_connection_t connection;
	unsigned events;
	dx_event_subscr_flag subscr_flags;

	if (!dx_get_subscription_connection(subscription, &connection) ||
		!dx_get_event_subscription_event_types(subscription, &events) ||
		!dx_get_event_subscription_flags(subscription, &subscr_flags)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_remove_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Unable to get connection "
						   L"information or event types or subscription flags'",
						   subscription, symbol_count, symbols[0], DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (IS_FLAG_SET(subscr_flags, dx_esf_wildcard)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_remove_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'An attempt to unsubscribe "
						   L"from symbols, despite the fact that there is a subscription to a wildcard. Skipped.'",
						   subscription, symbol_count, symbols[0], DXF_SUCCESS);

		return DXF_SUCCESS;
	}

	dxf_long_t time;

	if (!dx_get_event_subscription_time(subscription, &time) ||
		!dx_unsubscribe(connection, dx_get_order_sources(subscription), symbols, symbol_count, (int)events,
						subscr_flags, time) ||
		!dx_remove_symbols(subscription, symbols, symbol_count)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_remove_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Tried unsuccessfully to "
						   L"unsubscribe from symbols'",
						   subscription, symbol_count, symbols[0], DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_remove_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d", subscription,
					   symbol_count, symbols[0], DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_symbols(dxf_subscription_t subscription, OUT dxf_const_string_t **symbols,
									 OUT int *symbol_count) {
	dx_perform_common_actions(DX_RESET_ERROR);

	dx_logging_verbose(dx_ll_debug, L"dxf_get_symbols(sub = %p, symbols = %p, count = %p)", subscription, symbols,
					   symbol_count);

	if (subscription == dx_invalid_subscription || symbols == NULL || symbol_count == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);

		dx_logging_verbose(dx_ll_debug,
						   L"dxf_get_symbols(sub = %p, symbols = %p, count = %p) -> %d, 'Tried unsuccessfully to "
						   L"retrieve the subscription symbols'",
						   subscription, symbols, symbol_count, DXF_FAILURE);

		return DXF_FAILURE;
	}

	size_t symbols_size = 0;

	if (!dx_get_event_subscription_symbols(subscription, symbols, &symbols_size)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_get_symbols(sub = %p, symbols = %p, count = %p) -> %d, 'The subscription is invalid "
						   L"or symbols is NULL or symbols count is NULL'",
						   subscription, symbols, symbol_count, DXF_FAILURE);

		return DXF_FAILURE;
	}

	*symbol_count = (int)symbols_size;

	dx_logging_verbose(dx_ll_debug, L"dxf_get_symbols(sub = %p, symbols = %p, count = %p {*count = %d}) -> %d",
					   subscription, symbols, symbol_count, *symbol_count, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_set_symbols(dxf_subscription_t subscription, dxf_const_string_t *symbols, int symbol_count) {
	dx_perform_common_actions(DX_RESET_ERROR);

	dx_logging_verbose(dx_ll_debug, L"dxf_set_symbols(sub = %p, symbols[%d] = '%ls'...)", subscription, symbol_count,
					   (symbols == NULL) ? L"NULL" : (symbol_count > 0 ? symbols[0] : L""));

	if (subscription == dx_invalid_subscription || symbols == NULL || symbol_count < 0) {
		dx_set_error_code(dx_ec_invalid_func_param);

		dx_logging_verbose(dx_ll_debug,
						   L"dxf_set_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'The subscription is invalid or "
						   L"symbols is NULL or symbols count < 0'",
						   subscription, symbol_count,
						   (symbols == NULL) ? L"NULL" : (symbol_count > 0 ? symbols[0] : L""), DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (dxf_clear_symbols(subscription) == DXF_FAILURE ||
		dxf_add_symbols(subscription, symbols, symbol_count) == DXF_FAILURE) {
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_set_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d, 'Tried unsuccessfully to clear and add symbols'",
			subscription, symbol_count, symbols[0], DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_set_symbols(sub = %p, symbols[%d] = '%ls'...) -> %d", subscription,
					   symbol_count, symbols[0], DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_clear_symbols(dxf_subscription_t subscription) {
	dxf_connection_t connection;
	unsigned events;

	dxf_const_string_t *symbols;
	size_t symbol_count;
	dx_event_subscr_flag subscr_flags;
	dxf_long_t time;

	dx_logging_verbose(dx_ll_debug, L"dxf_clear_symbols(sub = %p)", subscription);

	dx_perform_common_actions(DX_RESET_ERROR);

	if (subscription == dx_invalid_subscription) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug, L"dxf_clear_symbols(sub = %p) -> %d, 'The subscription is invalid'",
						   subscription, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_get_event_subscription_symbols(subscription, &symbols, &symbol_count) ||
		!dx_get_subscription_connection(subscription, &connection) ||
		!dx_get_event_subscription_event_types(subscription, &events) ||
		!dx_get_event_subscription_flags(subscription, &subscr_flags) ||
		!dx_get_event_subscription_time(subscription, &time) ||
		!dx_unsubscribe(connection, dx_get_order_sources(subscription), symbols, symbol_count, (int)events,
						subscr_flags, time) ||
		!dx_remove_symbols(subscription, symbols, symbol_count)) {
		dx_logging_verbose(dx_ll_debug, L"dxf_clear_symbols(sub = %p) -> %d, 'Tried unsuccessfully to remove symbols'",
						   subscription, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (IS_FLAG_SET(subscr_flags, dx_esf_wildcard)) {
		subscr_flags ^= dx_esf_wildcard;

		if (!dx_set_event_subscription_flags(subscription, subscr_flags)) {
			dx_logging_verbose(
				dx_ll_debug,
				L"dxf_clear_symbols(sub = %p) -> %d, 'Tried unsuccessfully to unset wildcard subscription flag'",
				subscription, DXF_FAILURE);

			return DXF_FAILURE;
		}
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_clear_symbols(sub = %p) -> %d", subscription, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_attach_event_listener(dxf_subscription_t subscription, dxf_event_listener_t event_listener,
											   void *user_data) {
	dx_logging_verbose(dx_ll_debug, L"dxf_attach_event_listener(sub = %p, listener = %p, user_data = %p)", subscription,
					   event_listener, user_data);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (subscription == dx_invalid_subscription || event_listener == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_attach_event_listener(sub = %p, listener = %p, user_data = %p) -> %d, 'The subscription is "
			L"invalid or listener is NULL'",
			subscription, event_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_add_listener(subscription, event_listener, user_data)) {
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_attach_event_listener(sub = %p, listener = %p, user_data = %p) -> %d, 'Tried unsuccessfully "
			L"to attach the listener'",
			subscription, event_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_attach_event_listener(sub = %p, listener = %p, user_data = %p) -> %d",
					   subscription, event_listener, user_data, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_detach_event_listener(dxf_subscription_t subscription, dxf_event_listener_t event_listener) {
	dx_logging_verbose(dx_ll_debug, L"dxf_detach_event_listener(sub = %p, listener = %p)", subscription,
					   event_listener);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (subscription == dx_invalid_subscription || event_listener == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_event_listener(sub = %p, listener = %p) -> %d, 'The subscription is "
						   L"invalid or listener is NULL'",
						   subscription, event_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_remove_listener(subscription, event_listener)) {
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_detach_event_listener(sub = %p, listener = %p) -> %d, 'Tried unsuccessfully to detach the listener'",
			subscription, event_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_detach_event_listener(sub = %p, listener = %p) -> %d", subscription,
					   event_listener, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_attach_event_listener_v2(dxf_subscription_t subscription,
												  dxf_event_listener_v2_t event_listener, void *user_data) {
	dx_logging_verbose(dx_ll_debug, L"dxf_attach_event_listener_v2(sub = %p, listener = %p, user_data = %p)",
					   subscription, event_listener, user_data);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (subscription == dx_invalid_subscription || event_listener == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_event_listener_v2(sub = %p, listener = %p) -> %d, 'The subscription is "
						   L"invalid or listener is NULL'",
						   subscription, event_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_add_listener_v2(subscription, event_listener, user_data)) {
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_attach_event_listener_v2(sub = %p, listener = %p, user_data = %p) -> %d, 'Tried unsuccessfully "
			L"to attach the listener'",
			subscription, event_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_attach_event_listener_v2(sub = %p, listener = %p, user_data = %p) -> %d",
					   subscription, event_listener, user_data, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_detach_event_listener_v2(dxf_subscription_t subscription,
												  dxf_event_listener_v2_t event_listener) {
	dx_logging_verbose(dx_ll_debug, L"dxf_detach_event_listener_v2(sub = %p, listener = %p)", subscription,
					   event_listener);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (subscription == dx_invalid_subscription || event_listener == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_event_listener_v2(sub = %p, listener = %p) -> %d, 'The subscription is "
						   L"invalid or listener is NULL'",
						   subscription, event_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_remove_listener_v2(subscription, event_listener)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_event_listener_v2(sub = %p, listener = %p) -> %d, 'Tried unsuccessfully to "
						   L"detach the listener'",
						   subscription, event_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_detach_event_listener_v2(sub = %p, listener = %p) -> %d", subscription,
					   event_listener, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_subscription_event_types(dxf_subscription_t subscription, OUT int *event_types) {
	dx_logging_verbose(dx_ll_debug, L"dxf_get_subscription_event_types(sub = %p, event_types = %p)", subscription,
					   event_types);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (subscription == dx_invalid_subscription || event_types == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_get_subscription_event_types(sub = %p, event_types = %p) -> %d, 'The subscription is "
						   L"invalid or event_types is NULL'",
						   subscription, event_types, DXF_FAILURE);

		return DXF_FAILURE;
	}

	unsigned tmp_event_types;

	if (!dx_get_event_subscription_event_types(subscription, &tmp_event_types)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_get_subscription_event_types(sub = %p, event_types = %p) -> %d, 'Tried unsuccessfully "
						   L"to retrieve the event types'",
						   subscription, event_types, DXF_FAILURE);

		return DXF_FAILURE;
	}

	*event_types = (int)tmp_event_types;

	dx_logging_verbose(dx_ll_debug, L"dxf_get_subscription_event_types(sub = %p, event_types = %p) -> %d, 0x%0X",
					   subscription, event_types, DXF_SUCCESS, *event_types);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_last_event(dxf_connection_t connection, int event_type, dxf_const_string_t symbol,
										OUT dxf_event_data_t *data) {
	dx_logging_verbose(dx_ll_debug, L"dxf_get_last_event(con = %p, event_type = %d, symbol = '%ls', event data = %p)",
					   connection, event_type, symbol, data);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (!dx_validate_connection_handle(connection, false)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_get_last_event(con = %p, event_type = %d, symbol = '%ls', event data = %p) -> %d, "
						   L"'Invalid connection handle'",
						   connection, event_type, symbol, data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (data == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_get_last_event(con = %p, event_type = %d, symbol = '%ls', event data = %p) -> %d, 'The "
			L"event data ptr is NULL'",
			connection, event_type, symbol, data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_get_last_symbol_event(connection, symbol, event_type, data)) {
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_get_last_event(con = %p, event_type = %d, symbol = '%ls', event data = %p) -> %d, 'Tried "
			L"unsuccessfully to retrieve the event data'",
			connection, event_type, symbol, data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_get_last_event(con = %p, event_type = %d, symbol = '%ls', event data = %p) -> %d",
					   connection, event_type, symbol, data, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_last_error(OUT int *error_code, OUT dxf_const_string_t *error_descr) {
	dx_logging_verbose(dx_ll_debug, L"dxf_get_last_error(error_code = %p, error_descr = %p)", error_code, error_descr);

	if (error_code == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_get_last_error(error_code = %p, error_descr = %p) -> %d, 'The error code ptr is NULL'",
						   error_code, error_descr, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (dx_get_last_error(error_code) == dx_efr_success) {
		if (error_descr != NULL) {
			*error_descr = dx_get_error_description(*error_code);
		}

		dx_logging_verbose(dx_ll_debug, L"dxf_get_last_error(error_code = %p, error_descr = %p) -> %d, %d, '%ls'",
						   error_code, error_descr, DXF_SUCCESS, *error_code,
						   (error_descr == NULL) ? L"NULL" : *error_descr);

		return DXF_SUCCESS;
	}

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_get_last_error(error_code = %p, error_descr = %p) -> %d, 'Tried unsuccessfully to "
					   L"retrieve the error code & description'",
					   error_code, error_descr, DXF_FAILURE);

	return DXF_FAILURE;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_set_order_source(dxf_subscription_t subscription, const char *source) {
	dx_logging_verbose(dx_ll_debug, L"dxf_set_order_source(sub = %p, source = '%hs')", subscription,
					   (source == NULL) ? "NULL" : source);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (subscription == dx_invalid_subscription) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_set_order_source(sub = %p, source = '%hs') -> %d, 'The subscription is invalid'",
						   subscription, (source == NULL) ? "NULL" : source, DXF_FAILURE);

		return DXF_FAILURE;
	}

	size_t source_len = source == NULL ? 0 : strlen(source);

	if (source_len == 0 || source_len >= DXF_RECORD_SUFFIX_SIZE) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_set_order_source(sub = %p, source = '%hs') -> %d, 'The source is invalid'",
						   subscription, (source == NULL) ? "NULL" : source, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dxf_connection_t connection = NULL;
	dxf_const_string_t *symbols = NULL;
	size_t symbol_count = 0;
	unsigned events = 0;
	dx_event_subscr_flag subscr_flags = dx_esf_default;
	dxf_long_t time = 0;

	// unsubscribe from old order sources
	if (!dx_get_subscription_connection(subscription, &connection) ||
		!dx_get_event_subscription_symbols(subscription, &symbols, &symbol_count) ||
		!dx_get_event_subscription_event_types(subscription, &events) ||
		!dx_get_event_subscription_flags(subscription, &subscr_flags) ||
		!dx_get_event_subscription_time(subscription, &time) ||
		!dx_unsubscribe(connection, dx_get_order_sources(subscription), symbols, symbol_count, (int)events,
						subscr_flags, time)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_set_order_source(sub = %p, source = '%hs') -> %d, 'Tried unsuccessfully to "
						   L"unsubscribe from the old order sources'",
						   subscription, source, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_perform_common_actions(DX_RESET_ERROR);

	// remove old sources
	dx_clear_order_sources(subscription);
	// add new source
	dxf_string_t str = dx_ansi_to_unicode(source);

	if (!dx_add_order_source(subscription, str)) {
		dx_free(str);
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_set_order_source(sub = %p, source = '%hs') -> %d, 'Error while adding the new order source'",
			subscription, source, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_free(str);

	// subscribe to new order sources
	if (!dx_load_events_for_subscription(connection, dx_get_order_sources(subscription), (int)events, subscr_flags) ||
		!dx_send_record_description(connection, false) ||
		!dx_subscribe(connection, dx_get_order_sources(subscription), symbols, symbol_count, NULL, 0, (int)events,
					  subscr_flags, time)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_set_order_source(sub = %p, source = '%hs') -> %d, 'Tried unsuccessfully to subscribe "
						   L"to the new order source'",
						   subscription, source, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_set_order_source(sub = %p, source = '%hs') -> %d", subscription, source,
					   DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_add_order_source(dxf_subscription_t subscription, const char *source) {
	dx_logging_verbose(dx_ll_debug, L"dxf_add_order_source(sub = %p, source = '%hs')", subscription,
					   (source == NULL) ? "NULL" : source);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (subscription == dx_invalid_subscription) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_add_order_source(sub = %p, source = '%hs') -> %d, 'The subscription is invalid'",
						   subscription, (source == NULL) ? "NULL" : source, DXF_FAILURE);

		return DXF_FAILURE;
	}

	size_t source_len = source == NULL ? 0 : strlen(source);

	if (source_len == 0 || source_len >= DXF_RECORD_SUFFIX_SIZE) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_add_order_source(sub = %p, source = '%hs') -> %d, 'The source is invalid'",
						   subscription, (source == NULL) ? "NULL" : source, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dxf_string_t wide_source = dx_ansi_to_unicode(source);
	dx_suffix_t elem = {{0}};

	dx_copy_string_len(elem.suffix, wide_source, source_len);
	dx_free(wide_source);

	dx_order_source_array_t new_source_array = {NULL, 0, 0};
	int failed = false;

	DX_ARRAY_INSERT(new_source_array, dx_suffix_t, elem, new_source_array.size, dx_capacity_manager_halfer, failed);

	if (failed) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_add_order_source(sub = %p, source = '%hs') -> %d, 'Error while inserting the order "
						   L"source into the array'",
						   subscription, source, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dxf_connection_t connection = NULL;
	dxf_const_string_t *symbols = NULL;
	size_t symbol_count = 0;
	unsigned events = 0;
	dx_event_subscr_flag subscr_flags = dx_esf_default;
	dxf_long_t time = 0;

	if (!dx_get_subscription_connection(subscription, &connection) ||
		!dx_get_event_subscription_symbols(subscription, &symbols, &symbol_count) ||
		!dx_get_event_subscription_event_types(subscription, &events) ||
		!dx_get_event_subscription_flags(subscription, &subscr_flags) ||
		!dx_get_event_subscription_time(subscription, &time) ||
		!dx_load_events_for_subscription(connection, &new_source_array, (int)events, subscr_flags) ||
		!dx_send_record_description(connection, false) ||
		!dx_subscribe(connection, &new_source_array, symbols, symbol_count, NULL, 0, (int)events, subscr_flags, time) ||
		!dx_add_order_source(subscription, elem.suffix)) {
		dx_free(new_source_array.elements);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_add_order_source(sub = %p, source = '%hs') -> %d, 'Tried unsuccessfully to subscribe "
						   L"to the order source and to add it'",
						   subscription, source, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_free(new_source_array.elements);
	dx_logging_verbose(dx_ll_debug, L"dxf_add_order_source(sub = %p, source = '%hs') -> %d", subscription, source,
					   DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

ERRORCODE dx_create_snapshot_impl(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol,
								  dxf_const_string_t source, dxf_long_t time, OUT dxf_snapshot_t *snapshot) {
	dxf_subscription_t subscription = NULL;
	dx_record_info_id_t record_info_id;
	dxf_const_string_t source_value = NULL;
	ERRORCODE error_code;
	int event_types = DX_EVENT_BIT_MASK(event_id);
	size_t source_len = (source == NULL ? 0 : dx_string_length(source));
	dx_event_subscr_flag subscr_flags = dx_esf_time_series;

	if (event_id == dx_eid_order) {
		subscr_flags |= dx_esf_single_record;
		if (source_len > 0 &&
			(dx_compare_strings(source, DXF_ORDER_AGGREGATE_BID_STR) == 0 ||
			 dx_compare_strings(source, DXF_ORDER_AGGREGATE_ASK_STR) == 0)) {
			record_info_id = dx_rid_market_maker;
			subscr_flags |= dx_esf_sr_market_maker_order;
			source_value = dx_all_special_order_sources[dxf_sos_AGGREGATE];
		} else {
			record_info_id = dx_rid_order;

			if (source_len > 0) {
				source_value = source;
			}
		}
	} else if (event_id == dx_eid_candle) {
		record_info_id = dx_rid_candle;
	} else if (event_id == dx_eid_spread_order) {
		record_info_id = dx_rid_spread_order;
	} else if (event_id == dx_eid_time_and_sale) {
		record_info_id = dx_rid_time_and_sale;
	} else if (event_id == dx_eid_greeks) {
		record_info_id = dx_rid_greeks;
	} else if (event_id == dx_eid_series) {
		record_info_id = dx_rid_series;
	} else {
		dx_set_error_code(dx_ssec_invalid_event_id);
		return DXF_FAILURE;
	}

	dx_perform_common_actions(DX_RESET_ERROR);

	if (snapshot == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		return DXF_FAILURE;
	}

	if (symbol == NULL || dx_string_length(symbol) == 0) {
		dx_set_error_code(dx_ssec_invalid_symbol);
		return DXF_FAILURE;
	}

	error_code = dxf_create_subscription_impl(connection, event_types, subscr_flags, time, &subscription);
	if (error_code == DXF_FAILURE) return error_code;

	if (record_info_id == dx_rid_order || record_info_id == dx_rid_market_maker) {
		dx_clear_order_sources(subscription);

		if (source_value != NULL) {
			dx_add_order_source(subscription, source_value);
		}
	}

	*snapshot = dx_create_snapshot(connection, subscription, event_id, record_info_id, symbol,
								   (record_info_id == dx_rid_order) ? source_value : NULL, time);
	if (*snapshot == dx_invalid_snapshot) {
		dx_close_subscription(subscription, DX_KEEP_ERROR);
		return DXF_FAILURE;
	}

	error_code = dxf_add_symbol(subscription, symbol);
	if (error_code == DXF_FAILURE) {
		dx_close_snapshot(*snapshot);
		dxf_close_subscription(subscription);
		*snapshot = dx_invalid_snapshot;
		return error_code;
	}

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_snapshot(dxf_connection_t connection, dx_event_id_t event_id, dxf_const_string_t symbol,
										 const char *source, dxf_long_t time, OUT dxf_snapshot_t *snapshot) {
	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_snapshot(con = %p, event id = 0x%X, symbol = '%ls', source = '%hs', time = %" LS(
						   PRId64) L", snapshot = %p)",
					   connection, event_id, (symbol == NULL) ? L"NULL" : symbol, (source == NULL) ? "NULL" : source,
					   time, snapshot);

	dxf_string_t source_str = (source != NULL) ? dx_ansi_to_unicode(source) : NULL;

	ERRORCODE res = dx_create_snapshot_impl(connection, event_id, symbol, source_str, time, snapshot);

	dx_free(source_str);

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_snapshot(con = %p, event id = 0x%X, symbol = '%ls', source = '%hs', time = %" LS(
						   PRId64) L", snapshot = %p) -> %d, %p",
					   connection, event_id, (symbol == NULL) ? L"NULL" : symbol, (source == NULL) ? "NULL" : source,
					   time, snapshot, res, (snapshot == NULL) ? NULL : *snapshot);

	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_order_snapshot(dxf_connection_t connection, dxf_const_string_t symbol,
											   const char *source, dxf_long_t time, OUT dxf_snapshot_t *snapshot) {
	dx_logging_verbose(
		dx_ll_debug,
		L"dxf_create_order_snapshot(con = %p, symbol = '%ls', source = '%hs', time = %" LS(PRId64) L", snapshot = %p)",
		connection, (symbol == NULL) ? L"NULL" : symbol, (source == NULL) ? "NULL" : source, time, snapshot);

	ERRORCODE res = dxf_create_snapshot(connection, dx_eid_order, symbol, source, time, snapshot);

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_order_snapshot(con = %p, symbol = '%ls', source = '%hs', time = %" LS(
						   PRId64) L", snapshot = %p) -> %d, %p",
					   connection, (symbol == NULL) ? L"NULL" : symbol, (source == NULL) ? "NULL" : source, time,
					   snapshot, res, (snapshot == NULL) ? NULL : *snapshot);

	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_candle_snapshot(dxf_connection_t connection, dxf_candle_attributes_t candle_attributes,
												dxf_long_t time, OUT dxf_snapshot_t *snapshot) {
	dx_logging_verbose(
		dx_ll_debug, L"dxf_create_candle_snapshot(con = %p, candle attr = %p, time = %" LS(PRId64) L", snapshot = %p)",
		connection, candle_attributes, time, snapshot);

	ERRORCODE res;
	dxf_string_t candle_symbol = NULL;

	if (!dx_candle_symbol_to_string(candle_attributes, &candle_symbol)) {
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_create_candle_snapshot(con = %p, candle attr = %p, time = %" LS(
				PRId64) L", snapshot = %p) -> %d, 'Error while converting the candle attributes to a candle symbol'",
			connection, candle_attributes, time, snapshot, DXF_FAILURE);

		return DXF_FAILURE;
	}

	res = dx_create_snapshot_impl(connection, dx_eid_candle, candle_symbol, NULL, time, snapshot);

	dx_free(candle_symbol);

	dx_logging_verbose(
		dx_ll_debug,
		L"dxf_create_candle_snapshot(con = %p, candle attr = %p, time = %" LS(PRId64) L", snapshot = %p) -> %d, %p",
		connection, candle_attributes, time, snapshot, res, (snapshot == NULL) ? NULL : *snapshot);

	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_close_snapshot(dxf_snapshot_t snapshot) {
	dx_logging_verbose(dx_ll_debug, L"dxf_close_snapshot(snapshot = %p)", snapshot);

	dxf_subscription_t subscription = NULL;

	dx_perform_common_actions(DX_RESET_ERROR);

	if (snapshot == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug, L"dxf_close_snapshot(snapshot = %p) -> %d, 'Invalid snapshot'", snapshot,
						   DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_get_snapshot_subscription(snapshot, &subscription) || !dxf_close_subscription(subscription) ||
		!dx_close_snapshot(snapshot)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_close_snapshot(snapshot = %p) -> %d, 'Tried unsuccessfully to close the snapshot'",
						   snapshot, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_close_snapshot(snapshot = %p) -> %d", snapshot, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_attach_snapshot_listener(dxf_snapshot_t snapshot, dxf_snapshot_listener_t snapshot_listener,
												  void *user_data) {
	dx_logging_verbose(dx_ll_debug, L"dxf_attach_snapshot_listener(snapshot = %p, listener = %p, user data = %p)",
					   snapshot, snapshot_listener, user_data);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (snapshot == dx_invalid_snapshot || snapshot_listener == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_snapshot_listener(snapshot = %p, listener = %p, user data = %p) -> %d, "
						   L"'Invalid snapshot or the listener is NULL'",
						   snapshot, snapshot_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_add_snapshot_listener(snapshot, snapshot_listener, user_data)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_snapshot_listener(snapshot = %p, listener = %p, user data = %p) -> %d, 'Tried "
						   L"unsuccessfully to add the snapshot listener'",
						   snapshot, snapshot_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_attach_snapshot_listener(snapshot = %p, listener = %p, user data = %p) -> %d",
					   snapshot, snapshot_listener, user_data, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_detach_snapshot_listener(dxf_snapshot_t snapshot, dxf_snapshot_listener_t snapshot_listener) {
	dx_logging_verbose(dx_ll_debug, L"dxf_detach_snapshot_listener(snapshot = %p, listener = %p)", snapshot,
					   snapshot_listener);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (snapshot == dx_invalid_subscription || snapshot_listener == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_snapshot_listener(snapshot = %p, listener = %p) -> %d, 'Invalid snapshot or "
						   L"the listener is NULL'",
						   snapshot, snapshot_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_remove_snapshot_listener(snapshot, snapshot_listener)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_snapshot_listener(snapshot = %p, listener = %p) -> %d, 'Tried unsuccessfully "
						   L"to remove the snapshot listener'",
						   snapshot, snapshot_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_detach_snapshot_listener(snapshot = %p, listener = %p) -> %d", snapshot,
					   snapshot_listener, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_attach_snapshot_inc_listener(dxf_snapshot_t snapshot,
													  dxf_snapshot_inc_listener_t snapshot_listener, void *user_data) {
	dx_logging_verbose(dx_ll_debug, L"dxf_attach_snapshot_inc_listener(snapshot = %p, listener = %p, user data = %p)",
					   snapshot, snapshot_listener, user_data);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (snapshot == dx_invalid_snapshot || snapshot_listener == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_snapshot_inc_listener(snapshot = %p, listener = %p, user data = %p) -> %d, "
						   L"'Invalid snapshot or the listener is NULL'",
						   snapshot, snapshot_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_add_snapshot_inc_listener(snapshot, snapshot_listener, user_data)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_snapshot_inc_listener(snapshot = %p, listener = %p, user data = %p) -> %d, "
						   L"'Tried unsuccessfully to add the snapshot listener'",
						   snapshot, snapshot_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_attach_snapshot_inc_listener(snapshot = %p, listener = %p, user data = %p) -> %d",
					   snapshot, snapshot_listener, user_data, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_detach_snapshot_inc_listener(dxf_snapshot_t snapshot,
													  dxf_snapshot_inc_listener_t snapshot_listener) {
	dx_logging_verbose(dx_ll_debug, L"dxf_detach_snapshot_inc_listener(snapshot = %p, listener = %p)", snapshot,
					   snapshot_listener);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (snapshot == dx_invalid_subscription || snapshot_listener == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_snapshot_inc_listener(snapshot = %p, listener = %p) -> %d, 'Invalid snapshot "
						   L"or the listener is NULL'",
						   snapshot, snapshot_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_remove_snapshot_inc_listener(snapshot, snapshot_listener)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_snapshot_inc_listener(snapshot = %p, listener = %p) -> %d, 'Tried "
						   L"unsuccessfully to remove the snapshot listener'",
						   snapshot, snapshot_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_detach_snapshot_inc_listener(snapshot = %p, listener = %p) -> %d", snapshot,
					   snapshot_listener, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_get_snapshot_symbol(dxf_snapshot_t snapshot, OUT dxf_string_t *symbol) {
	dx_logging_verbose(dx_ll_debug, L"dxf_get_snapshot_symbol(snapshot = %p, symbol ptr = %p)", snapshot, symbol);

	dx_perform_common_actions(DX_RESET_ERROR);

	if (snapshot == dx_invalid_subscription || symbol == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_get_snapshot_symbol(snapshot = %p, symbol ptr = %p) -> %d, 'Invalid snapshot or symbol ptr'",
			snapshot, symbol, DXF_FAILURE);

		return DXF_FAILURE;
	}

	*symbol = dx_get_snapshot_symbol(snapshot);

	dx_logging_verbose(dx_ll_debug, L"dxf_get_snapshot_symbol(snapshot = %p, symbol ptr = %p) -> %d, '%ls'", snapshot,
					   symbol, DXF_SUCCESS, (*symbol == NULL) ? L"NULL" : *symbol);

	return DXF_SUCCESS;
}

/**
 * Returns number of sources
 *
 * @param sources The order sources
 * @return the number of sources
 */
static inline int dx_get_sources_count(const char **sources) {
	if (sources == NULL) {
		return 0;
	}

	int i = 0;

	while (sources[i]) i++;

	return i;
}

ERRORCODE dx_create_price_level_book_impl(dxf_connection_t connection, dxf_const_string_t symbol, const char **sources,
										  int sources_count_unchecked, OUT dxf_price_level_book_t *book) {
	dx_perform_common_actions(DX_RESET_ERROR);
	if (!dx_init_codec()) {
		return DXF_FAILURE;
	}

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		return DXF_FAILURE;
	}

	if (symbol == NULL || dx_string_length(symbol) == 0) {
		dx_set_error_code(dx_ssec_invalid_symbol);
		return DXF_FAILURE;
	}

	size_t sources_count = 0;
	dxf_ulong_t sources_flags = 0;

	/* Check sources */
	if (sources == NULL || sources_count_unchecked == 0) {
		sources_count = dx_all_order_sources_count;
		sources_flags = ~0ULL;
	} else {
		for (int i = 0; i < sources_count_unchecked; i++) {
			if (!sources[i] || strlen(sources[i]) < 1 || strlen(sources[i]) > 4) {
				dx_set_error_code(dx_ec_invalid_func_param);

				return DXF_FAILURE;
			}

			dxf_string_t source_wide = dx_ansi_to_unicode(sources[i]);
			int found = false;

			for (int know_sources_index = 0; !found && dx_all_order_sources[know_sources_index] != NULL;
				 know_sources_index++) {
				if (!dx_compare_strings(source_wide, dx_all_order_sources[know_sources_index])) {
					found = true;
					if ((sources_flags & (1ULL << know_sources_index)) == 0) sources_count++;
					sources_flags |= (1ULL << know_sources_index);
				}
			}

			dx_free(source_wide);

			if (!found) {
				dx_set_error_code(dx_ec_invalid_func_param);

				return DXF_FAILURE;
			}
		}
	}

	if (sources_count == 0) {
		dx_set_error_code(dx_ec_invalid_func_param);
		return DXF_FAILURE;
	}

	*book = dx_create_price_level_book(connection, symbol, sources_count, sources_flags);

	if (*book == NULL) {
		return DXF_FAILURE;
	}

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_create_price_level_book(dxf_connection_t connection, dxf_const_string_t symbol,
												 const char **sources, OUT dxf_price_level_book_t *book) {
	int sources_count = dx_get_sources_count(sources);

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_price_level_book(con = %p, symbol = '%ls', sources[%d] = '%hs'..., book = %p)",
					   connection, (symbol == NULL) ? L"NULL" : symbol, sources_count,
					   (sources == NULL) ? "NULL" : (sources_count > 0 ? sources[0] : ""), book);

	ERRORCODE res = dx_create_price_level_book_impl(connection, symbol, sources, sources_count, book);

	dx_logging_verbose(
		dx_ll_debug,
		L"dxf_create_price_level_book(con = %p, symbol = '%ls', sources[%d] = '%hs'..., book = %p) -> %d, %p",
		connection, (symbol == NULL) ? L"NULL" : symbol, sources_count,
		(sources == NULL) ? "NULL" : (sources_count > 0 ? sources[0] : ""), book, res, (book == NULL) ? NULL : *book);

	return res;
}

DXFEED_API ERRORCODE dxf_create_price_level_book_v2(dxf_connection_t connection, dxf_const_string_t symbol,
													const char **sources, int sources_count,
													OUT dxf_price_level_book_t *book) {
	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_price_level_book_v2(con = %p, symbol = '%ls', sources[%d] = '%hs'..., book = %p)",
					   connection, (symbol == NULL) ? L"NULL" : symbol, sources_count,
					   (sources == NULL) ? "NULL" : (sources_count > 0 ? sources[0] : ""), book);

	ERRORCODE res = dx_create_price_level_book_impl(connection, symbol, sources, sources_count, book);

	dx_logging_verbose(
		dx_ll_debug,
		L"dxf_create_price_level_book_v2(con = %p, symbol = '%ls', sources[%d] = '%hs'..., book = %p) -> %d, %p",
		connection, (symbol == NULL) ? L"NULL" : symbol, sources_count,
		(sources == NULL) ? "NULL" : (sources_count > 0 ? sources[0] : ""), book, res, (book == NULL) ? NULL : *book);

	return res;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_close_price_level_book(dxf_price_level_book_t book) {
	dx_logging_verbose(dx_ll_debug, L"dxf_close_price_level_book(book = %p)", book);

	dx_perform_common_actions(DX_RESET_ERROR);

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug, L"dxf_close_price_level_book(book = %p) -> %d, 'Invalid PLB'", book,
						   DXF_FAILURE);

		return DXF_FAILURE;
	}
	if (!dx_close_price_level_book(book)) {
		dx_logging_verbose(dx_ll_debug, L"dxf_close_price_level_book(book = %p) -> %d, 'Error while closing the PLB'",
						   book, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_close_price_level_book(book = %p) -> %d", book, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_attach_price_level_book_listener(dxf_price_level_book_t book,
														  dxf_price_level_book_listener_t book_listener,
														  void *user_data) {
	dx_logging_verbose(dx_ll_debug, L"dxf_attach_price_level_book_listener(book = %p, listener = %p, user data = %p)",
					   book, book_listener, user_data);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_attach_price_level_book_listener(book = %p, listener = %p, user data = %p) -> %d, 'Invalid PLB'",
			book, book_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_add_price_level_book_listener(book, book_listener, user_data)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_price_level_book_listener(book = %p, listener = %p, user data = %p) -> %d, "
						   L"'Error while adding the PLB listener'",
						   book, book_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_attach_price_level_book_listener(book = %p, listener = %p, user data = %p) -> %d", book,
					   book_listener, user_data, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_detach_price_level_book_listener(dxf_price_level_book_t book,
														  dxf_price_level_book_listener_t book_listener) {
	dx_logging_verbose(dx_ll_debug, L"dxf_detach_price_level_book_listener(book = %p, listener = %p)", book,
					   book_listener);

	dx_perform_common_actions(DX_RESET_ERROR);

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_price_level_book_listener(book = %p, listener = %p) -> %d, 'Invalid PLB'", book,
						   book_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_remove_price_level_book_listener(book, book_listener)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_price_level_book_listener(book = %p, listener = %p) -> %d, 'Error while "
						   L"removing the PLB listener'",
						   book, book_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_detach_price_level_book_listener(book = %p, listener = %p) -> %d", book,
					   book_listener, DXF_SUCCESS);

	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_create_price_level_book_v3(dxf_connection_t connection, dxf_const_string_t symbol,
													const char *source, int levels_number,
													OUT dxf_price_level_book_v2_t *book) {
	dx_logging_verbose(
		dx_ll_debug,
		L"dxf_create_price_level_book_v3(con = %p, symbol = '%ls', source = '%hs', levels_number = %d, book = %p)",
		connection, (symbol == NULL) ? L"NULL" : symbol, (source == NULL) ? "NULL" : source, levels_number, book);

	dx_perform_common_actions(DX_RESET_ERROR);
	if (!dx_init_codec()) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_price_level_book_v3(con = %p, symbol = '%ls', source = '%hs', levels_number = "
						   L"%d, book = %p) -> %d, 'Error while initializing the symbol codec.'",
						   connection, (symbol == NULL) ? L"NULL" : symbol, (source == NULL) ? "NULL" : source,
						   levels_number, book, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (book == NULL) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_price_level_book_v3(con = %p, symbol = '%ls', source = '%hs', levels_number = "
						   L"%d, book = %p) -> %d, 'Invalid PLB pointer'",
						   connection, (symbol == NULL) ? L"NULL" : symbol, (source == NULL) ? "NULL" : source,
						   levels_number, book, DXF_FAILURE);

		dx_set_error_code(dx_plbec_invalid_book_ptr);
		return DXF_FAILURE;
	}

	if (symbol == NULL || dx_string_length(symbol) == 0) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_price_level_book_v3(con = %p, symbol = '%ls', source = '%hs', levels_number = "
						   L"%d, book = %p) -> %d, 'Invalid PLB symbol'",
						   connection, (symbol == NULL) ? L"NULL" : symbol, (source == NULL) ? "NULL" : source,
						   levels_number, book, DXF_FAILURE);

		dx_set_error_code(dx_plbec_invalid_symbol);
		return DXF_FAILURE;
	}

	if (source == NULL || strlen(source) == 0) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_price_level_book_v3(con = %p, symbol = '%ls', source = '%hs', levels_number = "
						   L"%d, book = %p) -> %d, 'Invalid PLB source'",
						   connection, symbol, (source == NULL) ? "NULL" : source, levels_number, book, DXF_FAILURE);

		dx_set_error_code(dx_plbec_invalid_source);
		return DXF_FAILURE;
	}

	*book = dx_create_price_level_book_v2(connection, symbol, source, levels_number);

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_create_price_level_book_v3(con = %p, symbol = '%ls', source = '%hs', levels_number = %d, "
					   L"book = %p) -> %d, %p",
					   connection, symbol, source, levels_number, book, DXF_SUCCESS, *book);

	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_close_price_level_book_v2(dxf_price_level_book_v2_t book) {
	dx_logging_verbose(dx_ll_debug, L"dxf_close_price_level_book_v2(book = %p)", book);

	dx_perform_common_actions(DX_RESET_ERROR);

	if (book == NULL) {
		dx_set_error_code(dx_plbec_invalid_book_handle);
		dx_logging_verbose(dx_ll_debug, L"dxf_close_price_level_book_v2(book = %p) -> %d, 'Invalid PLB'", book,
						   DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_close_price_level_book_v2(book)) {
		dx_logging_verbose(dx_ll_debug, L"dxf_close_price_level_book_v2(book = %p) -> %d", book, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_close_price_level_book_v2(book = %p) -> %d, 'Invalid PLB'", book,
					   DXF_SUCCESS);

	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_set_price_level_book_listeners_v2(
	dxf_price_level_book_v2_t book, dxf_price_level_book_listener_t on_new_book_listener,
	dxf_price_level_book_listener_t on_book_update_listener,
	dxf_price_level_book_inc_listener_t on_incremental_change_listener, void *user_data) {
	dx_logging_verbose(dx_ll_debug,
					   L"dxf_set_price_level_book_listeners_v2(book = %p, new = %p, update = %p, inc = %p)", book,
					   on_new_book_listener, on_book_update_listener, on_incremental_change_listener);

	dx_perform_common_actions(DX_RESET_ERROR);

	ERRORCODE res = dx_set_price_level_book_listeners_v2(book, on_new_book_listener, on_book_update_listener,
														 on_incremental_change_listener, user_data);

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_set_price_level_book_listeners_v2(book = %p, new = %p, update = %p, inc = %p) -> %d", book,
					   on_new_book_listener, on_book_update_listener, on_incremental_change_listener, res);

	return res;
}

DXFEED_API ERRORCODE dxf_create_regional_book(dxf_connection_t connection, dxf_const_string_t symbol,
											  OUT dxf_regional_book_t *book) {
	dx_logging_verbose(dx_ll_debug, L"dxf_create_regional_book(con = %p, symbol = '%ls', book ptr = %p)", connection,
					   (symbol == NULL) ? L"NULL" : symbol, book);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (!dx_init_codec()) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_regional_book(con = %p, symbol = '%ls', book ptr = %p) -> %d, 'Error while "
						   L"initializing a symbol codec'",
						   connection, (symbol == NULL) ? L"NULL" : symbol, book, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_create_regional_book(con = %p, symbol = '%ls', book ptr = %p) -> %d, 'Invalid regional book ptr'",
			connection, (symbol == NULL) ? L"NULL" : symbol, book, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (symbol == NULL || dx_string_length(symbol) == 0) {
		dx_set_error_code(dx_ssec_invalid_symbol);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_regional_book(con = %p, symbol = '%ls', book ptr = %p) -> %d, 'Invalid symbol'",
						   connection, (symbol == NULL) ? L"NULL" : symbol, book, DXF_FAILURE);

		return DXF_FAILURE;
	}

	*book = dx_create_regional_book(connection, symbol);

	if (*book == NULL) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_create_regional_book(con = %p, symbol = '%ls', book ptr = %p) -> %d, 'Tried "
						   L"unsuccessfully to create regional book'",
						   connection, symbol, book, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_create_regional_book(con = %p, symbol = '%ls', book ptr = %p) -> %d, %p",
					   connection, symbol, book, DXF_SUCCESS, *book);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_close_regional_book(dxf_regional_book_t book) {
	dx_logging_verbose(dx_ll_debug, L"dxf_close_regional_book(book = %p)", book);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug, L"dxf_close_regional_book(book = %p) -> %d, 'Invalid regional book'", book,
						   DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_close_regional_book(book)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_close_regional_book(book = %p) -> %d, 'Tried unsuccessfully to close regional book'",
						   book, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_close_regional_book(book = %p) -> %d", book, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_attach_regional_book_listener(dxf_regional_book_t book,
													   dxf_price_level_book_listener_t book_listener, void *user_data) {
	dx_logging_verbose(dx_ll_debug, L"dxf_attach_regional_book_listener(book = %p, listener = %p, user data = %p)",
					   book, book_listener, user_data);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_regional_book_listener(book = %p, listener = %p, user data = %p) -> %d, "
						   L"'Invalid regional book'",
						   book, book_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_add_regional_book_listener(book, book_listener, user_data)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_regional_book_listener(book = %p, listener = %p, user data = %p) -> %d, 'Error "
						   L"while adding the regional book listener'",
						   book, book_listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_attach_regional_book_listener(book = %p, listener = %p, user data = %p) -> %d", book,
					   book_listener, user_data, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_detach_regional_book_listener(dxf_regional_book_t book,
													   dxf_price_level_book_listener_t book_listener) {
	dx_logging_verbose(dx_ll_debug, L"dxf_detach_regional_book_listener(book = %p, listener = %p)", book,
					   book_listener);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(
			dx_ll_debug, L"dxf_detach_regional_book_listener(book = %p, listener = %p) -> %d, 'Invalid regional book'",
			book, book_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_remove_regional_book_listener(book, book_listener)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_regional_book_listener(book = %p, listener = %p) -> %d, 'Error while removing "
						   L"the regional book listener'",
						   book, book_listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_attach_regional_book_listener(book = %p, listener = %p) -> %d", book,
					   book_listener, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_attach_regional_book_listener_v2(dxf_regional_book_t book,
														  dxf_regional_quote_listener_t listener, void *user_data) {
	dx_logging_verbose(dx_ll_debug, L"dxf_attach_regional_book_listener_v2(book = %p, listener = %p, user data = %p)",
					   book, listener, user_data);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_regional_book_listener_v2(book = %p, listener = %p, user data = %p) -> %d, "
						   L"'Invalid regional book'",
						   book, listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_add_regional_book_listener_v2(book, listener, user_data)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_attach_regional_book_listener_v2(book = %p, listener = %p, user data = %p) -> %d, "
						   L"'Error while adding the regional book listener'",
						   book, listener, user_data, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug,
					   L"dxf_attach_regional_book_listener_v2(book = %p, listener = %p, user data = %p) -> %d", book,
					   listener, user_data, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_detach_regional_book_listener_v2(dxf_regional_book_t book,
														  dxf_regional_quote_listener_t listener) {
	dx_logging_verbose(dx_ll_debug, L"dxf_detach_regional_book_listener_v2(book = %p, listener = %p)", book, listener);
	dx_perform_common_actions(DX_RESET_ERROR);

	if (book == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);
		dx_logging_verbose(
			dx_ll_debug,
			L"dxf_detach_regional_book_listener_v2(book = %p, listener = %p) -> %d, 'Invalid regional book'", book,
			listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	if (!dx_remove_regional_book_listener_v2(book, listener)) {
		dx_logging_verbose(dx_ll_debug,
						   L"dxf_detach_regional_book_listener_v2(book = %p, listener = %p) -> %d, 'Error while "
						   L"removing the regional book listener'",
						   book, listener, DXF_FAILURE);

		return DXF_FAILURE;
	}

	dx_logging_verbose(dx_ll_debug, L"dxf_detach_regional_book_listener_v2(book = %p, listener = %p) -> %d", book,
					   listener, DXF_SUCCESS);

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_write_raw_data(dxf_connection_t connection, const char *raw_file_name) {
	if (!dx_add_raw_dump_file(connection, raw_file_name)) {
		return DXF_FAILURE;
	}

	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_get_connection_properties_snapshot(dxf_connection_t connection,
															OUT dxf_property_item_t **properties, OUT int *count) {
	if (!dx_protocol_property_get_snapshot(connection, properties, count)) {
		return DXF_FAILURE;
	}
	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_free_connection_properties_snapshot(dxf_property_item_t *properties, int count) {
	if (properties != NULL) {
		for (int i = 0; i < count; ++i) {
			if (properties[i].key != NULL) {
				dx_free(properties[i].key);
			}
			if (properties[i].value != NULL) {
				dx_free(properties[i].value);
			}
		}
		dx_free(properties);
	}
	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_get_current_connected_address(dxf_connection_t connection, OUT char **address) {
	if (!dx_get_current_connected_address(connection, address)) {
		return DXF_FAILURE;
	}
	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_get_current_connection_status(dxf_connection_t connection,
													   OUT dxf_connection_status_t *status) {
	if (status == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);

		return DXF_FAILURE;
	}

	*status = dx_connection_status_get(connection);

	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_free(void *pointer) {
	dx_free(pointer);
	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_load_config_from_wstring(dxf_const_string_t config) {
	if (config == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);

		return DXF_FAILURE;
	}

	if (!dx_load_config_from_wstring(config)) {
		return DXF_FAILURE;
	}

	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_load_config_from_string(const char *config) {
	if (config == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);

		return DXF_FAILURE;
	}

	if (!dx_load_config_from_string(config)) {
		return DXF_FAILURE;
	}

	return DXF_SUCCESS;
}

DXFEED_API ERRORCODE dxf_load_config_from_file(const char *file_name) {
	if (file_name == NULL) {
		dx_set_error_code(dx_ec_invalid_func_param);

		return DXF_FAILURE;
	}

	if (!dx_load_config_from_file(file_name)) {
		return DXF_FAILURE;
	}

	return DXF_SUCCESS;
}

DXFEED_API dxf_const_string_t dxf_get_order_action_wstring_name(dxf_order_action_t action) {
	switch (action) {
		case dxf_oa_undefined:
			return L"Undefined";
		case dxf_oa_new:
			return L"New";
		case dxf_oa_replace:
			return L"Replace";
		case dxf_oa_modify:
			return L"Modify";
		case dxf_oa_delete:
			return L"Delete";
		case dxf_oa_partial:
			return L"Partial";
		case dxf_oa_execute:
			return L"Execute";
		case dxf_oa_trade:
			return L"Trade";
		case dxf_oa_bust:
			return L"Bust";
		default:
			return L"Unknown";
	}
}

DXFEED_API const char *dxf_get_order_action_string_name(dxf_order_action_t action) {
	switch (action) {
		case dxf_oa_undefined:
			return "Undefined";
		case dxf_oa_new:
			return "New";
		case dxf_oa_replace:
			return "Replace";
		case dxf_oa_modify:
			return "Modify";
		case dxf_oa_delete:
			return "Delete";
		case dxf_oa_partial:
			return "Partial";
		case dxf_oa_execute:
			return "Execute";
		case dxf_oa_trade:
			return "Trade";
		case dxf_oa_bust:
			return "Bust";
		default:
			return "Unknown";
	}
}
