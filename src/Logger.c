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

#include <Windows.h>
#include <stdio.h>
#include <stdarg.h>

#include "Logger.h"
#include "DXErrorHandling.h"
#include "BufferedIOCommon.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "DXThreads.h"
#include "DXErrorCodes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Implementation Details
 */
/* -------------------------------------------------------------------------- */

#define CURRENT_TIME_STR_LENGTH 31
#define CURRENT_TIME_STR_TIME_OFFSET 24

static dxf_const_string_t g_error_prefix = L"Error:";
static dxf_const_string_t g_info_prefix = L"";
static dxf_const_string_t g_default_time_string = L"Incorrect time";

static bool g_verbose_logger_mode;
static bool g_show_timezone;
static FILE* g_log_file = NULL;

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
        _snwprintf(time_buffer + CURRENT_TIME_STR_TIME_OFFSET, 7, L" GMT%+.2d", time_zone.Bias / 60);
    }

    return time_buffer;
}

#else // _WIN32

dxf_const_string_t dx_get_current_time () {
    return g_default_time_string;
}

#endif // _WIN32

/* -------------------------------------------------------------------------- */

static void dx_current_time_buffer_destructor (void* data) {
    if (data != current_time_str && data != NULL) {
        dx_free(data);
	}
}

static void dx_key_remover(void *data) {
	dx_log_debug_message(L"Remove logging key");
	if (g_key_created) {
		dx_thread_data_key_destroy(g_current_time_str_key);
		g_key_created = g_key_creation_attempted = false;
	}
	dx_log_debug_message(L"Remove logging key -- removed");
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
        
		dx_log_debug_message(L"dx_get_current_time_buffer()");
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
		dx_log_debug_message(L"dx_close_logger() - close log file");
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

	puts("Initialize logger...");

	if (!dx_init_error_subsystem()) {
        printf("\nCan not init error subsystem\n");
        return DXF_FAILURE;
	}

    g_log_file = fopen(file_name, rewrite_file ? "w" : "a");
    
    if (g_log_file == NULL) {
        printf("\ncan not open log-file %s", file_name);
        return DXF_FAILURE;
    }

	dx_register_process_destructor(&dx_close_logging, NULL);

    g_show_timezone = show_timezone_info ? true : false;
    g_verbose_logger_mode = verbose ? true : false;
    
    if (!dx_init_current_time_key()) {
        return DXF_FAILURE;
    }

    dx_logging_info(L"Logging started: file %s, verbose mode is %s",
                    rewrite_file ? L"rewritten" : L"not rewritten", verbose ? L"on" : L"off");
    dx_flush_log();
    
    return DXF_SUCCESS;
}

/* -------------------------------------------------------------------------- */

void dx_logging_error (dxf_const_string_t message ) {
    if (g_log_file == NULL || message == NULL) {
        return;
    }

	dx_log_debug_message(L"%s", message);
    fwprintf(g_log_file, L"\n%s [%08lx] %s%s", dx_get_current_time(), (unsigned long)GetCurrentThreadId(), g_error_prefix, message);
    dx_flush_log();
}

/* -------------------------------------------------------------------------- */

void dx_logging_last_error (void) {
    dx_logging_error(dx_get_error_description(dx_get_error_code()));
    dx_flush_log();
}

/* -------------------------------------------------------------------------- */

void dx_logging_last_error_verbose (void) {
    if (!g_verbose_logger_mode) {
        return;
    }
    
    dx_logging_error(dx_get_error_description(dx_get_error_code()));
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

    fwprintf(g_log_file, L"\n%s [%08lx] %s ", dx_get_current_time(), (unsigned long)GetCurrentThreadId(), g_info_prefix);
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

void dx_logging_info( const dxf_char_t* format, ... ) {
    if (g_log_file == NULL || format == NULL) {
        return;
    }

    fwprintf(g_log_file, L"\n%s [%08lx] %s ", dx_get_current_time(), (unsigned long)GetCurrentThreadId(), g_info_prefix);
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