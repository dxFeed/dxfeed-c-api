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

#ifndef LOGGER_H
#define LOGGER_H

#include "DXFeed.h"
#include "PrimitiveTypes.h"

typedef enum dx_log_date_format {
    dx_ltf_DD_MM_YYYY,
    dx_ltf_MM_DD_YYYY,
    dx_ltf_DD_MMMM_YYYY,
    dx_ltf_MMMM_DD_YYYY
} dx_log_time_format_t;

DXFEED_API bool dx_logger_initialize (const char* file_name, bool rewrite, bool show_timezone_info);

void dx_logging_error (const char* message);

void dx_logging_info (const char* message);

void dx_logging_last_error();

#endif // LOGGER_H