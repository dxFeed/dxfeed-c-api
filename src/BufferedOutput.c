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
#include <malloc.h>
#include <limits.h>

#include "BufferedOutput.h"
#include "DXErrorHandling.h"
#include "DXMemory.h"


// pointer to extern buffer
dx_byte_t* outBuffer = 0;
dx_int_t   outBufferLength = 0;
dx_int_t   currentOutBufferPosition = 0;

////////////////////////////////////////////////////////////////////////////////
void setOutBuffer( dx_byte_t* buf ) {
    outBuffer = buf;
}

////////////////////////////////////////////////////////////////////////////////
// ========== Implementation Details ==========
////////////////////////////////////////////////////////////////////////////////

enum dx_result_t checkWrite(int requiredCapacity) {
    if ( !outBuffer ) {
        return setParseError(pr_buffer_not_initialized);
    }

    if ( outBufferLength - currentOutBufferPosition < requiredCapacity && ensureCapacity(requiredCapacity) != R_SUCCESSFUL ) {
        return R_FAILED;
    }

    return parseSuccessful();
}

void writeUTF2Unchecked(dx_int_t codePoint) {
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xC0 | codePoint >> 6);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0x80 | codePoint & 0x3F);
}

////////////////////////////////////////////////////////////////////////////////
void writeUTF3Unchecked(dx_int_t codePoint) {
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xE0 | codePoint >> 12);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0x80 | codePoint >> 6 & 0x3F);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0x80 | codePoint & 0x3F);
}

////////////////////////////////////////////////////////////////////////////////
void writeUTF4Unchecked(dx_int_t codePoint) {
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xF0 | codePoint >> 18);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0x80 | codePoint >> 12 & 0x3F);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0x80 | codePoint >> 6 & 0x3F);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0x80 | codePoint & 0x3F);
}

////////////////////////////////////////////////////////////////////////////////
void writeIntUnchecked(dx_int_t val) {
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 24);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 16);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;
}

////////////////////////////////////////////////////////////////////////////////
// ========== external interface ==========
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//enum DXResult write(dx_int_t b) {
//    enum DXResult res;
//    if (currentOutBufferPosition >= outBufferLength && ((res = ensureCapacity(1)) != DS_Successful) ) {
//        return res;
//    }
//
//    buffer[currentOutBufferPosition++] = (dx_byte_t)b;
//    return parseSuccessful();
//}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t write(const dx_byte_t* b, dx_int_t bLen, dx_int_t off, dx_int_t len) {
    if ((off | len | (off + len) | (bLen - (off + len))) < 0) {
        return setParseError(pr_index_out_of_bounds);
    }

    if (outBufferLength - currentOutBufferPosition < len && (ensureCapacity(len) != R_SUCCESSFUL) ) {
        return R_FAILED;
    }

    memcpy(outBuffer + currentOutBufferPosition, b + off, len);
    currentOutBufferPosition += len;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeBoolean( OUT dx_bool_t val ) {
    if (checkWrite(1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = val ? (dx_byte_t)1 : (dx_byte_t)0;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeByte( dx_byte_t val ) {
    if (checkWrite(1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeShort( dx_short_t val ) {
    if (checkWrite(2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeChar( dx_char_t val ) {
    if (checkWrite(2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeInt( dx_int_t val ) {
    if (checkWrite(4) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 24);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 16);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeLong( dx_long_t val ) {
    if (checkWrite(8) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 56);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 48);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 40);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 32);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 24);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 16);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeFloat( dx_float_t val ) {
    return writeInt(*((dx_int_t*)&val));
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeDouble( dx_double_t val) {
    return writeLong(*((dx_long_t*)&val));
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeBytes( const dx_char_t* val ) {
    enum dx_result_t res = R_SUCCESSFUL;
    size_t length = wcslen(val);
    size_t i = 0;
    for (;i < length && res == R_SUCCESSFUL; i++) {
        res = writeByte((dx_byte_t)val[i]);
    }

    if (res == R_FAILED) {
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeChars( const dx_char_t* val ) {
    enum dx_result_t res = R_SUCCESSFUL;
    size_t length = wcslen(val);
    size_t i = 0;
    for (; i < length && res == R_SUCCESSFUL; i++) {
        res = writeChar(val[i]);
    }

    if (res == R_FAILED) {
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeUTF( const dx_string_t val ) {
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
        return setParseError(pr_bad_utf_data_format);
    }

    if (writeShort(utflen) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    for (i = 0; i < strlen; i++) {
        dx_char_t c;
        if (outBufferLength - currentOutBufferPosition < 3 && ensureCapacity(3) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        c = val[i];
        if (c > 0 && c <= 0x007F)
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)c;
        else if (c <= 0x07FF)
            writeUTF2Unchecked(c);
        else
            writeUTF3Unchecked(c);
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeCompactInt(dx_int_t val) {
    if (checkWrite(5) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if (val >= 0) {
        if (val < 0x40) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;
        } else if (val < 0x2000) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0x80 | val >> 8);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;
        } else if (val < 0x100000) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xC0 | val >> 16);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 8);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;
        } else if (val < 0x08000000) {
            writeIntUnchecked(0xE0000000 | val);
        } else {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)0xF0;
            writeIntUnchecked(val);
        }
    } else {
        if (val >= -0x40) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0x7F & val);
        } else if (val >= -0x2000) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xBF & val >> 8);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;
        } else if (val >= -0x100000) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xDF & val >> 16);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(val >> 8);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)val;
        } else if (val >= -0x08000000) {
            writeIntUnchecked(0xEFFFFFFF & val);
        } else {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)0xF7;
            writeIntUnchecked(val);
        }
    }
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeCompactLong(dx_long_t v) {
    dx_int_t hi;
    if (v == (dx_long_t)(dx_int_t)v) {
        return writeCompactInt((dx_int_t)v);
    }

    if (checkWrite(9) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    hi = (dx_int_t)(v >> 32);
    if (hi >= 0) {
        if (hi < 0x04) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xF0 | hi);
        } else if (hi < 0x0200) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xF8 | hi >> 8);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)hi;
        } else if (hi < 0x010000) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)0xFC;
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(hi >> 8);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)hi;
        } else if (hi < 0x800000) {
            writeIntUnchecked(0xFE000000 | hi);
        } else {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)0xFF;
            writeIntUnchecked(hi);
        }
    } else {
        if (hi >= -0x04) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xF7 & hi);
        } else if (hi >= -0x0200) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(0xFB & hi >> 8);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)hi;
        } else if (hi >= -0x010000) {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)0xFD;
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)(hi >> 8);
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)hi;
        } else if (hi >= -0x800000) {
            writeIntUnchecked(0xFEFFFFFF & hi);
        } else {
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)0xFF;
            writeIntUnchecked(hi);
        }
    }
    writeIntUnchecked((int)v);

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeByteArray(const dx_byte_t* bytes, dx_int_t size) {
    if (bytes == NULL) {
        return writeCompactInt(-1);
    }

    if (writeCompactInt(size) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (write(bytes, size, 0, size) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t writeUTFChar(dx_int_t codePoint) {
    if (checkWrite(4) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if (codePoint < 0) {
        return setParseError(pr_bad_utf_data_format);
    }

    if (codePoint <= 0x007F) {
        outBuffer[currentOutBufferPosition++] = (dx_byte_t)codePoint;
    } else if (codePoint <= 0x07FF) {
        writeUTF2Unchecked(codePoint);
    } else if (codePoint <= 0xFFFF) {
        writeUTF3Unchecked(codePoint);
    } else if (codePoint <= 0x10FFFF) {
        writeUTF4Unchecked(codePoint);
    } else {
        return setParseError(pr_bad_utf_data_format);
    }

    return parseSuccessful();
}

enum dx_result_t writeUTFString(const dx_string_t str) {
    size_t strlen;
    size_t utflen;
    size_t i;

    if (str == NULL) {
        return writeCompactInt(-1);
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
        return setParseError(pr_bad_utf_data_format);
    }

    if (writeCompactInt((dx_int_t)utflen) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    for (i = 0; i < strlen;) {
        dx_char_t c;
        if (outBufferLength - currentOutBufferPosition < 4 && (ensureCapacity(4) != R_SUCCESSFUL)) {
            return R_FAILED;
        }
        c = str[i++];
        if (c <= 0x007F)
            outBuffer[currentOutBufferPosition++] = (dx_byte_t)c;
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
enum dx_result_t ensureCapacity( int requiredCapacity ) {
    if (INT_MAX - currentOutBufferPosition < outBufferLength) {
        return setParseError(pr_buffer_overflow);
    }

    if (requiredCapacity > outBufferLength) {
        dx_int_t length = MAX(MAX((dx_int_t)MIN((dx_long_t)outBufferLength << 1, (dx_long_t)INT_MAX), 1024), requiredCapacity);
        dx_byte_t* newBuffer = (dx_byte_t*)malloc(length);
        memcpy(newBuffer, outBuffer, outBufferLength);
        free(outBuffer);
        outBuffer = newBuffer;
        outBufferLength = length;
    }
    return parseSuccessful();
}

