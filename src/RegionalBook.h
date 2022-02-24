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

#ifndef REGIONALBOOK_H_INCLUDED
#define REGIONALBOOK_H_INCLUDED

#include "PrimitiveTypes.h"
#include "EventData.h"
#include "DXTypes.h"

dxf_regional_book_t dx_create_regional_book(dxf_connection_t connection,
											dxf_const_string_t symbol);

int dx_close_regional_book(dxf_regional_book_t book);

int dx_add_regional_book_listener(dxf_regional_book_t book,
  								dxf_price_level_book_listener_t book_listener,
								void *user_data);

int dx_remove_regional_book_listener(dxf_regional_book_t book,
									dxf_price_level_book_listener_t book_listener);

int dx_add_regional_book_listener_v2(dxf_regional_book_t book,
                                    dxf_regional_quote_listener_t book_listener,
                                    void *user_data);

int dx_remove_regional_book_listener_v2(dxf_regional_book_t book,
                                        dxf_regional_quote_listener_t book_listener);

#endif /* REGIONALBOOK_H_INCLUDED */
