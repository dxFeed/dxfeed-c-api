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
#include "DXErrorHandling.h"
#include "DXMemory.h"

/* -------------------------------------------------------------------------- */
/*
*	Thread error codes
*/
/* -------------------------------------------------------------------------- */

static struct dx_error_code_descr_t g_parser_errors[] = {
    { dx_pr_successful, NULL },
    { dx_pr_failed, NULL },
    { dx_pr_buffer_overflow, "Buffer overflow" },
    { dx_pr_illegal_argument, "The argument of function is not valid" },
    { dx_pr_illegal_length, "Illegal length of string or byte array" },
    { dx_pr_bad_utf_data_format, "Bad format of UTF string" },
    { dx_pr_index_out_of_bounds, "Index of buffer is not valid" },
    { dx_pr_out_of_buffer, "reached the end of buffer" },
    { dx_pr_buffer_not_initialized, "There isn't a buffer to read" },
    { dx_pr_out_of_memory, "Out of memory" },
    { dx_pr_buffer_corrupt, "Buffer is corrupt" },
    { dx_pr_message_not_complete, "Message is not complete" },
    { dx_pr_internal_error, "Internal error" },
    { dx_pr_reserved_bit_sequence, "Reserved bit sequence" },
    { dx_pr_undefined_symbol, "Symbol is undefined" },
    { dx_pr_record_info_corrupt, "Corrupted record information" },
    { dx_pr_field_info_corrupt, "Corrupted field information" },
	{ dx_pr_wrong_record_id, "Wrong record id received" },
	{ dx_pr_type_not_supported, "Serialization of this type not supported" },
    { dx_pr_record_reading_failed, "Record reading failed" },
    { dx_pr_unknown_record_name, "Unknown record name" },
    { dx_pr_unknown_record_field, "Unknown record field" },
    
	{ ERROR_CODE_FOOTER, ERROR_DESCR_FOOTER }
};

const struct dx_error_code_descr_t* parser_error_roster = g_parser_errors;


////////////////////////////////////////////////////////////////////////////////
dx_result_t setParseError(int err) {
    dx_set_last_error(dx_sc_parser, err);
    return R_FAILED;
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t parseSuccessful() {
    dx_set_last_error(dx_sc_parser, dx_pr_successful);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum parser_result_t dx_get_parser_last_error() {
    dx_int_t res = dx_pr_successful;
    enum dx_error_function_result_t resultErr = dx_get_last_error (NULL, &res, NULL);
    if (resultErr == efr_success || resultErr == efr_no_error_stored) {
        return res;
    }

    return dx_pr_failed;
}

/**
* The minimum value of a Unicode high-surrogate code unit in the
* UTF-16 encoding. A high-surrogate is also known as a
* leading-surrogate.
*/
const dx_char_t MIN_HIGH_SURROGATE = '\uD800';

/**
* The maximum value of a Unicode high-surrogate code unit in the
* UTF-16 encoding. A high-surrogate is also known as a
* leading-surrogate.
*/
const dx_char_t MAX_HIGH_SURROGATE = '\uDBFF';

/**
* The minimum value of a Unicode low-surrogate code unit in the
* UTF-16 encoding. A low-surrogate is also known as a
* trailing-surrogate.
*/
const dx_char_t MIN_LOW_SURROGATE  = '\uDC00';

/**
* The maximum value of a Unicode low-surrogate code unit in the
* UTF-16 encoding. A low-surrogate is also known as a
* trailing-surrogate.
*/
const dx_char_t MAX_LOW_SURROGATE  = '\uDFFF';

/**
* The minimum value of a supplementary code point.
*/
const dx_int_t MIN_SUPPLEMENTARY_CODE_POINT = 0x010000;

/**
* The maximum value of a Unicode code point.
*/
dx_int_t MAX_CODE_POINT = 0x10ffff;

////////////////////////////////////////////////////////////////////////////////
int isHighSurrogate(dx_char_t ch) {
    return ch >= MIN_HIGH_SURROGATE && ch <= MAX_HIGH_SURROGATE;
}

////////////////////////////////////////////////////////////////////////////////
int isLowSurrogate(dx_char_t ch) {
    return ch >= MIN_LOW_SURROGATE && ch <= MAX_LOW_SURROGATE;
}

////////////////////////////////////////////////////////////////////////////////
dx_int_t toCodePoint(dx_char_t high, dx_char_t low) {
    return ((high - MIN_HIGH_SURROGATE) << 10)
        + (low - MIN_LOW_SURROGATE) + MIN_SUPPLEMENTARY_CODE_POINT;
}

////////////////////////////////////////////////////////////////////////////////
void toSurrogates(dx_int_t codePoint, dx_int_t index, OUT dx_string_t* dst) {
    dx_int_t offset = codePoint - MIN_SUPPLEMENTARY_CODE_POINT;
    (*dst)[index+1] = (dx_char_t)((offset & 0x3ff) + MIN_LOW_SURROGATE);
    (*dst)[index] = (dx_char_t)((offset >> 10) + MIN_HIGH_SURROGATE);
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t toChars(dx_int_t codePoint, dx_int_t dstIndex, dx_int_t dstLen, OUT dx_string_t* dst, OUT dx_int_t* res) {
    if (!dst || !(*dst) || !res || codePoint < 0 || codePoint > MAX_CODE_POINT) {
        setParseError(dx_pr_illegal_argument);
        return R_FAILED;
    }

    if (codePoint < MIN_SUPPLEMENTARY_CODE_POINT) {
        if (dstLen - dstIndex < 1) {
            setParseError(dx_pr_index_out_of_bounds);
            return R_FAILED;
        }
        (*dst)[dstIndex] = (dx_char_t)codePoint;
        *res = 1;
        return parseSuccessful();
    }

    if(dstLen - dstIndex < 2) {
        setParseError(dx_pr_index_out_of_bounds);
        return R_FAILED;
    }

    toSurrogates(codePoint, dstIndex, dst);
    *res = 2;
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_string_t dx_create_string( dx_int_t size ) {
    return (dx_string_t)dx_calloc((size_t)(size + 1), sizeof(dx_char_t));
}

/* -------------------------------------------------------------------------- */
/*
/* compact API 
/*
/* -------------------------------------------------------------------------- */

dx_int_t dx_get_compact_length(dx_long_t n) {
    if (n >= 0) {
        if (n < 0x40) {
            return 1;
        } else if (n < 0x2000) {
            return 2;
        } else if (n < 0x100000) {
            return 3;
        } else if (n < 0x08000000) {
            return 4;
        } else if (n < 0x0400000000L) {
            return 5;
        } else if (n < 0x020000000000L) {
            return 6;
        } else if (n < 0x01000000000000L) {
            return 7;
        } else if (n < 0x80000000000000L) {
            return 8;
        } else {
            return 9;
        }
    } else {
        if (n >= -0x40) {
            return 1;
        } else if (n >= -0x2000) {
            return 2;
        } else if (n >= -0x100000) {
            return 3;
        } else if (n >= -0x08000000) {
            return 4;
        } else if (n >= -0x0400000000L) {
            return 5;
        } else if (n >= -0x020000000000L) {
            return 6;
        } else if (n >= -0x01000000000000L) {
            return 7;
        } else if (n >= -0x80000000000000L) {
            return 8;
        } else {
            return 9;
        }
    }
}
