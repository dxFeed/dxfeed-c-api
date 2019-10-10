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
#include <Windows.h>
#ifdef _DEBUG
#include <DbgHelp.h>
#endif
#else
#include <pthread.h>
#include <time.h>
#endif

#include <stdio.h>
#include <stdarg.h>

#include "Logger.h"
#include "DXErrorHandling.h"
#include "BufferedIOCommon.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "DXThreads.h"
#include "DXErrorCodes.h"
#include "Version.h"

/* -------------------------------------------------------------------------- */
/*
 *	Implementation Details
 */
/* -------------------------------------------------------------------------- */

#define CURRENT_TIME_STR_LENGTH 31
#define CURRENT_TIME_STR_TIME_OFFSET 24
#define TIME_ZONE_BIAS_TO_HOURS -60

static dxf_const_string_t g_error_prefix = L"Error: ";
static dxf_const_string_t g_info_prefix = L"";
static dxf_const_string_t g_default_time_string = L"Incorrect time";

static bool g_verbose_logger_mode;
static bool g_show_timezone;
static FILE* g_log_file = NULL;
#ifdef _DEBUG
static FILE* g_dbg_file = NULL;
static dx_mutex_t g_dbg_lock;
#endif

static dx_key_t g_current_time_str_key;
static bool g_key_creation_attempted = false;
static bool g_key_created = false;

static dxf_char_t current_time_str[CURRENT_TIME_STR_LENGTH + 1];

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions
 */
/* -------------------------------------------------------------------------- */

dxf_string_t dx_get_current_time_buffer (void);

#ifdef _WIN32

#ifdef _DEBUG
void dx_log_debug_message(const dxf_char_t *format, ...) {
	dxf_char_t message[256];
	va_list ap;
	swprintf(message, 255, L"[%08lx] ", (unsigned long)GetCurrentThreadId());
	va_start(ap, format);
	vswprintf(&message[11], 244, format, ap);
	OutputDebugStringW(message);
	va_end(ap);
}

void dx_vlog_debug_message(const dxf_char_t *format, va_list ap) {
	dxf_char_t message[256];
	swprintf(message, 255, L"[%08lx] ", (unsigned long)GetCurrentThreadId());
	vswprintf(&message[11], 244, format, ap);
	OutputDebugStringW(message);
}
#else
#define dx_vlog_debug_message(f, ap)
#endif

const dxf_char_t* dx_get_current_time (void) {
	SYSTEMTIME current_time;
	dxf_char_t* time_buffer = dx_get_current_time_buffer();

	if (time_buffer == NULL) {
		return NULL;
	}

	GetLocalTime(&current_time);
	_snwprintf(time_buffer, CURRENT_TIME_STR_LENGTH, L"%.2u.%.2u.%.4u %.2u:%.2u:%.2u.%.4u",
													current_time.wDay, current_time.wMonth, current_time.wYear,
													current_time.wHour, current_time.wMinute, current_time.wSecond,
													current_time.wMilliseconds);
	if (g_show_timezone) {
		TIME_ZONE_INFORMATION time_zone;
		GetTimeZoneInformation(&time_zone);
		_snwprintf(time_buffer + CURRENT_TIME_STR_TIME_OFFSET, 7, L" GMT%+.2d", time_zone.Bias / TIME_ZONE_BIAS_TO_HOURS);
	}

	return time_buffer;
}

#else // _WIN32

dxf_const_string_t dx_get_current_time () {
	dxf_char_t* time_buffer = dx_get_current_time_buffer();
	time_t clock;
	struct tm ltm;

	if (time_buffer == NULL) {
		return NULL;
	}

	time(&clock);
	localtime_r(&clock, &ltm);
	swprintf(time_buffer, CURRENT_TIME_STR_LENGTH, L"%.2u.%.2u.%.4u %.2u:%.2u:%.2u.%.4u",
													ltm.tm_mday, ltm.tm_mon + 1, ltm.tm_year + 1900,
													ltm.tm_hour, ltm.tm_min, ltm.tm_sec,
													0);
	if (g_show_timezone) {
		swprintf(time_buffer + CURRENT_TIME_STR_TIME_OFFSET, 7, L" GMT%+.2d", ltm.tm_gmtoff / 3600);
	}

	return time_buffer;
}

#ifdef __linux
#define _GNU_SOURCE
#include <unistd.h>
#include <sys/syscall.h>
#define pthread_getthreadid_np() syscall(__NR_gettid)
#else
#define pthread_getthreadid_np() (0)
#endif

#ifdef _DEBUG
void dx_log_debug_message(const dxf_char_t *format, ...) {
	dxf_char_t message[256];
	va_list ap;
	swprintf(message, 255, L"[%08lx] ", (unsigned long)pthread_getthreadid_np());
	va_start(ap, format);
	vswprintf(&message[11], 244, format, ap);
	printf("%s", message);
	va_end(ap);
}

void dx_vlog_debug_message(const dxf_char_t *format, va_list ap) {
	dxf_char_t message[256];
	swprintf(message, 255, L"[%08lx] ", (unsigned long)pthread_getthreadid_np());
	vswprintf(&message[11], 244, format, ap);
	printf("%s", message);
}
#else
#define dx_vlog_debug_message(f, ap)
#endif

#endif // _WIN32

/* -------------------------------------------------------------------------- */

static void dx_current_time_buffer_destructor (void* data) {
	if (data != current_time_str && data != NULL) {
		dx_free(data);
	}
}

static void dx_key_remover(void *data) {
	if (g_key_created) {
		dx_thread_data_key_destroy(g_current_time_str_key);
		g_key_created = g_key_creation_attempted = false;
	}
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_get_current_time_buffer (void) {
	/*
	*	This function uses the low-level functions without embedded error handling/logging,
	*  because otherwise an infinite recursion and a stack overflow may occur
	*/

	dxf_string_t buffer = NULL;

	if (!g_key_created) {
		return NULL;
	}

	buffer = (dxf_string_t)dx_get_thread_data(g_current_time_str_key);

	if (buffer == NULL) {
		/* buffer for this thread wasn't yet created. */
		buffer = (dxf_string_t)dx_calloc_no_ehm(CURRENT_TIME_STR_LENGTH + 1, sizeof(dxf_char_t));

		if (buffer == NULL) {
			return NULL;
		}

		if (!dx_set_thread_data_no_ehm(g_current_time_str_key, buffer)) {
			dx_free_no_ehm(buffer);

			return NULL;
		}
	}

	return buffer;
}

/* -------------------------------------------------------------------------- */

bool dx_init_current_time_key (void) {
	if (g_key_creation_attempted) {
		return g_key_created;
	}

	g_key_creation_attempted = true;

	if (!dx_thread_data_key_create(&g_current_time_str_key, dx_current_time_buffer_destructor)) {
		return false;
	}

	if (!dx_set_thread_data(g_current_time_str_key, current_time_str)) {
		dx_thread_data_key_destroy(g_current_time_str_key);

		return false;
	}
	dx_register_process_destructor(&dx_key_remover, NULL);

	g_key_created = true;

	return true;
}

/* -------------------------------------------------------------------------- */

static void dx_flush_log (void) {
	if (g_log_file == NULL) {
		return;
	}

	fflush(g_log_file);
}

static void dx_close_logging(void *arg) {
	if (g_log_file != NULL) {
		fclose(g_log_file);
		g_log_file = NULL;
	}
}


/* -------------------------------------------------------------------------- */
/*
 *	External interface
 */
/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_initialize_logger (const char* file_name, int rewrite_file, int show_timezone_info, int verbose) {
	if (!dx_init_error_subsystem()) {
		wprintf(L"\nCan not init error subsystem\n");
		return DXF_FAILURE;
	}

	g_log_file = fopen(file_name, rewrite_file ? "w" : "a");

	if (g_log_file == NULL) {
		printf("\nCan not open log-file %s", file_name);
		return DXF_FAILURE;
	}

	dx_register_process_destructor(&dx_close_logging, NULL);

	g_show_timezone = show_timezone_info ? true : false;
	g_verbose_logger_mode = verbose ? true : false;

	if (!dx_init_current_time_key()) {
		return DXF_FAILURE;
	}

	dx_logging_info(L"Logging started: file %ls, verbose mode is %ls",
					rewrite_file ? L"rewritten" : L"not rewritten", verbose ? L"on" : L"off");
	dx_logging_info(L"Version: %ls, options: %ls", DX_VER_PRODUCT_VERSION_LSTR, DX_LIBRARY_OPTIONS);
	dx_flush_log();

#ifdef _DEBUG
	dx_mutex_create(&g_dbg_lock);
#endif

	return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

void dx_logging_error (dxf_const_string_t message ) {
	if (g_log_file == NULL || message == NULL) {
		return;
	}

	dx_log_debug_message(L"%ls", message);
	fwprintf(g_log_file, L"\n%ls [%08lx] %ls%ls", dx_get_current_time(),
#ifdef _WIN32
	(unsigned long)GetCurrentThreadId(),
#else
	(unsigned long)pthread_getthreadid_np(),
#endif
	g_error_prefix, message);
	dx_flush_log();
}

void dx_logging_error_by_code(int error_code) {
	if (g_log_file == NULL) {
		return;
	}
	dxf_const_string_t message = dx_get_error_description(error_code);
	if (message == NULL) {
		return;
	}
	dx_log_debug_message(L"%ls (%d)", message, error_code);
	fwprintf(g_log_file, L"\n%ls [%08lx] %ls%ls (%d)", dx_get_current_time(),
#ifdef _WIN32
	(unsigned long)GetCurrentThreadId(),
#else
		(unsigned long)pthread_getthreadid_np(),
#endif
		g_error_prefix, message, error_code);
	dx_flush_log();
}

/* -------------------------------------------------------------------------- */

void dx_logging_last_error (void) {
	dx_logging_error_by_code(dx_get_error_code());
}

/* -------------------------------------------------------------------------- */

void dx_logging_last_error_verbose (void) {
	if (!g_verbose_logger_mode) {
		return;
	}

	dx_logging_error_by_code(dx_get_error_code());
	dx_flush_log();
}

/* -------------------------------------------------------------------------- */

void dx_logging_verbose_info( const dxf_char_t* format, ... ) {
	if (!g_verbose_logger_mode) {
		return;
	}

	if (g_log_file == NULL || format == NULL) {
		return;
	}

	fwprintf(g_log_file, L"\n%ls [%08lx] %ls ", dx_get_current_time(),
#ifdef _WIN32
	(unsigned long)GetCurrentThreadId(),
#else
	(unsigned long)pthread_getthreadid_np(),
#endif
	g_info_prefix);
	{
		va_list ap;
		va_start(ap, format);
		dx_vlog_debug_message(format, ap);
		vfwprintf(g_log_file, format, ap);
		va_end(ap);
	}
	dx_flush_log();
}
/* -------------------------------------------------------------------------- */

void dx_logging_dbg_lock() {
#ifdef _DEBUG
	dx_mutex_lock(&g_dbg_lock);
	if (g_dbg_file == NULL)
		g_dbg_file = fopen("dxfeed-debug.log", "w");
#endif
}

void dx_logging_dbg( const dxf_char_t* format, ... ) {
#ifdef _DEBUG
	if (g_dbg_file == NULL || format == NULL) {
		return;
	}

	fwprintf(g_dbg_file, L"[%08lx] ", (unsigned long)GetCurrentThreadId());
	va_list ap;
	va_start(ap, format);
	dx_vlog_debug_message(format, ap);
	vfwprintf(g_dbg_file, format, ap);
	va_end(ap);
	fwprintf(g_dbg_file, L"\n");
#endif
}

#ifdef _DEBUG
#ifdef _WIN32
#define STACK_SIZE 256
static dxf_ubyte_t SYM_INFO[sizeof(SYM_TYPE) + 255];
static void *STACK[STACK_SIZE];
#endif
#endif

void dx_logging_dbg_stack() {
#ifdef _DEBUG
#ifdef _WIN32
	USHORT frames;
	SYMBOL_INFO *symbol = (SYMBOL_INFO *)&SYM_INFO[0];
	HANDLE process;

	process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	frames = CaptureStackBackTrace(0, 256, STACK, NULL);
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol->MaxNameLen = 255;

	for (USHORT i = 1; i < frames; i++) {
		SymFromAddr(process, (DWORD64)(STACK[i]), 0, symbol);
		dx_logging_dbg(L"* %3hu: %ls - 0x%016p", frames - i - 1, symbol->Name, (void*)symbol->Address);
	}
	dx_logging_dbg_flush();
#else
	dx_logging_dbg(L"* <STACK IS NOT SUPPORTED>");
	dx_logging_dbg_flush();
#endif
#endif
}

const char *dx_logging_dbg_sym(void *addr) {
#ifdef _DEBUG
#ifdef _WIN32
	SYMBOL_INFO *symbol = (SYMBOL_INFO *)&SYM_INFO[0];
	HANDLE process;
	process = GetCurrentProcess();
	SymInitialize(process, NULL, TRUE);
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol->MaxNameLen = 255;
	SymFromAddr(process, (DWORD64)addr, 0, symbol);
	return symbol->Name;
#else
	return "<STACK IS NOT SUPPORTED>";
#endif
#else
	return "";
#endif
}

void dx_logging_dbg_flush() {
#ifdef _DEBUG
	fflush(g_dbg_file);
#endif
}

void dx_logging_dbg_unlock(){
#ifdef _DEBUG
	dx_mutex_unlock(&g_dbg_lock);
#endif
}

/* -------------------------------------------------------------------------- */

void dx_logging_info( const dxf_char_t* format, ... ) {
	if (g_log_file == NULL || format == NULL) {
		return;
	}

	fwprintf(g_log_file, L"\n%ls [%08lx] %ls", dx_get_current_time(),
#ifdef _WIN32
	(unsigned long)GetCurrentThreadId(),
#else
	(unsigned long)pthread_getthreadid_np(),
#endif
	g_info_prefix);
	{
		va_list ap;
		va_start(ap, format);
		dx_vlog_debug_message(format, ap);
		vfwprintf(g_log_file, format, ap);
		va_end(ap);
	}
	dx_flush_log();
}

/* -------------------------------------------------------------------------- */

void dx_logging_verbose_gap (void) {
	if (!g_verbose_logger_mode) {
		return;
	}
	fwprintf(g_log_file, L"\n");
	dx_flush_log();
}