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

#include "ErrorReport.h"

enum dx_errcode_t dx_reg_errcodes (int subsystem_id, int errcode_count, const int* errcodes, const char* const* errcodedescr) {
    return ECSUCCESS;
};

/* reports an error. */

enum dx_errcode_t dx_set_last_error (int subsystem_id, int errcode) {
    return ECSUCCESS;
};

/* retrieves an information about a previously reported error. */

enum dx_errcode_t dx_get_last_error (int* subsystem_id, int* errcode, const char** errcodedescr) {
    return ECSUCCESS;
};
