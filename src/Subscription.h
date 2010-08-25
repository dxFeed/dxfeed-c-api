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
#ifndef SUBSCRIPTION_H
#define SUBSCRIPTION_H

#include "PrimitiveTypes.h"
#include "Parser.h"

enum dx_result_t dx_create_subscription(dx_byte_t* out, dx_int_t out_len, enum dx_message_type_t type, dx_int_t chiper, dx_string_t symbol, dx_int_t record_id);

#endif // SUBSCRIPTION_H