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

#include <string.h>
#include <limits.h>

#include "BufferedOutput.h"
#include "DXErrorHandling.h"
#include "DXMemory.h"


// pointer to extern buffer
dx_byte_t* out_buffer = 0;
dx_int_t   out_buffer_length = 0;
dx_int_t   current_out_buffer_position = 0;

////////////////////////////////////////////////////////////////////////////////
void dx_set_out_buffer( dx_byte_t* buf, dx_int_t buf_len ) {
    out_buffer = buf;
    out_buffer_length = buf_len;
    current_out_buffer_position = 0;
}

/* -------------------------------------------------------------------------- */

dx_byte_t* dx_get_out_buffer( OUT dx_int_t* size ) {
    if (size) {
        *size = out_buffer_length;
    }

    return out_buffer;
}

/* -------------------------------------------------------------------------- */

dx_int_t dx_get_out_buffer_position() {
    return current_out_buffer_position;
}

/* -------------------------------------------------------------------------- */

void dx_set_out_buffer_position( dx_int_t pos ) {
    current_out_buffer_position = pos;
}

////////////////////////////////////////////////////////////////////////////////
// ========== Implementation Details ==========
////////////////////////////////////////////////////////////////////////////////

dx_result_t checkWrite(int requiredCapacity) {
    if ( !out_buffer ) {
        return setParseError(dx_pr_buffer_not_initialized);
    }

    if ( out_buffer_length - current_out_buffer_position < requiredCapacity && dx_ensure_capacity(requiredCapacity) != R_SUCCESSFUL ) {
        return R_FAILED;
    }

    return parseSuccessful();
}

void writeUTF2Unchecked(dx_int_t codePoint) {
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xC0 | codePoint >> 6);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(0x80 | codePoint & 0x3F);
}

////////////////////////////////////////////////////////////////////////////////
void writeUTF3Unchecked(dx_int_t codePoint) {
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xE0 | codePoint >> 12);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(0x80 | codePoint >> 6 & 0x3F);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(0x80 | codePoint & 0x3F);
}

////////////////////////////////////////////////////////////////////////////////
void writeUTF4Unchecked(dx_int_t codePoint) {
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xF0 | codePoint >> 18);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(0x80 | codePoint >> 12 & 0x3F);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(0x80 | codePoint >> 6 & 0x3F);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(0x80 | codePoint & 0x3F);
}

////////////////////////////////////////////////////////////////////////////////
void writeIntUnchecked(dx_int_t val) {
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 24);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 16);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 8);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)val;
}

////////////////////////////////////////////////////////////////////////////////
// ========== external interface ==========
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//enum DXResult write(dx_int_t b) {
//    enum DXResult res;
//    if (current_out_buffer_position >= out_buffer_length && ((res = dx_ensure_capacity(1)) != DS_Successful) ) {
//        return res;
//    }
//
//    buffer[current_out_buffer_position++] = (dx_byte_t)b;
//    return parseSuccessful();
//}

////////////////////////////////////////////////////////////////////////////////
dx_result_t write(const dx_byte_t* b, dx_int_t bLen, dx_int_t off, dx_int_t len) {
    if ((off | len | (off + len) | (bLen - (off + len))) < 0) {
        return setParseError(dx_pr_index_out_of_bounds);
    }

    if (out_buffer_length - current_out_buffer_position < len && (dx_ensure_capacity(len) != R_SUCCESSFUL) ) {
        return R_FAILED;
    }

    dx_memcpy(out_buffer + current_out_buffer_position, b + off, len);
    current_out_buffer_position += len;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_boolean( OUT dx_bool_t val ) {
    if (checkWrite(1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    out_buffer[current_out_buffer_position++] = val ? (dx_byte_t)1 : (dx_byte_t)0;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_byte( dx_byte_t val ) {
    if (checkWrite(1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    out_buffer[current_out_buffer_position++] = (dx_byte_t)val;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_short( dx_short_t val ) {
    if (checkWrite(2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 8);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_char( dx_char_t val ) {
    if (checkWrite(2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 8);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_int( dx_int_t val ) {
    if (checkWrite(4) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 24);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 16);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 8);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_long( dx_long_t val ) {
    if (checkWrite(8) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 56);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 48);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 40);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 32);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 24);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 16);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 8);
    out_buffer[current_out_buffer_position++] = (dx_byte_t)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_float( dx_float_t val ) {
    return dx_write_int(*((dx_int_t*)&val));
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_double( dx_double_t val) {
    return dx_write_long(*((dx_long_t*)&val));
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_bytes( const dx_char_t* val ) {
    dx_result_t res = R_SUCCESSFUL;
    size_t length = wcslen(val);
    size_t i = 0;
    for (;i < length && res == R_SUCCESSFUL; i++) {
        res = dx_write_byte((dx_byte_t)val[i]);
    }

    if (res == R_FAILED) {
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_chars( const dx_char_t* val ) {
    dx_result_t res = R_SUCCESSFUL;
    size_t length = wcslen(val);
    size_t i = 0;
    for (; i < length && res == R_SUCCESSFUL; i++) {
        res = dx_write_char(val[i]);
    }

    if (res == R_FAILED) {
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_utf( const dx_string_t val ) {
    size_t strlen = wcslen(val);
    dx_short_t utflen = 0;
    size_t i = 0;
    for (; i < strlen; i++) {
        dx_char_t c = val[i];
        if (c > 0 && c <= 0x007F)
            utflen++;
        else if (c <= 0x07FF)
            utflen += 2;
        else
            utflen += 3;
    }

    if ((size_t)utflen < strlen || utflen > 65535) {
        return setParseError(dx_pr_bad_utf_data_format);
    }

    if (dx_write_short(utflen) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    for (i = 0; i < strlen; i++) {
        dx_char_t c;
        if (out_buffer_length - current_out_buffer_position < 3 && dx_ensure_capacity(3) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        c = val[i];
        if (c > 0 && c <= 0x007F)
            out_buffer[current_out_buffer_position++] = (dx_byte_t)c;
        else if (c <= 0x07FF)
            writeUTF2Unchecked(c);
        else
            writeUTF3Unchecked(c);
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_compact_int(dx_int_t val) {
    if (checkWrite(5) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if (val >= 0) {
        if (val < 0x40) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)val;
        } else if (val < 0x2000) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(0x80 | val >> 8);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)val;
        } else if (val < 0x100000) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xC0 | val >> 16);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 8);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)val;
        } else if (val < 0x08000000) {
            writeIntUnchecked(0xE0000000 | val);
        } else {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)0xF0;
            writeIntUnchecked(val);
        }
    } else {
        if (val >= -0x40) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(0x7F & val);
        } else if (val >= -0x2000) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xBF & val >> 8);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)val;
        } else if (val >= -0x100000) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xDF & val >> 16);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(val >> 8);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)val;
        } else if (val >= -0x08000000) {
            writeIntUnchecked(0xEFFFFFFF & val);
        } else {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)0xF7;
            writeIntUnchecked(val);
        }
    }
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_compact_long(dx_long_t v) {
    dx_int_t hi;
    if (v == (dx_long_t)(dx_int_t)v) {
        return dx_write_compact_int((dx_int_t)v);
    }

    if (checkWrite(9) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    hi = (dx_int_t)(v >> 32);
    if (hi >= 0) {
        if (hi < 0x04) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xF0 | hi);
        } else if (hi < 0x0200) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xF8 | hi >> 8);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)hi;
        } else if (hi < 0x010000) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)0xFC;
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(hi >> 8);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)hi;
        } else if (hi < 0x800000) {
            writeIntUnchecked(0xFE000000 | hi);
        } else {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)0xFF;
            writeIntUnchecked(hi);
        }
    } else {
        if (hi >= -0x04) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xF7 & hi);
        } else if (hi >= -0x0200) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(0xFB & hi >> 8);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)hi;
        } else if (hi >= -0x010000) {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)0xFD;
            out_buffer[current_out_buffer_position++] = (dx_byte_t)(hi >> 8);
            out_buffer[current_out_buffer_position++] = (dx_byte_t)hi;
        } else if (hi >= -0x800000) {
            writeIntUnchecked(0xFEFFFFFF & hi);
        } else {
            out_buffer[current_out_buffer_position++] = (dx_byte_t)0xFF;
            writeIntUnchecked(hi);
        }
    }
    writeIntUnchecked((int)v);

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_byte_array(const dx_byte_t* bytes, dx_int_t size) {
    if (bytes == NULL) {
        return dx_write_compact_int(-1);
    }

    if (dx_write_compact_int(size) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (write(bytes, size, 0, size) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_write_utf_char(dx_int_t codePoint) {
    if (checkWrite(4) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if (codePoint < 0) {
        return setParseError(dx_pr_bad_utf_data_format);
    }

    if (codePoint <= 0x007F) {
        out_buffer[current_out_buffer_position++] = (dx_byte_t)codePoint;
    } else if (codePoint <= 0x07FF) {
        writeUTF2Unchecked(codePoint);
    } else if (codePoint <= 0xFFFF) {
        writeUTF3Unchecked(codePoint);
    } else if (codePoint <= 0x10FFFF) {
        writeUTF4Unchecked(codePoint);
    } else {
        return setParseError(dx_pr_bad_utf_data_format);
    }

    return parseSuccessful();
}

dx_result_t dx_write_utf_string(const dx_string_t str) {
    size_t strlen;
    size_t utflen;
    size_t i;

    if (str == NULL) {
        return dx_write_compact_int(-1);
    }

    strlen = wcslen(str);
    utflen = 0;
    for (i = 0; i < strlen;) {
        dx_char_t c = str[i++];
        if (c <= 0x007F)
            utflen++;
        else if (c <= 0x07FF)
            utflen += 2;
        else if (isHighSurrogate(c) && i < strlen && isLowSurrogate(str[i])) {
            i++;
            utflen += 4;
        } else
            utflen += 3;
    }
    if (utflen < strlen) {
        return setParseError(dx_pr_bad_utf_data_format);
    }

    if (dx_write_compact_int((dx_int_t)utflen) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    for (i = 0; i < strlen;) {
        dx_char_t c;
        if (out_buffer_length - current_out_buffer_position < 4 && (dx_ensure_capacity(4) != R_SUCCESSFUL)) {
            return R_FAILED;
        }
        c = str[i++];
        if (c <= 0x007F)
            out_buffer[current_out_buffer_position++] = (dx_byte_t)c;
        else if (c <= 0x07FF)
            writeUTF2Unchecked(c);
        else if (isHighSurrogate(c) && i < strlen && isLowSurrogate(str[i]))
            writeUTF4Unchecked(toCodePoint(c, str[i++]));
        else
            writeUTF3Unchecked(c);
    }

    return parseSuccessful();
}


////////////////////////////////////////////////////////////////////////////////
dx_result_t dx_ensure_capacity( int requiredCapacity ) {
    if (INT_MAX - current_out_buffer_position < out_buffer_length) {
        return setParseError(dx_pr_buffer_overflow);
    }

    if (requiredCapacity > out_buffer_length) {
        dx_int_t length = MAX(MAX((dx_int_t)MIN((dx_long_t)out_buffer_length << 1, (dx_long_t)INT_MAX), 1024), requiredCapacity);
        dx_byte_t* newBuffer = (dx_byte_t*)dx_malloc((size_t)length);
        dx_memcpy(newBuffer, out_buffer, out_buffer_length);
        dx_free(out_buffer);
        out_buffer = newBuffer;
        out_buffer_length = length;
    }
    return parseSuccessful();
}
