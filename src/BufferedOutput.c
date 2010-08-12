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
#include "ErrorReport.h"
#include "DXMemory.h"


// pointer to extern buffer
jByte* outBuffer = 0;
jInt   outBufferLength = 0;
jInt   currentOutBufferPosition = 0;

////////////////////////////////////////////////////////////////////////////////
void setOutBuffer( jByte* buf ) {
    outBuffer = buf;
}

////////////////////////////////////////////////////////////////////////////////
// ========== Implementation Details ==========
////////////////////////////////////////////////////////////////////////////////

enum DXResult checkWrite(int requiredCapacity) {
    if ( !outBuffer ) {
        return setParseError(DS_BufferNotInitialized);
    }

    if ( outBufferLength - currentOutBufferPosition < requiredCapacity && ensureCapacity(requiredCapacity) != R_SUCCESSFUL ) {
        return R_FAILED;
    }

    return parseSuccessful();
}

void writeUTF2Unchecked(jInt codePoint) {
    outBuffer[currentOutBufferPosition++] = (jByte)(0xC0 | codePoint >> 6);
    outBuffer[currentOutBufferPosition++] = (jByte)(0x80 | codePoint & 0x3F);
}

////////////////////////////////////////////////////////////////////////////////
void writeUTF3Unchecked(jInt codePoint) {
    outBuffer[currentOutBufferPosition++] = (jByte)(0xE0 | codePoint >> 12);
    outBuffer[currentOutBufferPosition++] = (jByte)(0x80 | codePoint >> 6 & 0x3F);
    outBuffer[currentOutBufferPosition++] = (jByte)(0x80 | codePoint & 0x3F);
}

////////////////////////////////////////////////////////////////////////////////
void writeUTF4Unchecked(jInt codePoint) {
    outBuffer[currentOutBufferPosition++] = (jByte)(0xF0 | codePoint >> 18);
    outBuffer[currentOutBufferPosition++] = (jByte)(0x80 | codePoint >> 12 & 0x3F);
    outBuffer[currentOutBufferPosition++] = (jByte)(0x80 | codePoint >> 6 & 0x3F);
    outBuffer[currentOutBufferPosition++] = (jByte)(0x80 | codePoint & 0x3F);
}

////////////////////////////////////////////////////////////////////////////////
void writeIntUnchecked(jInt val) {
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 24);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 16);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (jByte)val;
}

////////////////////////////////////////////////////////////////////////////////
// ========== external interface ==========
////////////////////////////////////////////////////////////////////////////////

////////////////////////////////////////////////////////////////////////////////
//enum DXResult write(jInt b) {
//    enum DXResult res;
//    if (currentOutBufferPosition >= outBufferLength && ((res = ensureCapacity(1)) != DS_Successful) ) {
//        return res;
//    }
//
//    buffer[currentOutBufferPosition++] = (jByte)b;
//    return parseSuccessful();
//}

////////////////////////////////////////////////////////////////////////////////
enum DXResult write(const jByte* b, jInt bLen, jInt off, jInt len) {
    if ((off | len | (off + len) | (bLen - (off + len))) < 0) {
        return setParseError(DS_IndexOutOfBounds);
    }

    if (outBufferLength - currentOutBufferPosition < len && (ensureCapacity(len) != R_SUCCESSFUL) ) {
        return R_FAILED;
    }

    memcpy(outBuffer + currentOutBufferPosition, b + off, len);
    currentOutBufferPosition += len;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeBoolean( OUT jBool val ) {
    if (checkWrite(1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = val ? (jByte)1 : (jByte)0;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeByte( jByte val ) {
    if (checkWrite(1) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (jByte)val;
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeShort( jShort val ) {
    if (checkWrite(2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (jByte)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeChar( jChar val ) {
    if (checkWrite(2) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (jByte)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeInt( jInt val ) {
    if (checkWrite(4) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 24);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 16);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (jByte)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeLong( jLong val ) {
    if (checkWrite(8) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 56);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 48);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 40);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 32);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 24);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 16);
    outBuffer[currentOutBufferPosition++] = (jByte)(val >> 8);
    outBuffer[currentOutBufferPosition++] = (jByte)val;

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeFloat( jFloat val ) {
    return writeInt(*((jInt*)&val));
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeDouble( jDouble val) {
    return writeLong(*((jLong*)&val));
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeBytes( const jChar* val ) {
    enum DXResult res = R_SUCCESSFUL;
    size_t length = wcslen(val);
    size_t i = 0;
    for (;i < length && res == R_SUCCESSFUL; i++) {
        res = writeByte((jByte)val[i]);
    }

    if (res == R_FAILED) {
        return R_FAILED;
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeChars( const jChar* val ) {
    enum DXResult res = R_SUCCESSFUL;
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
enum DXResult writeUTF( const dx_string val ) {
    size_t strlen = wcslen(val);
    jShort utflen = 0;
    size_t i = 0;
    for (; i < strlen; i++) {
        jChar c = val[i];
        if (c > 0 && c <= 0x007F)
            utflen++;
        else if (c <= 0x07FF)
            utflen += 2;
        else
            utflen += 3;
    }

    if ((size_t)utflen < strlen || utflen > 65535) {
        return setParseError(DS_BadUTFDataFormat);
    }

    if (writeShort(utflen) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    for (i = 0; i < strlen; i++) {
        jChar c;
        if (outBufferLength - currentOutBufferPosition < 3 && ensureCapacity(3) != R_SUCCESSFUL) {
            return R_FAILED;
        }

        c = val[i];
        if (c > 0 && c <= 0x007F)
            outBuffer[currentOutBufferPosition++] = (jByte)c;
        else if (c <= 0x07FF)
            writeUTF2Unchecked(c);
        else
            writeUTF3Unchecked(c);
    }

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeCompactInt(jInt val) {
    if (checkWrite(5) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if (val >= 0) {
        if (val < 0x40) {
            outBuffer[currentOutBufferPosition++] = (jByte)val;
        } else if (val < 0x2000) {
            outBuffer[currentOutBufferPosition++] = (jByte)(0x80 | val >> 8);
            outBuffer[currentOutBufferPosition++] = (jByte)val;
        } else if (val < 0x100000) {
            outBuffer[currentOutBufferPosition++] = (jByte)(0xC0 | val >> 16);
            outBuffer[currentOutBufferPosition++] = (jByte)(val >> 8);
            outBuffer[currentOutBufferPosition++] = (jByte)val;
        } else if (val < 0x08000000) {
            writeIntUnchecked(0xE0000000 | val);
        } else {
            outBuffer[currentOutBufferPosition++] = (jByte)0xF0;
            writeIntUnchecked(val);
        }
    } else {
        if (val >= -0x40) {
            outBuffer[currentOutBufferPosition++] = (jByte)(0x7F & val);
        } else if (val >= -0x2000) {
            outBuffer[currentOutBufferPosition++] = (jByte)(0xBF & val >> 8);
            outBuffer[currentOutBufferPosition++] = (jByte)val;
        } else if (val >= -0x100000) {
            outBuffer[currentOutBufferPosition++] = (jByte)(0xDF & val >> 16);
            outBuffer[currentOutBufferPosition++] = (jByte)(val >> 8);
            outBuffer[currentOutBufferPosition++] = (jByte)val;
        } else if (val >= -0x08000000) {
            writeIntUnchecked(0xEFFFFFFF & val);
        } else {
            outBuffer[currentOutBufferPosition++] = (jByte)0xF7;
            writeIntUnchecked(val);
        }
    }
    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeCompactLong(jLong v) {
    jInt hi;
    if (v == (jLong)(jInt)v) {
        return writeCompactInt((jInt)v);
    }

    if (checkWrite(9) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    hi = (jInt)(v >> 32);
    if (hi >= 0) {
        if (hi < 0x04) {
            outBuffer[currentOutBufferPosition++] = (jByte)(0xF0 | hi);
        } else if (hi < 0x0200) {
            outBuffer[currentOutBufferPosition++] = (jByte)(0xF8 | hi >> 8);
            outBuffer[currentOutBufferPosition++] = (jByte)hi;
        } else if (hi < 0x010000) {
            outBuffer[currentOutBufferPosition++] = (jByte)0xFC;
            outBuffer[currentOutBufferPosition++] = (jByte)(hi >> 8);
            outBuffer[currentOutBufferPosition++] = (jByte)hi;
        } else if (hi < 0x800000) {
            writeIntUnchecked(0xFE000000 | hi);
        } else {
            outBuffer[currentOutBufferPosition++] = (jByte)0xFF;
            writeIntUnchecked(hi);
        }
    } else {
        if (hi >= -0x04) {
            outBuffer[currentOutBufferPosition++] = (jByte)(0xF7 & hi);
        } else if (hi >= -0x0200) {
            outBuffer[currentOutBufferPosition++] = (jByte)(0xFB & hi >> 8);
            outBuffer[currentOutBufferPosition++] = (jByte)hi;
        } else if (hi >= -0x010000) {
            outBuffer[currentOutBufferPosition++] = (jByte)0xFD;
            outBuffer[currentOutBufferPosition++] = (jByte)(hi >> 8);
            outBuffer[currentOutBufferPosition++] = (jByte)hi;
        } else if (hi >= -0x800000) {
            writeIntUnchecked(0xFEFFFFFF & hi);
        } else {
            outBuffer[currentOutBufferPosition++] = (jByte)0xFF;
            writeIntUnchecked(hi);
        }
    }
    writeIntUnchecked((int)v);

    return parseSuccessful();
}

////////////////////////////////////////////////////////////////////////////////
enum DXResult writeByteArray(const jByte* bytes, jInt size) {
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
enum DXResult writeUTFChar(jInt codePoint) {
    if (checkWrite(4) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    if (codePoint < 0) {
        return setParseError(DS_BadUTFDataFormat);
    }

    if (codePoint <= 0x007F) {
        outBuffer[currentOutBufferPosition++] = (jByte)codePoint;
    } else if (codePoint <= 0x07FF) {
        writeUTF2Unchecked(codePoint);
    } else if (codePoint <= 0xFFFF) {
        writeUTF3Unchecked(codePoint);
    } else if (codePoint <= 0x10FFFF) {
        writeUTF4Unchecked(codePoint);
    } else {
        return setParseError(DS_BadUTFDataFormat);
    }

    return parseSuccessful();
}

enum DXResult writeUTFString(const dx_string str) {
    size_t strlen;
    size_t utflen;
    size_t i;

    if (str == NULL) {
        return writeCompactInt(-1);
    }

    strlen = wcslen(str);
    utflen = 0;
    for (i = 0; i < strlen;) {
        jChar c = str[i++];
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
        return setParseError(DS_BadUTFDataFormat);
    }

    if (writeCompactInt((jInt)utflen) != R_SUCCESSFUL) {
        return R_FAILED;
    }

    for (i = 0; i < strlen;) {
        jChar c;
        if (outBufferLength - currentOutBufferPosition < 4 && (ensureCapacity(4) != R_SUCCESSFUL)) {
            return R_FAILED;
        }
        c = str[i++];
        if (c <= 0x007F)
            outBuffer[currentOutBufferPosition++] = (jByte)c;
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
enum DXResult ensureCapacity( int requiredCapacity ) {
    if (INT_MAX - currentOutBufferPosition < outBufferLength) {
        return setParseError(DS_BufferOverFlow);
    }

    if (requiredCapacity > outBufferLength) {
        jInt length = MAX(MAX((jInt)MIN((jLong)outBufferLength << 1, (jLong)INT_MAX), 1024), requiredCapacity);
        jByte* newBuffer = (jByte*)malloc(length);
        memcpy(newBuffer, outBuffer, outBufferLength);
        free(outBuffer);
        outBuffer = newBuffer;
        outBufferLength = length;
    }
    return parseSuccessful();
}

