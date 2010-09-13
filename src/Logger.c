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

#include "Logger.h"
#include "DXErrorHandling.h"
#include <stdio.h>
#include <stdarg.h>
#include <Windows.h>

/* -------------------------------------------------------------------------- 
*
*  Implementation Details
*
/* -------------------------------------------------------------------------- */

static const char* error_prefix = "Error:";

static const char* info_prefix = "";

static const char* default_time_string = "Incorrect time";

static bool verbose_mode;

#define CURRENT_TIME_STR_LENGTH 31
static char current_time_str[CURRENT_TIME_STR_LENGTH + 1];

// todo add different formats of date
//static const char* data_format_strings[4] = {
//    "%02u.%02u.%04u %02u:%02u:%02u:%03u",
//    "%02u.%02u.%04u %02u:%02u:%02u:%03u",
//    "%02u.%04u.%04u %02u:%02u:%02u:%03u",
//    "%04u.%02u.%04u %02u:%02u:%02u:%03u"
//};
//static dx_log_date_format_t log_date_format;

static bool show_timezone;

FILE* log_file = NULL;


#ifdef _WIN32

const char* dx_get_current_time() {
    
    SYSTEMTIME current_time;

    GetLocalTime(&current_time);
    _snprintf(current_time_str, CURRENT_TIME_STR_LENGTH, "%02u.%02u.%04u %02u:%02u:%02u:%03u", current_time.wDay,
                                                                                               current_time.wMonth,
                                                                                               current_time.wYear,
                                                                                               current_time.wHour,
                                                                                               current_time.wMinute,
                                                                                               current_time.wSecond,
                                                                                               current_time.wMilliseconds);

    if (show_timezone) {
        TIME_ZONE_INFORMATION time_zone;
        GetTimeZoneInformation(&time_zone);
        _snprintf(current_time_str + 21, 7, " GMT%+02d", time_zone.Bias / 60);
    }

    return current_time_str;
}

#else // _WIN32

const char* dx_get_current_time() {
    return default_time_string;
}

#endif // _WIN32


/* -------------------------------------------------------------------------- 
*
*  External interface
*
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */

DXFEED_API bool dx_logger_initialize( const char* file_name, bool rewrite_file, bool show_timezone_info, bool verbose ) {

    puts("Initialize logger...");

    log_file = fopen(file_name, rewrite_file ? "w" : "a");
    if (log_file == NULL) {
        printf("\ncan not open log-file %s", file_name);
        return false;
    }

    show_timezone = show_timezone_info;
    verbose_mode = verbose;

    puts("Logger is initialized");
    return true;
}

/* -------------------------------------------------------------------------- */

DXFEED_API void dx_logging_error( const char* message ) {
    if (log_file == NULL || message == NULL) {
        return;
    }

    fprintf(log_file, "\n%s %s%s", dx_get_current_time(), error_prefix, message);
}

/* -------------------------------------------------------------------------- */

//DXFEED_API void dx_logging_info (const char* message1, const char* message2) {
//    if (log_file == NULL || message1 == NULL) {
//        return;
//    }
//
//    fprintf(log_file, "\n%s %s%s", dx_get_current_time(), info_prefix, message1);
//
//    if (message2) {
//        fprintf(log_file, " %s", message2);
//    }
//}

/* -------------------------------------------------------------------------- */

DXFEED_API void dx_logging_info( const char* format, ... ) {
    if (log_file == NULL || format == NULL) {
        return;
    }

    fprintf(log_file, "\n%s %s ", dx_get_current_time(), info_prefix);

    {
        va_list ap;
        va_start(ap, format);
        vfprintf(log_file, format, ap);
        va_end(ap);
    }
}
/* -------------------------------------------------------------------------- */

void dx_logging_last_error() {
    const char* descr;
    dx_get_last_error(NULL, NULL, &descr);

    dx_logging_error(descr);
}

/* -------------------------------------------------------------------------- */

DXFEED_API void dx_logging_verbose_info(const char* format, ...) {
    if (!verbose_mode) {
        return;
    }

    if (log_file == NULL || format == NULL) {
        return;
    }

    fprintf(log_file, "\n%s %s ", dx_get_current_time(), info_prefix);

    {
        va_list ap;
        va_start(ap, format);
        vfprintf(log_file, format, ap);
        va_end(ap);
    }
}
