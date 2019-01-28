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

#if defined(_WIN32) && defined(_DEBUG)
void dx_log_debug_message(const dxf_char_t *format, ...);
#else
#define dx_log_debug_message(f, ...)
#endif

//typedef DXFEED_API enum dx_log_date_format {
//    dx_ltf_DD_MM_YYYY = 0,
//    dx_ltf_MM_DD_YYYY,
//    dx_ltf_DD_MMMM_YYYY,
//    dx_ltf_MMMM_DD_YYYY,
//} dx_log_date_format_t;

void dx_logging_error (dxf_const_string_t message);
void dx_logging_error_by_code(int error_code);
void dx_logging_info (const dxf_char_t* format, ...);
void dx_logging_verbose_info (const dxf_char_t* format, ...);
void dx_logging_verbose_gap (void);

void dx_logging_dbg_lock ();
void dx_logging_dbg (const dxf_char_t* format, ...);
void dx_logging_dbg_stack ();
const char *dx_logging_dbg_sym (void *addr);
void dx_logging_dbg_flush ();
void dx_logging_dbg_unlock ();

void dx_logging_last_error (void);
void dx_logging_last_error_verbose (void);

#endif // LOGGER_H