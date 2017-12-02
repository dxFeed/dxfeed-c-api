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

#ifndef PRICELVELBOOK_H_INCLUDED
#define PRICELVELBOOK_H_INCLUDED

#include "PrimitiveTypes.h"
#include "EventData.h"
#include "DXTypes.h"

dxf_price_level_book_t dx_create_price_level_book(dxf_connection_t connection,
												dxf_const_string_t symbol,
												size_t srccount, dxf_ulong_t srcflags);
bool dx_close_price_level_book(dxf_price_level_book_t book);
bool dx_add_price_level_book_listener(dxf_price_level_book_t book,
									dxf_price_level_book_listener_t book_listener,
									void *user_data);
bool  dx_remove_price_level_book_listener(dxf_price_level_book_t book,
										dxf_price_level_book_listener_t book_listener);

#endif /* PRICELVELBOOK_H_INCLUDED */