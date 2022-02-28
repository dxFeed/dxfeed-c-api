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

#pragma once

#include "DXTypes.h"
#include "EventData.h"
#include "PrimitiveTypes.h"

#ifdef __cplusplus
extern "C" {
#endif

dxf_price_level_book_v2_t dx_create_price_level_book_v2(dxf_connection_t connection, dxf_const_string_t symbol,
														const char* source, int levels_number);

int dx_close_price_level_book_v2(dxf_price_level_book_v2_t book);

int dx_set_price_level_book_listeners_v2(dxf_price_level_book_v2_t book,
										 dxf_price_level_book_listener_t on_new_book_listener,
										 dxf_price_level_book_listener_t on_book_update_listener,
										 dxf_price_level_book_inc_listener_t on_incremental_change_listener,
										 void *user_data);

#ifdef __cplusplus
}
#endif


