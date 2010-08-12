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

#include "ParserCommon.h"
#include "ErrorReport.h"

////////////////////////////////////////////////////////////////////////////////
enum DXResult setParseError(int err) {
    dx_set_last_error(SS_DataSerializer, err);
    return R_FAILED;
}

enum DXResult parseSuccessful() {
    dx_set_last_error(SS_DataSerializer, DS_Successful);
    return R_SUCCESSFUL;
}

/**
* The minimum value of a Unicode high-surrogate code unit in the
* UTF-16 encoding. A high-surrogate is also known as a
* leading-surrogate.
*/
const jChar MIN_HIGH_SURROGATE = '\uD800';

/**
* The maximum value of a Unicode high-surrogate code unit in the
* UTF-16 encoding. A high-surrogate is also known as a
* leading-surrogate.
*/
const jChar MAX_HIGH_SURROGATE = '\uDBFF';

/**
* The minimum value of a Unicode low-surrogate code unit in the
* UTF-16 encoding. A low-surrogate is also known as a
* trailing-surrogate.
*/
const jChar MIN_LOW_SURROGATE  = '\uDC00';

/**
* The maximum value of a Unicode low-surrogate code unit in the
* UTF-16 encoding. A low-surrogate is also known as a
* trailing-surrogate.
*/
const jChar MAX_LOW_SURROGATE  = '\uDFFF';

/**
* The minimum value of a supplementary code point.
*/
const jInt MIN_SUPPLEMENTARY_CODE_POINT = 0x010000;

/**
* The maximum value of a Unicode code point.
*/
jInt MAX_CODE_POINT = 0x10ffff;

////////////////////////////////////////////////////////////////////////////////
int isHighSurrogate(jChar ch) {
    return ch >= MIN_HIGH_SURROGATE && ch <= MAX_HIGH_SURROGATE;
}

////////////////////////////////////////////////////////////////////////////////
int isLowSurrogate(jChar ch) {
    return ch >= MIN_LOW_SURROGATE && ch <= MAX_LOW_SURROGATE;
}

////////////////////////////////////////////////////////////////////////////////
jInt toCodePoint(jChar high, jChar low) {
    return ((high - MIN_HIGH_SURROGATE) << 10)
        + (low - MIN_LOW_SURROGATE) + MIN_SUPPLEMENTARY_CODE_POINT;
}

////////////////////////////////////////////////////////////////////////////////
void toSurrogates(jInt codePoint, jInt index, OUT dx_string* dst) {
    jInt offset = codePoint - MIN_SUPPLEMENTARY_CODE_POINT;
    (*dst)[index+1] = (jChar)((offset & 0x3ff) + MIN_LOW_SURROGATE);
    (*dst)[index] = (jChar)((offset >> 10) + MIN_HIGH_SURROGATE);
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult toChars(jInt codePoint, jInt dstIndex, jInt dstLen, OUT dx_string* dst, OUT jInt* res) {
    if (!dst || !(*dst) || !res || codePoint < 0 || codePoint > MAX_CODE_POINT) {
        setParseError(DS_IllegalArgument);
        return R_FAILED;
    }

    if (codePoint < MIN_SUPPLEMENTARY_CODE_POINT) {
        if (dstLen - dstIndex < 1) {
            setParseError(DS_IndexOutOfBounds);
            return R_FAILED;
        }
        (*dst)[dstIndex] = (jChar)codePoint;
        *res = 1;
        return parseSuccessful();
    }

    if(dstLen - dstIndex < 2) {
        setParseError(DS_IndexOutOfBounds);
        return R_FAILED;
    }

    toSurrogates(codePoint, dstIndex, dst);
    *res = 2;
    return parseSuccessful();
}

