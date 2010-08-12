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
#include "ErrorReport.h"
#include "DXMemory.h"

// pointer to extern inBuffer
jByte* inBuffer = 0;
jInt   inBufferLength = 0;
jInt   currentInBufferPosition = 0;

////////////////////////////////////////////////////////////////////////////////
void setBuffer( jByte* buf ) {
    inBuffer = buf;
}

////////////////////////////////////////////////////////////////////////////////
// ========== Implementation Details ==========
////////////////////////////////////////////////////////////////////////////////

//jChar* charBuffer; // Used for reading strings; lazy initialized.
//
//jChar* getCharBuffer(jInt capacity) {
//    if (capacity > 256)
//        return (jChar*)malloc(sizeof(jChar) * capacity);
//    return charBuffer != null ? charBuffer : (charBuffer = new char[256]);
//}

////////////////////////////////////////////////////////////////////////////////
enum DXResult checkValid(void* val, jInt len) {
    if (!inBuffer) {
        return setParseError(DS_BufferNotInitialized);
    }
    if (!val) {
        return setParseError(DS_IllegalArgument);
    }
    if (inBufferLength - currentInBufferPosition < len) {
        return setParseError(DS_OutOfBuffer);
    }

    setParseError(DS_Successful);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readUTF2(int first, OUT jInt* res) {
    jByte second;
    if (!res) {
        setParseError(DS_IllegalArgument);
        return R_FAILED;
    }

    if (readByte(&second) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if ((second & 0xC0) != 0x80) {
        setParseError(DS_BadUTFDataFormat);
        return R_FAILED;
    }

    *res = (jChar)(((first & 0x1F) << 6) | (second & 0x3F));
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readUTF3(int first, OUT jInt* res) {
    jShort tail;
    if (!res) {
        setParseError(DS_IllegalArgument);
        return R_FAILED;
    }

    if (readShort(&tail) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if ((tail & 0xC0C0) != 0x8080) {
        setParseError(DS_BadUTFDataFormat);
        return R_FAILED;
    }

    *res = (jChar)(((first & 0x0F) << 12) | ((tail & 0x3F00) >> 2) | (tail & 0x3F));
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readUTF4(int first, OUT jInt* res) {
    jByte second;
    jShort tail;

    if (!res) {
        setParseError(DS_IllegalArgument);
        return R_FAILED;
    }

    if (readByte(&second) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if (readShort(&tail) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if ((second & 0xC0) != 0x80 || (tail & 0xC0C0) != 0x8080) {
        setParseError(DS_BadUTFDataFormat);
        return R_FAILED;
    }

    *res = ((first & 0x07) << 18) | ((second & 0x3F) << 12) | ((tail & 0x3F00) >> 2) | (tail & 0x3F);

    if (*res > 0x10FFFF) {
        setParseError(DS_BadUTFDataFormat);
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readUTFBody(int utflen, OUT dx_string* str ) {
    dx_string chars;
    jInt count;
    jInt tmpCh;

    if (!str) {
        setParseError(DS_IllegalArgument);
        return R_FAILED;
    }

    if (utflen == 0) {
        *str = (dx_string)calloc(1, sizeof(jChar));
        return parseSuccessful();
    };

    chars = (dx_string)malloc((utflen + 1) * sizeof(jChar));
    count = 0;
    while (utflen > 0) {
        jByte c;
        if (readByte(&c) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        if (c >= 0) {
            utflen--;
            chars[count++] = (jChar)c;
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
            jInt res;
            utflen -= 4;
            if (readUTF4(c, &tmpInt) != R_SUCCESSFUL) {
                return R_FAILED;
            }

            if (toChars(tmpInt, count, utflen, &chars, &res) != R_SUCCESSFUL) {
                return R_FAILED;
            }
            count += res;
        } else{
            setParseError(DS_BadUTFDataFormat);
            return R_FAILED;
        }
    }
    if (utflen < 0) {
        setParseError(DS_BadUTFDataFormat);
        return R_FAILED;
    }

    chars[count] = 0; // end of string
    *str = chars;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
//void setInput(jByte* buf, jInt offset, jInt length) {
//    setBuffer(buf);
//    setLimit(offset + length);
//    setPosition(offset);
//}
//
//////////////////////////////////////////////////////////////////////////////////
//void setLimit(jInt newLimit) {
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
//jInt read() {
//    if (currentInBufferPosition >= inBufferLength)
//        return -1;
//    return inBuffer[currentInBufferPosition++] & 0xFF;
//}
//
//////////////////////////////////////////////////////////////////////////////////
//jInt read(jByte* b) {
//    return read(b, 0, b.length);
//}
//
//////////////////////////////////////////////////////////////////////////////////
//jInt read(jByte* b, int off, int len) {
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
jInt skip(jInt n) {
    jInt remaining = n;
    while (remaining > 0) {
        jInt skipping;
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

enum DXResult readFully(jByte* b, jLong bLength, jInt off, jInt len) {
    if ((off | len | (off + len) | (bLength - (off + len))) < 0) {
        return setParseError(DS_IndexOutOfBounds);
    }
    if (inBufferLength - currentInBufferPosition < len){
        return setParseError(DS_OutOfBuffer);
    }

    memcpy(b + off, inBuffer + currentInBufferPosition, len);
    currentInBufferPosition += len;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
// ========== external interface ==========
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
enum DXResult readBoolean( OUT jBool* val ) {
    if (checkValid(val, 1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = inBuffer[currentInBufferPosition++];
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readByte( OUT jByte* val ) {
    if (checkValid(val, 1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = inBuffer[currentInBufferPosition++];
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readUnsignedByte( OUT jInt* val ) {
    if (checkValid(val, 1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = (jInt)inBuffer[currentInBufferPosition++] && 0xFF;
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readShort( OUT jShort* val ) {
    if (checkValid(val, 2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((jShort)inBuffer[currentInBufferPosition++] << 8) | ((jShort)inBuffer[currentInBufferPosition++] & 0xFF);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readUnsignedShort( OUT jInt* val ) {
    if (checkValid(val, 2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((jInt)(inBuffer[currentInBufferPosition++] & 0xFF) << 8) | ((jInt)inBuffer[currentInBufferPosition++] & 0xFF);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readChar( OUT jChar* val ) {
    if (checkValid(val, 2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((jChar)inBuffer[currentInBufferPosition++] << 8) | ((jChar)inBuffer[currentInBufferPosition++] & 0xFF);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readInt( OUT jInt* val ) {
    if (checkValid(val, 4) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((jInt)inBuffer[currentInBufferPosition++] << 24) |
           ((jInt)inBuffer[currentInBufferPosition++] << 16) |
           ((jInt)inBuffer[currentInBufferPosition++] << 8)  |
           ((jInt)inBuffer[currentInBufferPosition++] & 0xFF);
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readLong( OUT jLong* val ) {
    if (checkValid(val, 8) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    *val = ((jLong)inBuffer[currentInBufferPosition++] << 56) |
           ((jLong)inBuffer[currentInBufferPosition++] << 48) |
           ((jLong)inBuffer[currentInBufferPosition++] << 40) |
           ((jLong)inBuffer[currentInBufferPosition++] << 32) |
           ((jLong)inBuffer[currentInBufferPosition++] << 24) |
           ((jLong)inBuffer[currentInBufferPosition++] << 16) |
           ((jLong)inBuffer[currentInBufferPosition++] << 8)  |
           ((jLong)inBuffer[currentInBufferPosition++] & 0xFF);

    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readFloat( OUT jFloat* val) {
    jInt intVal;
    if ( readInt(&intVal) == R_FAILED ) {
        return R_FAILED;
    }

    *val = *((jFloat*)(&intVal));
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readDouble( OUT jDouble* val ) {
    jLong longVal;
    if ( readLong(&longVal) == R_FAILED ) {
        return R_FAILED;
    }

    *val = *((jDouble*)(&longVal));
    return R_SUCCESSFUL;
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readLine( OUT dx_string* val ) {
    const int tmpBufSize = 128;
    dx_string tmpBuffer;
    int count;
    if (checkValid(val, 1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    tmpBuffer = (jChar*)malloc(tmpBufSize * sizeof(jChar));
    count = 0;
    while (currentInBufferPosition < inBufferLength) {
        jChar c = inBuffer[currentInBufferPosition++] & 0xFF;
        if (c == '\n')
            break;
        if (c == '\r') {
            if ((currentInBufferPosition < inBufferLength) && inBuffer[currentInBufferPosition] == '\n')
                currentInBufferPosition++;
            break;
        }

        if (count >= tmpBufSize) {
            jChar* tmp = (jChar*)malloc((tmpBufSize << 1) * sizeof(jChar));
            memcpy(tmp, tmpBuffer, tmpBufSize * sizeof(jChar));
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
enum DXResult readUTF( OUT dx_string* val ) {
    jInt utflen;
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
enum DXResult readCompactInt( OUT jInt* val ) {
    jInt n;
    if (!val) {
        return setParseError(DS_IllegalArgument);
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
        jInt m;
        if (readUnsignedByte(&m) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        *val = (((n << 8) | m) << 18) >> 18;
        return parseSuccessful();
    }
    if (n < 0xE0) {
        jInt m;
        if (readUnsignedShort(&m) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        *val = (((n << 16) | m) << 11) >> 11;
        return parseSuccessful();
    }
    if (n < 0xF0) {
        jInt tmpByte;
        jInt tmpShort;

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
enum DXResult readCompactLong( OUT jLong* val ) {
    jInt n;
    jInt tmpInt;
    if (!val) {
        return setParseError(DS_IllegalArgument);
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
        jInt tmpByte;
        if (readUnsignedByte(&tmpByte) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        *val = (((n << 8) | tmpByte) << 18) >> 18;
        return parseSuccessful();
    }
    if (n < 0xE0) {
        jInt tmpShort;
        if (readUnsignedShort(&tmpShort) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        *val = (((n << 16) | tmpShort) << 11) >> 11;
        return parseSuccessful();
    }
    if (n < 0xF0) {
        jInt tmpByte;
        jInt tmpShort;
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
        jInt tmpByte;
        if (readUnsignedByte(&tmpByte) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        n = (((n << 8) | tmpByte) << 22) >> 22;
    } else if (n < 0xFE) {
        jInt tmpShort;
        if (readUnsignedShort(&tmpShort) != R_SUCCESSFUL) {
            return R_FAILED;
        }
        n = (((n << 16) | tmpShort) << 15) >> 15;
    } else if (n < 0xFF) {
        jByte tmpByte;
        jInt tmpShort;
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
    *val = ((jLong)n << 32) | (tmpInt & 0xFFFFFFFFL);
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readByteArray( OUT jByte** val ) {
    jByte* bytes;
    jLong length;
    if (!val) {
        return setParseError(DS_IllegalArgument);
    }
    if (readCompactLong(&length) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (length < -1 || length > INT_MAX) {
        return setParseError(DS_IllegalLength);
    }
    if (length == -1) {
        *val = NULL;
        return parseSuccessful();
    }
    if (length == 0) {
        *val = (jByte*)dx_malloc(0);
        if (!(*val)) {
            return R_FAILED;
        }
        return parseSuccessful();
    }

    bytes = dx_malloc((size_t)length);
    if (readFully(bytes, length, 0, (jInt)length) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    *val = bytes;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readUTFChar( OUT jInt* val ) {
    jByte c;
    if (!val) {
        return setParseError(DS_IllegalArgument);
    }
    if (readByte(&c) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (c >= 0) {
        *val = (jChar)c;
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

    return setParseError(DS_BadUTFDataFormat);
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult readUTFString( OUT dx_string* val ) {
    jLong utflen;
    if (readCompactLong(&utflen) != R_SUCCESSFUL) {
        return R_FAILED;
    }
    if (utflen < -1 || utflen > INT_MAX) {
        return setParseError(DS_IllegalLength);
    }
    if (utflen == -1) {
        *val = NULL;
        return parseSuccessful();
    }
    return readUTFBody((jInt)utflen, val);
}

