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

#include "BufferedInput.h"
#include "DXErrorHandling.h"
#include "DXMemory.h"

// pointer to extern inBuffer
dx_byte_t* inBuffer = 0;
dx_int_t   inBufferLength = 0;
dx_int_t   inBufferLimit = 0;
dx_int_t   currentInBufferPosition = 0;

////////////////////////////////////////////////////////////////////////////////
void setInBuffer( dx_byte_t* buf, dx_int_t length ) {
    inBuffer = buf;
    inBufferLength = length;
}

////////////////////////////////////////////////////////////////////////////////
void setInBufferPosition( dx_int_t pos ) {
    currentInBufferPosition = pos;
}

////////////////////////////////////////////////////////////////////////////////
void setInBufferLimit( dx_int_t limit ) {
    inBufferLimit = limit;
}

////////////////////////////////////////////////////////////////////////////////
dx_int_t getInBufferPosition() {
    return currentInBufferPosition;
}

////////////////////////////////////////////////////////////////////////////////
dx_int_t getInBufferLimit() {
    return inBufferLimit;
}

////////////////////////////////////////////////////////////////////////////////
// ========== Implementation Details ==========
////////////////////////////////////////////////////////////////////////////////

//dx_char_t* charBuffer; // Used for reading strings; lazy initialized.
//
//dx_char_t* getCharBuffer(dx_int_t capacity) {
//    if (capacity > 256)
//        return (dx_char_t*)malloc(sizeof(dx_char_t) * capacity);
//    return charBuffer != null ? charBuffer : (charBuffer = new char[256]);
//}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t checkValid(void* val, dx_int_t len) {
    if (!inBuffer) {
        return setParseError(pr_buffer_not_initialized);
    }
    if (!val) {
        return setParseError(pr_illegal_argument);
    }
    if (inBufferLength - currentInBufferPosition < len) {
        return setParseError(pr_out_of_buffer);
    }

    setParseError(pr_successful);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readUTF2(int first, OUT dx_int_t* res) {
    dx_byte_t second;
    if (!res) {
        setParseError(pr_illegal_argument);
        return R_FAILED;
    }

    if (readByte(&second) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if ((second & 0xC0) != 0x80) {
        setParseError(pr_bad_utf_data_format);
        return R_FAILED;
    }

    *res = (dx_char_t)(((first & 0x1F) << 6) | (second & 0x3F));
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readUTF3(int first, OUT dx_int_t* res) {
    dx_short_t tail;
    if (!res) {
        setParseError(pr_illegal_argument);
        return R_FAILED;
    }

    if (readShort(&tail) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if ((tail & 0xC0C0) != 0x8080) {
        setParseError(pr_bad_utf_data_format);
        return R_FAILED;
    }

    *res = (dx_char_t)(((first & 0x0F) << 12) | ((tail & 0x3F00) >> 2) | (tail & 0x3F));
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readUTF4(int first, OUT dx_int_t* res) {
    dx_byte_t second;
    dx_short_t tail;

    if (!res) {
        setParseError(pr_illegal_argument);
        return R_FAILED;
    }

    if (readByte(&second) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if (readShort(&tail) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if ((second & 0xC0) != 0x80 || (tail & 0xC0C0) != 0x8080) {
        setParseError(pr_bad_utf_data_format);
        return R_FAILED;
    }

    *res = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((tail & 0x3F00) >> 2) | (tail & 0x3F);

    if (*res > 0x10FFFF) {
        setParseError(pr_bad_utf_data_format);
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readUTFBody(int utflen, OUT dx_string_t* str ) {
    dx_string_t chars;
    dx_int_t count;
    dx_int_t tmpCh;

    if (!str) {
        setParseError(pr_illegal_argument);
        return R_FAILED;
    }

    if (utflen == 0) {
        *str = (dx_string_t)calloc(1, sizeof(dx_char_t));
        return parseSuccessful();
    };

    chars = (dx_string_t)malloc((utflen + 1) * sizeof(dx_char_t));
    count = 0;
    while (utflen > 0) {
        dx_byte_t c;
        if (readByte(&c) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        if (c >= 0) {
            utflen--;
            chars[count++] = (dx_char_t)c;
        } else if ((c & 0xE0) == 0xC0) {
            utflen -= 2;
            if (readUTF2(c, &tmpCh) != R_SUCCESSFUL) {
                return R_FAILED;
            }
            chars[count++] = tmpCh;
        } else if ((c & 0xF0) == 0xE0) {
            utflen -= 3;
            if (readUTF3(c, &tmpCh) != R_SUCCESSFUL) {
                return R_FAILED;
            }
            chars[count++] = tmpCh;
        } else if ((c & 0xF8) == 0xF0) {
            int tmpInt;
            dx_int_t res;
            utflen -= 4;
            if (readUTF4(c, &tmpInt) != R_SUCCESSFUL) {
                return R_FAILED;
            }

            if (toChars(tmpInt, count, utflen, &chars, &res) != R_SUCCESSFUL) {
                return R_FAILED;
            }
            count += res;
        } else{
            setParseError(pr_bad_utf_data_format);
            return R_FAILED;
        }
    }
    if (utflen < 0) {
        setParseError(pr_bad_utf_data_format);
        return R_FAILED;
    }

    chars[count] = 0; // end of string
    *str = chars;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
//void setInput(dx_byte_t* buf, dx_int_t offset, dx_int_t length) {
//    setInBuffer(buf);
//    setLimit(offset + length);
//    setPosition(offset);
//}
//
//////////////////////////////////////////////////////////////////////////////////
//void setLimit(dx_int_t newLimit) {
//    if (newLimit < 0 || newLimit > inBuffer.length)
//        throw new IllegalArgumentException();
//    inBufferLength = newLimit;
//    if (position > inBufferLength)
//        position = inBufferLength;
//}

////////////////////////////////////////////////////////////////////////////////
//DXResult needData( int length ) {
//    while (inBufferLength - currentInBufferPosition < length)
//        if (readData(length - (inBufferLength - currentInBufferPosition)) <= 0) {
//            dx_set_last_error(SS_DataSerializer, DS_OutOfBuffer);
//            return R_FAILED;
//        };
//    return R_SUCCESSFUL;
//}

////////////////////////////////////////////////////////////////////////////////
//dx_int_t read() {
//    if (currentInBufferPosition >= inBufferLength)
//        return -1;
//    return inBuffer[currentInBufferPosition++] & 0xFF;
//}
//
//////////////////////////////////////////////////////////////////////////////////
//dx_int_t read(dx_byte_t* b) {
//    return read(b, 0, b.length);
//}
//
//////////////////////////////////////////////////////////////////////////////////
//dx_int_t read(dx_byte_t* b, int off, int len) {
//    if ((off | len | (off + len) | (b.length - (off + len))) < 0)
//    {
//        dx_set_last_error(SS_DataSerializer, DS_IndexOutOfBounds);
//        return 
//    }
//        //throw new IndexOutOfBoundsException();
//    if (len == 0)
//        return 0;
//    if (currentInBufferPosition >= inBufferLength)
//        return -1;
//    len = Math.min(len, inBufferLength - currentInBufferPosition);
//    System.arraycopy(inBuffer, currentInBufferPosition, b, off, len);
//    currentInBufferPosition += len;
//    return len;
//}
//
dx_int_t skip(dx_int_t n) {
    dx_int_t remaining = n;
    while (remaining > 0) {
        dx_int_t skipping;
        if (currentInBufferPosition >= inBufferLength)
            break;
        skipping = MIN(remaining, inBufferLength - currentInBufferPosition);
        currentInBufferPosition += skipping;
        remaining -= skipping;
    }
    return n - remaining;
}
//
//public int available() {
//    return inBufferLength - currentInBufferPosition;
//}

enum dx_result_t readFully(dx_byte_t* b, dx_long_t bLength, dx_int_t off, dx_int_t len) {
    if ((off | len | (off + len) | (bLength - (off + len))) < 0) {
        return setParseError(pr_index_out_of_bounds);
    }
    if (inBufferLength - currentInBufferPosition < len){
        return setParseError(pr_out_of_buffer);
    }

    memcpy(b + off, inBuffer + currentInBufferPosition, len);
    currentInBufferPosition += len;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
// ========== external interface ==========
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readBoolean( OUT dx_bool_t* val ) {
    if (checkValid(val, 1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = inBuffer[currentInBufferPosition++];
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readByte( OUT dx_byte_t* val ) {
    if (checkValid(val, 1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = inBuffer[currentInBufferPosition++];
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readUnsignedByte( OUT dx_int_t* val ) {
    if (checkValid(val, 1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = (dx_int_t)inBuffer[currentInBufferPosition++] && 0xFF;
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readShort( OUT dx_short_t* val ) {
    if (checkValid(val, 2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((dx_short_t)inBuffer[currentInBufferPosition++] << 8) | ((dx_short_t)inBuffer[currentInBufferPosition++] & 0xFF);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readUnsignedShort( OUT dx_int_t* val ) {
    if (checkValid(val, 2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((dx_int_t)(inBuffer[currentInBufferPosition++] & 0xFF) << 8) | ((dx_int_t)inBuffer[currentInBufferPosition++] & 0xFF);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readChar( OUT dx_char_t* val ) {
    if (checkValid(val, 2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((dx_char_t)inBuffer[currentInBufferPosition++] << 8) | ((dx_char_t)inBuffer[currentInBufferPosition++] & 0xFF);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readInt( OUT dx_int_t* val ) {
    if (checkValid(val, 4) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((dx_int_t)inBuffer[currentInBufferPosition++] << 24) |
           ((dx_int_t)inBuffer[currentInBufferPosition++] << 16) |
           ((dx_int_t)inBuffer[currentInBufferPosition++] << 8)  |
           ((dx_int_t)inBuffer[currentInBufferPosition++] & 0xFF);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readLong( OUT dx_long_t* val ) {
    if (checkValid(val, 8) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((dx_long_t)inBuffer[currentInBufferPosition++] << 56) |
           ((dx_long_t)inBuffer[currentInBufferPosition++] << 48) |
           ((dx_long_t)inBuffer[currentInBufferPosition++] << 40) |
           ((dx_long_t)inBuffer[currentInBufferPosition++] << 32) |
           ((dx_long_t)inBuffer[currentInBufferPosition++] << 24) |
           ((dx_long_t)inBuffer[currentInBufferPosition++] << 16) |
           ((dx_long_t)inBuffer[currentInBufferPosition++] << 8)  |
           ((dx_long_t)inBuffer[currentInBufferPosition++] & 0xFF);

    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readFloat( OUT dx_float_t* val) {
    dx_int_t intVal;
    if ( readInt(&intVal) == R_FAILED ) {
        return R_FAILED;
    }

    *val = *((dx_float_t*)(&intVal));
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readDouble( OUT dx_double_t* val ) {
    dx_long_t longVal;
    if ( readLong(&longVal) == R_FAILED ) {
        return R_FAILED;
    }

    *val = *((dx_double_t*)(&longVal));
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readLine( OUT dx_string_t* val ) {
    const int tmpBufSize = 128;
    dx_string_t tmpBuffer;
    int count;
    if (checkValid(val, 1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    tmpBuffer = (dx_char_t*)malloc(tmpBufSize * sizeof(dx_char_t));
    count = 0;
    while (currentInBufferPosition < inBufferLength) {
        dx_char_t c = inBuffer[currentInBufferPosition++] & 0xFF;
        if (c == '\n')
            break;
        if (c == '\r') {
            if ((currentInBufferPosition < inBufferLength) && inBuffer[currentInBufferPosition] == '\n')
                currentInBufferPosition++;
            break;
        }

        if (count >= tmpBufSize) {
            dx_char_t* tmp = (dx_char_t*)malloc((tmpBufSize << 1) * sizeof(dx_char_t));
            memcpy(tmp, tmpBuffer, tmpBufSize * sizeof(dx_char_t));
            free(tmpBuffer);
            tmpBuffer = tmp;
        }
        tmpBuffer[count++] = c;
    }
    
    tmpBuffer[count] = 0;

    *val = tmpBuffer;
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readUTF( OUT dx_string_t* val ) {
    dx_int_t utflen;
    if (readUnsignedShort(&utflen) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    return readUTFBody(utflen, OUT val);
}

////////////////////////////////////////////////////////////////////////////////
//Object readObject() {
//    return IOUtil.readObject(this);
//}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readCompactInt( OUT dx_int_t* val ) {
    dx_int_t n;
    if (!val) {
        return setParseError(pr_illegal_argument);
    }
    // The ((n << k) >> k) expression performs two's complement.
    if (readUnsignedByte(&n) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (n < 0x80) {
        *val = (n << 25) >> 25;
        return parseSuccessful();
    }
    if (n < 0xC0) {
        dx_int_t m;
        if (readUnsignedByte(&m) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        *val = (((n << 8) | m) << 18) >> 18;
        return parseSuccessful();
    }
    if (n < 0xE0) {
        dx_int_t m;
        if (readUnsignedShort(&m) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        *val = (((n << 16) | m) << 11) >> 11;
        return parseSuccessful();
    }
    if (n < 0xF0) {
        dx_int_t tmpByte;
        dx_int_t tmpShort;

        if (readUnsignedByte(&tmpByte) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        if (readUnsignedShort(&tmpShort) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        *val = (((n << 24) | (tmpByte << 16) | tmpShort) << 4) >> 4;
        return parseSuccessful();
    }


    // The encoded number is possibly out of range, some bytes have to be skipped.
    // The skipBytes(...) does the strange thing, thus readUnsignedByte() is used.
    while (((n <<= 1) & 0x10) != 0) {
        skip(1);
    }

    if (readInt(&n) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readCompactLong( OUT dx_long_t* val ) {
    dx_int_t n;
    dx_int_t tmpInt;
    if (!val) {
        return setParseError(pr_illegal_argument);
    }

    // The ((n << k) >> k) expression performs two's complement.
    if (readUnsignedByte(&n) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (n < 0x80) {
        *val = (n << 25) >> 25;
        return parseSuccessful();
    }
    if (n < 0xC0) {
        dx_int_t tmpByte;
        if (readUnsignedByte(&tmpByte) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        *val = (((n << 8) | tmpByte) << 18) >> 18;
        return parseSuccessful();
    }
    if (n < 0xE0) {
        dx_int_t tmpShort;
        if (readUnsignedShort(&tmpShort) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        *val = (((n << 16) | tmpShort) << 11) >> 11;
        return parseSuccessful();
    }
    if (n < 0xF0) {
        dx_int_t tmpByte;
        dx_int_t tmpShort;
        if (readUnsignedByte(&tmpByte) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        if (readUnsignedShort(&tmpShort) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        *val = (((n << 24) | (tmpByte << 16) | tmpShort) << 4) >> 4;
        return parseSuccessful();
    }
    if (n < 0xF8) {
        n = (n << 29) >> 29;
    } else if (n < 0xFC) {
        dx_int_t tmpByte;
        if (readUnsignedByte(&tmpByte) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        n = (((n << 8) | tmpByte) << 22) >> 22;
    } else if (n < 0xFE) {
        dx_int_t tmpShort;
        if (readUnsignedShort(&tmpShort) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        n = (((n << 16) | tmpShort) << 15) >> 15;
    } else if (n < 0xFF) {
        dx_byte_t tmpByte;
        dx_int_t tmpShort;
        if (readByte(&tmpByte) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        if (readUnsignedShort(&tmpShort) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        n = (tmpByte << 16) | tmpShort;
    } else {
        if (readInt(&n) != R_SUCCESSFUL) {
            return R_FAILED;
        }
    }
    if (readInt(&tmpInt) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    *val = ((dx_long_t)n << 32) | (tmpInt & 0xFFFFFFFFL);
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readByteArray( OUT dx_byte_t** val ) {
    dx_byte_t* bytes;
    dx_long_t length;
    if (!val) {
        return setParseError(pr_illegal_argument);
    }
    if (readCompactLong(&length) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (length < -1 || length > INT_MAX) {
        return setParseError(pr_illegal_length);
    }
    if (length == -1) {
        *val = NULL;
        return parseSuccessful();
    }
    if (length == 0) {
        *val = (dx_byte_t*)dx_malloc(0);
        if (!(*val)) {
            return R_FAILED;
        }
        return parseSuccessful();
    }

    bytes = dx_malloc((size_t)length);
    if (readFully(bytes, length, 0, (dx_int_t)length) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    *val = bytes;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readUTFChar( OUT dx_int_t* val ) {
    dx_byte_t c;
    if (!val) {
        return setParseError(pr_illegal_argument);
    }
    if (readByte(&c) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (c >= 0) {
        *val = (dx_char_t)c;
        return parseSuccessful();
    }
    if ((c & 0xE0) == 0xC0) {
        return readUTF2(c, val);
    }
    if ((c & 0xF0) == 0xE0) {
        return readUTF3(c, val);
    }
    if ((c & 0xF8) == 0xF0) {
        return readUTF4(c, val);
    }

    return setParseError(pr_bad_utf_data_format);
}

////////////////////////////////////////////////////////////////////////////////
enum dx_result_t readUTFString( OUT dx_string_t* val ) {
    dx_long_t utflen;
    if (readCompactLong(&utflen) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (utflen < -1 || utflen > INT_MAX) {
        return setParseError(pr_illegal_length);
    }
    if (utflen == -1) {
        *val = NULL;
        return parseSuccessful();
    }
    return readUTFBody((dx_int_t)utflen, val);
}

