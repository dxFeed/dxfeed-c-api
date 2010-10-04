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

#include <limits.h>

#include "BufferedInput.h"
#include "DXErrorHandling.h"
#include "DXMemory.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"

/* -------------------------------------------------------------------------- */
/*
 *	Buffered input connection context
 */
/* -------------------------------------------------------------------------- */

typedef struct {
    dx_byte_t* inBuffer;
    dx_int_t inBufferLength;
    dx_int_t inBufferLimit;
    dx_int_t currentInBufferPosition;
} dx_buffered_input_connection_context_t;

#define CONTEXT_FIELD(context, field) \
    (((dx_buffered_input_connection_context_t*)context)->field)
    
#define CONTEXT_VAL(ctx) \
    dx_buffered_input_connection_context_t* ctx

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_buffered_input) {
    dx_buffered_input_connection_context_t* context = dx_calloc(1, sizeof(dx_buffered_input_connection_context_t));
    
    if (context == NULL) {
        return false;
    }
    
    if (!dx_set_subsystem_data(connection, dx_ccs_buffered_input, context)) {
        dx_free(context);
        
        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_buffered_input) {
    dx_buffered_input_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_buffered_input);
    
    if (context != NULL) {
        dx_free(context);
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

void* dx_get_buffered_input_connection_context (dxf_connection_t connection) {
    return dx_get_subsystem_data(connection, dx_ccs_buffered_input);
}

/* -------------------------------------------------------------------------- */
/*
 *	Interface functions implementation
 */
/* -------------------------------------------------------------------------- */

void dx_set_in_buffer (void* context, dx_byte_t* buf, dx_int_t length) {
    CONTEXT_FIELD(context, inBuffer) = buf;
    CONTEXT_FIELD(context, inBufferLength) = length;
}

/* -------------------------------------------------------------------------- */

void dx_set_in_buffer_position (void* context, dx_int_t pos) {
    CONTEXT_FIELD(context, currentInBufferPosition) = pos;
}

/* -------------------------------------------------------------------------- */

void dx_set_in_buffer_limit (void* context, dx_int_t limit) {
    CONTEXT_FIELD(context, inBufferLimit) = limit;
}

/* -------------------------------------------------------------------------- */

dx_int_t dx_get_in_buffer_position (void* context) {
    return CONTEXT_FIELD(context, currentInBufferPosition);
}

/* -------------------------------------------------------------------------- */

dx_int_t dx_get_in_buffer_limit (void* context) {
    return CONTEXT_FIELD(context, inBufferLimit);
}

/* -------------------------------------------------------------------------- */

dx_byte_t* dx_get_in_buffer (void* context, OUT dx_int_t* size) {
    if (size != NULL) {
        *size = CONTEXT_FIELD(context, inBufferLength);
    }

    return CONTEXT_FIELD(context, inBuffer);
}

/* -------------------------------------------------------------------------- */

dx_result_t checkValid (CONTEXT_VAL(ctx), void* val, dx_int_t len) {
    if (ctx->inBuffer == NULL) {
        return setParseError(dx_pr_buffer_not_initialized);
    }
    
    if (val == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }
    
    if (ctx->inBufferLength - ctx->currentInBufferPosition < len) {
        return setParseError(dx_pr_out_of_buffer);
    }

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_byte_impl (CONTEXT_VAL(ctx), OUT dx_byte_t* val);

dx_result_t readUTF2 (CONTEXT_VAL(ctx), int first, OUT dx_int_t* res) {
    dx_byte_t second;
   
    if (res == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }

    CHECKED_CALL_2(dx_read_byte_impl, ctx, &second);

    if ((second & 0xC0) != 0x80) {
        return setParseError(dx_pr_bad_utf_data_format);
    }

    *res = (dx_char_t)(((first & 0x1F) << 6) | (second & 0x3F));
    
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_short_impl (CONTEXT_VAL(ctx), OUT dx_short_t* val);

dx_result_t readUTF3 (CONTEXT_VAL(ctx), int first, OUT dx_int_t* res) {
    dx_short_t tail;
    
    if (res == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }

    CHECKED_CALL_2(dx_read_short_impl, ctx, &tail);

    if ((tail & 0xC0C0) != 0x8080) {
        return setParseError(dx_pr_bad_utf_data_format);
    }

    *res = (dx_char_t)(((first & 0x0F) << 12) | ((tail & 0x3F00) >> 2) | (tail & 0x3F));
    
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t readUTF4 (CONTEXT_VAL(ctx), int first, OUT dx_int_t* res) {
    dx_byte_t second;
    dx_short_t tail;

    if (res == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }

    CHECKED_CALL_2(dx_read_byte_impl, ctx, &second);
    CHECKED_CALL_2(dx_read_short_impl, ctx, &tail);

    if ((second & 0xC0) != 0x80 || (tail & 0xC0C0) != 0x8080) {
        return setParseError(dx_pr_bad_utf_data_format);
    }

    *res = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((tail & 0x3F00) >> 2) | (tail & 0x3F);

    if (*res > 0x10FFFF) {
        return setParseError(dx_pr_bad_utf_data_format);
    }

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_result_t readUTFBody (CONTEXT_VAL(ctx), int utflen, OUT dx_string_t* str) {
    dx_string_t chars;
    dx_int_t count = 0;
    dx_int_t tmpCh;

    if (str == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }

    if (utflen == 0) {
        *str = (dx_string_t)dx_calloc(1, sizeof(dx_char_t));
        
        return parseSuccessful();
    }

    chars = dx_create_string(utflen);
    
    if (chars == NULL) {
        return R_FAILED;
    }
    
    while (utflen > 0) {
        dx_byte_t c;
        
        CHECKED_CALL_2(dx_read_byte_impl, ctx, &c);

        if (c >= 0) {
            --utflen;
            chars[count++] = (dx_char_t)c;
        } else if ((c & 0xE0) == 0xC0) {
            utflen -= 2;
            
            CHECKED_CALL_3(readUTF2, ctx, c, &tmpCh);
            
            chars[count++] = tmpCh;
        } else if ((c & 0xF0) == 0xE0) {
            utflen -= 3;
            
            CHECKED_CALL_3(readUTF3, ctx, c, &tmpCh);
            
            chars[count++] = tmpCh;
        } else if ((c & 0xF8) == 0xF0) {
            dx_int_t tmpInt;
            dx_int_t res;
            utflen -= 4;
            
            CHECKED_CALL_3(readUTF4, ctx, c, &tmpInt);
            CHECKED_CALL_5(toChars, tmpInt, count, utflen, &chars, &res);
            
            count += res;
        } else {
            return setParseError(dx_pr_bad_utf_data_format);
        }
    }
    
    if (utflen < 0) {
        return setParseError(dx_pr_bad_utf_data_format);
    }

    chars[count] = 0; // end of string
    *str = chars;

    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */

dx_int_t skip (CONTEXT_VAL(ctx), dx_int_t n) {
    dx_int_t remaining = n;
    
    while (remaining > 0) {
        dx_int_t skipping;
        
        if (ctx->currentInBufferPosition >= ctx->inBufferLength) {
            break;
        }
        
        skipping = MIN(remaining, ctx->inBufferLength - ctx->currentInBufferPosition);
        ctx->currentInBufferPosition += skipping;
        remaining -= skipping;
    }
    
    return n - remaining;
}

/* -------------------------------------------------------------------------- */

dx_result_t readFully (CONTEXT_VAL(ctx), dx_byte_t* b, dx_long_t bLength, dx_int_t off, dx_int_t len) {
    if ((off | len | (off + len) | (bLength - (off + len))) < 0) {
        return setParseError(dx_pr_index_out_of_bounds);
    }
    
    if (ctx->inBufferLength - ctx->currentInBufferPosition < len){
        return setParseError(dx_pr_out_of_buffer);
    }

    dx_memcpy(b + off, ctx->inBuffer + ctx->currentInBufferPosition, len);
    
    ctx->currentInBufferPosition += len;
    
    return parseSuccessful();
}

/* -------------------------------------------------------------------------- */
/*
 *	Interface functions implementation
 */
/* -------------------------------------------------------------------------- */

#define IMPL_CALLER_FUN_BODY(fun_name, param_type) \
    dx_result_t fun_name (void* context, OUT param_type* val) { \
        return fun_name##_impl((dx_buffered_input_connection_context_t*)context, val); \
    }
    
/* -------------------------------------------------------------------------- */

dx_result_t dx_read_boolean_impl (CONTEXT_VAL(ctx), OUT dx_bool_t* val) {
    CHECKED_CALL_3(checkValid, ctx, val, 1);
    
    *val = ctx->inBuffer[ctx->currentInBufferPosition++];
    
    return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_boolean, dx_bool_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_byte_impl (CONTEXT_VAL(ctx), OUT dx_byte_t* val) {
    CHECKED_CALL_3(checkValid, ctx, val, 1);

    *val = ctx->inBuffer[ctx->currentInBufferPosition++];
    
    return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_byte, dx_byte_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_unsigned_byte_impl (CONTEXT_VAL(ctx), OUT dx_uint_t* val) {
	CHECKED_CALL_3(checkValid, ctx, val, 1);
    
    *val = ((dx_uint_t)ctx->inBuffer[ctx->currentInBufferPosition++]) & 0xFF;
    
    return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_unsigned_byte, dx_uint_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_short_impl (CONTEXT_VAL(ctx), OUT dx_short_t* val) {
    CHECKED_CALL_3(checkValid, ctx, val, 2);
	
    *val = ((dx_short_t)ctx->inBuffer[ctx->currentInBufferPosition++] << 8);
    *val = *val | ((dx_short_t)ctx->inBuffer[ctx->currentInBufferPosition++] & 0xFF);

    return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_short, dx_short_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_unsigned_short_impl (CONTEXT_VAL(ctx), OUT dx_uint_t* val) {
	CHECKED_CALL_3(checkValid, ctx, val, 2);
	
    *val = ((dx_uint_t)(ctx->inBuffer[ctx->currentInBufferPosition++] & 0xFF) << 8);
    *val = *val | ((dx_uint_t)ctx->inBuffer[ctx->currentInBufferPosition++] & 0xFF);
    
    return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_unsigned_short, dx_uint_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_int_impl (CONTEXT_VAL(ctx), OUT dx_int_t* val) {
    CHECKED_CALL_3(checkValid, ctx, val, 4);

    *val = ((dx_int_t) (ctx->inBuffer[ctx->currentInBufferPosition++] & 0xFF) << 24);
	*val = *val | ((dx_int_t)(ctx->inBuffer[ctx->currentInBufferPosition++] & 0xFF) << 16);
	*val = *val | ((dx_int_t)(ctx->inBuffer[ctx->currentInBufferPosition++]  & 0xFF) << 8);
	*val = *val | ((dx_int_t)ctx->inBuffer[ctx->currentInBufferPosition++] & 0xFF);
	
	return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_int, dx_int_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_long_impl (CONTEXT_VAL(ctx), OUT dx_long_t* val) {
    CHECKED_CALL_3(checkValid, ctx, val, 8);

    *val = ((dx_long_t)ctx->inBuffer[ctx->currentInBufferPosition++] << 56);
    *val = *val | ((dx_long_t)ctx->inBuffer[ctx->currentInBufferPosition++] << 48);
    *val = *val | ((dx_long_t)ctx->inBuffer[ctx->currentInBufferPosition++] << 40);
    *val = *val | ((dx_long_t)ctx->inBuffer[ctx->currentInBufferPosition++] << 32);
    *val = *val | ((dx_long_t)ctx->inBuffer[ctx->currentInBufferPosition++] << 24);
    *val = *val | ((dx_long_t)ctx->inBuffer[ctx->currentInBufferPosition++] << 16);
    *val = *val | ((dx_long_t)ctx->inBuffer[ctx->currentInBufferPosition++] << 8);
    *val = *val | ((dx_long_t)ctx->inBuffer[ctx->currentInBufferPosition++] & 0xFF);

    return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_long, dx_long_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_float_impl (CONTEXT_VAL(ctx), OUT dx_float_t* val) {
    dx_int_t intVal;
    
    CHECKED_CALL_2(dx_read_int_impl, ctx, &intVal);

    *val = *((dx_float_t*)(&intVal));
    
    return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_float, dx_float_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_double_impl (CONTEXT_VAL(ctx), OUT dx_double_t* val) {
    dx_long_t longVal;
    
    CHECKED_CALL_2(dx_read_long_impl, ctx, &longVal);

    *val = *((dx_double_t*)(&longVal));
    
    return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_double, dx_double_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_line_impl (CONTEXT_VAL(ctx), OUT dx_string_t* val) {
    const int tmpBufSize = 128;
    dx_string_t tmpBuffer = NULL;
    int count = 0;
    
    CHECKED_CALL_3(checkValid, ctx, val, 1);

    tmpBuffer = (dx_char_t*)dx_calloc(tmpBufSize, sizeof(dx_char_t));
    
    if (tmpBuffer == NULL) {
        return R_FAILED;
    }
    
    while (ctx->currentInBufferPosition < ctx->inBufferLength) {
        dx_char_t c = ctx->inBuffer[ctx->currentInBufferPosition++] & 0xFF;
        
        if (c == '\n')
            break;
        if (c == '\r') {
            if ((ctx->currentInBufferPosition < ctx->inBufferLength) && ctx->inBuffer[ctx->currentInBufferPosition] == '\n') {
                ++ctx->currentInBufferPosition;
            }
            
            break;
        }

        if (count >= tmpBufSize) {
            dx_char_t* tmp = (dx_char_t*)dx_calloc(tmpBufSize << 1, sizeof(dx_char_t));
            
            if (tmp == NULL) {
                dx_free(tmpBuffer);
                
                return R_FAILED;
            }
            
            dx_memcpy(tmp, tmpBuffer, tmpBufSize * sizeof(dx_char_t));
            dx_free(tmpBuffer);
            
            tmpBuffer = tmp;
        }
        
        tmpBuffer[count++] = c;
    }
    
    tmpBuffer[count] = 0;
    *val = tmpBuffer;
    
    return R_SUCCESSFUL;
}

IMPL_CALLER_FUN_BODY(dx_read_line, dx_string_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_utf_impl (CONTEXT_VAL(ctx), OUT dx_string_t* val) {
    dx_int_t utflen;
    
    CHECKED_CALL_2(dx_read_unsigned_short_impl, ctx, &utflen);
    
    return readUTFBody(ctx, utflen, OUT val);
}

IMPL_CALLER_FUN_BODY(dx_read_utf, dx_string_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_compact_int_impl (CONTEXT_VAL(ctx), OUT dx_int_t* val) {
    dx_int_t n;
    
    if (val == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }
    
    // The ((n << k) >> k) expression performs two's complement.
    CHECKED_CALL_2(dx_read_unsigned_byte_impl, ctx, &n);
    
    if (n < 0x80) {
        *val = (n << 25) >> 25;
        
        return parseSuccessful();
    }
    
    if (n < 0xC0) {
        dx_int_t m;
        
        CHECKED_CALL_2(dx_read_unsigned_byte_impl, ctx, &m);

        *val = (((n << 8) | m) << 18) >> 18;
        
        return parseSuccessful();
    }
    
    if (n < 0xE0) {
        dx_int_t m;
        
        CHECKED_CALL_2(dx_read_unsigned_short_impl, ctx, &m);

        *val = (((n << 16) | m) << 11) >> 11;
        
        return parseSuccessful();
    }
    
    if (n < 0xF0) {
        dx_int_t tmpByte;
        dx_int_t tmpShort;

        CHECKED_CALL_2(dx_read_unsigned_byte_impl, ctx, &tmpByte);
        CHECKED_CALL_2(dx_read_unsigned_short_impl, ctx, &tmpShort);

        *val = (((n << 24) | (tmpByte << 16) | tmpShort) << 4) >> 4;
        
        return parseSuccessful();
    }


    // The encoded number is possibly out of range, some bytes have to be skipped.
    // The skipBytes(...) does the strange thing, thus dx_read_unsigned_byte() is used.
    while (((n <<= 1) & 0x10) != 0) {
        skip(ctx, 1);
    }

    CHECKED_CALL_2(dx_read_int_impl, ctx, &n);
    
	*val = n;

    return parseSuccessful();
}

IMPL_CALLER_FUN_BODY(dx_read_compact_int, dx_int_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_compact_long_impl (CONTEXT_VAL(ctx), OUT dx_long_t* val) {
    dx_uint_t n;
    dx_int_t tmpInt;
    
    if (val == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }

    // The ((n << k) >> k) expression performs two's complement.
    CHECKED_CALL_2(dx_read_unsigned_byte_impl, ctx, &n);
    
    if (n < 0x80) {
        *val = (n << 25) >> 25;
        
        return parseSuccessful();
    }
    
    if (n < 0xC0) {
        dx_uint_t tmpByte;
        
        CHECKED_CALL_2(dx_read_unsigned_byte_impl, ctx, &tmpByte);
        
        *val = (((n << 8) | tmpByte) << 18) >> 18;
        
        return parseSuccessful();
    }
    
    if (n < 0xE0) {
        dx_int_t tmpShort;
        
        CHECKED_CALL_2(dx_read_unsigned_short_impl, ctx, &tmpShort);
        
        *val = (((n << 16) | tmpShort) << 11) >> 11;
        
        return parseSuccessful();
    }
    
    if (n < 0xF0) {
        dx_int_t tmpByte;
        dx_int_t tmpShort;
        
        CHECKED_CALL_2(dx_read_unsigned_byte_impl, ctx, &tmpByte);
        CHECKED_CALL_2(dx_read_unsigned_short_impl, ctx, &tmpShort);
        
        *val = (((n << 24) | (tmpByte << 16) | tmpShort) << 4) >> 4;
        
        return parseSuccessful();
    }
    
    if (n < 0xF8) {
        n = (n << 29) >> 29;
    } else if (n < 0xFC) {
        dx_int_t tmpByte;
        
        CHECKED_CALL_2(dx_read_unsigned_byte_impl, ctx, &tmpByte);
        
        n = (((n << 8) | tmpByte) << 22) >> 22;
    } else if (n < 0xFE) {
        dx_int_t tmpShort;
        
        CHECKED_CALL_2(dx_read_unsigned_short_impl, ctx, &tmpShort);
        
        n = (((n << 16) | tmpShort) << 15) >> 15;
    } else if (n < 0xFF) {
        dx_byte_t tmpByte;
        dx_int_t tmpShort;
        
        CHECKED_CALL_2(dx_read_byte_impl, ctx, &tmpByte);
        CHECKED_CALL_2(dx_read_unsigned_short_impl, ctx, &tmpShort);
        
        n = (tmpByte << 16) | tmpShort;
    } else {
        CHECKED_CALL_2(dx_read_int_impl, ctx, &n);
    }
    
    CHECKED_CALL_2(dx_read_int_impl, ctx, &tmpInt);
    
    *val = ((dx_long_t)n << 32) | (tmpInt & 0xFFFFFFFFL);
    
    return parseSuccessful();
}

IMPL_CALLER_FUN_BODY(dx_read_compact_long, dx_long_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_byte_array_impl (CONTEXT_VAL(ctx), OUT dx_byte_t** val) {
    dx_byte_t* bytes;
    dx_long_t length;
    
    if (val == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }
    
    CHECKED_CALL_2(dx_read_compact_long_impl, ctx, &length);
    
    if (length < -1 || length > INT_MAX) {
        return setParseError(dx_pr_illegal_length);
    }
    
    if (length == -1) {
        *val = NULL;
        
        return parseSuccessful();
    }
    
    if (length == 0) {
        *val = (dx_byte_t*)dx_malloc(0);
        
        if (*val == NULL) {
            return R_FAILED;
        }
        
        return parseSuccessful();
    }

    bytes = dx_malloc((int)length);
    
    if (bytes == NULL) {
        return R_FAILED;
    }
    
    if (readFully(ctx, bytes, length, 0, (dx_int_t)length) != R_SUCCESSFUL) {
        dx_free(bytes);
        
        return R_FAILED;
    }
    
    *val = bytes;
    
    return parseSuccessful();
}

IMPL_CALLER_FUN_BODY(dx_read_byte_array, dx_byte_t*)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_utf_char_impl (CONTEXT_VAL(ctx), OUT dx_int_t* val) {
    dx_byte_t c;
    
    if (val == NULL) {
        return setParseError(dx_pr_illegal_argument);
    }
    
    CHECKED_CALL_2(dx_read_byte_impl, ctx, &c);
    
    if (c >= 0) {
        *val = (dx_char_t)c;
        
        return parseSuccessful();
    }
    
    if ((c & 0xE0) == 0xC0) {
        return readUTF2(ctx, c, val);
    }
    
    if ((c & 0xF0) == 0xE0) {
        return readUTF3(ctx, c, val);
    }
    
    if ((c & 0xF8) == 0xF0) {
        return readUTF4(ctx, c, val);
    }

    return setParseError(dx_pr_bad_utf_data_format);
}

IMPL_CALLER_FUN_BODY(dx_read_utf_char, dx_int_t)

/* -------------------------------------------------------------------------- */

dx_result_t dx_read_utf_string_impl (CONTEXT_VAL(ctx), OUT dx_string_t* val) {
    dx_long_t utflen;
    
    CHECKED_CALL_2(dx_read_compact_long_impl, ctx, &utflen);
    
    if (utflen < -1 || utflen > INT_MAX) {
        return setParseError(dx_pr_illegal_length);
    }
    
    if (utflen == -1) {
        *val = NULL;
        
        return parseSuccessful();
    }
    
    return readUTFBody(ctx, (dx_int_t)utflen, val);
}

IMPL_CALLER_FUN_BODY(dx_read_utf_string, dx_string_t)