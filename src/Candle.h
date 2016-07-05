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

#ifndef CANDLE_H_INCLUDED
#define CANDLE_H_INCLUDED

#include "EventData.h"
#include "DXTypes.h"
#include "PrimitiveTypes.h"

bool dx_candle_symbol_to_string(dxf_candle_attributes_t attributes, OUT dxf_string_t* string);

#endif /* CANDLE_H_INCLUDED */