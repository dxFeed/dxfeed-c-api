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
#include "ParserCommon.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "DXThreads.h"
#include "DXErrorCodes.h"

/* -------------------------------------------------------------------------- */
/*
 *	Error related stuff
 */
/* -------------------------------------------------------------------------- */

static const dx_error_code_descr_t g_logger_errors[] = {
    { dx_lec_failed_to_open_file, L"Failed to open the log file" },

    { ERROR_CODE_FOOTER, ERROR_DESCR_FOOTER }
};

const dx_error_code_descr_t* logger_error_roster = g_logger_errors;

/* -------------------------------------------------------------------------- */
/*
 *	Implementation Details
 */
/* -------------------------------------------------------------------------- */

#define CURRENT_TIME_STR_LENGTH 31

static dx_const_string_t g_error_prefix = L"Error:";
static dx_const_string_t g_info_prefix = L"";
static dx_const_string_t g_default_time_string = L"Incorrect time";

static bool g_verbose_logger_mode;
static bool g_show_timezone;
static FILE* g_log_file = NULL;

static pthread_key_t g_current_time_str_key;
static bool g_key_not_created = true;

static dx_char_t current_time_str[CURRENT_TIME_STR_LENGTH + 1];

/* -------------------------------------------------------------------------- */
/*
 *	Auxiliary functions
 */
/* -------------------------------------------------------------------------- */

#ifdef _WIN32

const dx_char_t* dx_get_current_time() {
    
    SYSTEMTIME current_time;

    GetLocalTime(&current_time);
    _snwprintf(current_time_str, CURRENT_TIME_STR_LENGTH, L"%02u.%02u.%04u %02u:%02u:%02u:%03u", current_time.wDay,
                                                                                                 current_time.wMonth,
                                                                                                 current_time.wYear,
                                                                                                 current_time.wHour,
                                                                                                 current_time.wMinute,
                                                                                                 current_time.wSecond,
                                                                                                 current_time.wMilliseconds);

    if (g_show_timezone) {
        TIME_ZONE_INFORMATION time_zone;
        GetTimeZoneInformation(&time_zone);
        _snwprintf(current_time_str + 21, 7, L" GMT%+02d", time_zone.Bias / 60);
    }

    return current_time_str;
}

#else // _WIN32

dx_const_string_t dx_get_current_time () {
    return g_default_time_string;
}

#endif // _WIN32

/* -------------------------------------------------------------------------- */

void dx_current_time_buffer_destructor (void* data) {
    if (data != current_time_str) {
        dx_free(data);
    } else {
        dx_thread_data_key_destroy(g_current_time_str_key);
    }
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_get_current_time_buffer (void) {
    dx_string_t buffer = NULL;
    
    if (g_key_not_created) {
        return NULL;
    }
    
    buffer = (dx_string_t)dx_get_thread_data(g_current_time_str_key);
    
    if (buffer == NULL) {
        /* buffer for this thread wasn't yet created. */
        
        buffer = (dx_string_t)dx_calloc(CURRENT_TIME_STR_LENGTH + 1, sizeof(dx_char_t));
        
        if (buffer == NULL) {
            return NULL;
        }
        
        if (!dx_set_thread_data(g_current_time_str_key, buffer)) {
            dx_free(buffer);
            
            return NULL;
        }
    }
    
    return buffer;
}

/* -------------------------------------------------------------------------- */

bool dx_init_current_time_key (void) {
    if (!g_key_not_created) {
        return true;
    }
    
    if (!dx_thread_data_key_create(&g_current_time_str_key, dx_current_time_buffer_destructor)) {
        return false;
    }
    
    if (!dx_set_thread_data(g_current_time_str_key, current_time_str)) {
        dx_thread_data_key_destroy(g_current_time_str_key);
        
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	External interface
 */
/* -------------------------------------------------------------------------- */

DXFEED_API ERRORCODE dxf_initialize_logger (const char* file_name, bool rewrite_file, bool show_timezone_info, bool verbose) {

    puts("Initialize logger...");

    g_log_file = fopen(file_name, rewrite_file ? "w" : "a");
    if (g_log_file == NULL) {
        printf("\ncan not open log-file %s", file_name);
        return false;
    }

    g_show_timezone = show_timezone_info;
    g_verbose_logger_mode = verbose;
    
    if (!dx_init_current_time_key()) {
        
    }

    puts("Logger is initialized");
    return true;
}

/* -------------------------------------------------------------------------- */

void dx_logging_error (dx_const_string_t message ) {
    if (g_log_file == NULL || message == NULL) {
        return;
    }

    fwprintf(g_log_file, L"\n%s %s%s", dx_get_current_time(), g_error_prefix, message);
}

/* -------------------------------------------------------------------------- */

void dx_logging_last_error() {
    dx_const_string_t descr;
    dx_get_last_error(NULL, NULL, &descr);

    dx_logging_error(descr);
}

/* -------------------------------------------------------------------------- */

void dx_logging_verbose_info( const dx_char_t* format, ... ) {
    if (!g_verbose_logger_mode) {
        return;
    }

    if (g_log_file == NULL || format == NULL) {
        return;
    }

    fwprintf(g_log_file, L"\n%s %s ", dx_get_current_time(), g_info_prefix);

    {
        va_list ap;
        va_start(ap, format);
        vfwprintf(g_log_file, format, ap);
        va_end(ap);
    }
}
/* -------------------------------------------------------------------------- */

void dx_logging_info( const dx_char_t* format, ... ) {
    if (g_log_file == NULL || format == NULL) {
        return;
    }

    fwprintf(g_log_file, L"\n%s %s ", dx_get_current_time(), g_info_prefix);

    {
        va_list ap;
        va_start(ap, format);
        vfwprintf(g_log_file, format, ap);
        va_end(ap);
    }
}

/* -------------------------------------------------------------------------- */

void dx_logging_gap() {
    fwprintf(g_log_file, L"\n");
}