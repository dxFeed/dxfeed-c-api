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


#include "Candle.h"
#include "DXFeed.h"
#include "DXErrorCodes.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "EventData.h"
#include "DXErrorHandling.h"
#include "DXErrorCodes.h"


DXFEED_API ERRORCODE dxf_initialise_candle_symbol_attributes(OUT dxf_candle_attributes_t* candle_attributes,
                                                             dxf_const_string_t base_symbol,
                                                             dxf_char_t exchange_code,
                                                             dxf_candle_price_attribute_t price,
                                                             dxf_candle_session_attribute_t session,
                                                             dxf_candle_period_attribute_t period,
                                                             dxf_candle_alignment_attribute_t alignment)
{
    dxf_candle_attributes_data_t* attributes;
    if (candle_attributes == NULL) {
        dx_set_error_code(dx_ec_invalid_func_param);

        return DXF_FAILURE;
    }
    attributes = dx_calloc(1, sizeof(dxf_candle_attributes_data_t));
    if (attributes == NULL) {
        return DXF_FAILURE;
    }
    attributes->base_symbol = dx_create_string_src(base_symbol);
    if (attributes->base_symbol == NULL) {
        dx_free(attributes);
        return DXF_FAILURE;
    }
    attributes->exchange_code = exchange_code;
    attributes->price = price;
    attributes->session = session;
    attributes->period = period;
    attributes->alignment = alignment;

    *candle_attributes = attributes;
    return DXF_SUCCESS;
}

dxf_string_t candle_symbol_to_string(dxf_string_t string, dxf_candle_attributes_data_t attributes)
{
    return L"";
}
