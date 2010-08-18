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

#include "SymbolCodec.h"
#include "ParserCommon.h"
#include "BufferedInput.h"

/**
* The PentaCodec performs symbol coding and serialization using
* extensible 5-bit encoding. The eligible characters are assigned penta codes
* (either single 5-bit or double 10-bit) according to the following table:
* 
* 'A' to 'Z'                 - 5-bit pentas from 1 to 26
* '.'                        - 5-bit penta 27
* '/'                        - 5-bit penta 28
* '$'                        - 5-bit penta 29
* ''' and '`'                - none (ineligible characters)
* ' ' to '~' except above    - 10-bit pentas from 960 to 1023
* all other                  - none (ineligible characters)
* 
* The 5-bit penta 0 represents empty space and is eligible only at the start.
* The 5-bit pentas 30 and 31 are used as a transition mark to switch to 10-bit pentas.
* The 10-bit pentas from 0 to 959 do not exist as they collide with 5-bit pentas.
* 
* The individual penta codes for character sequence are packed into 64-bit value
* from high bits to low bits aligned to the low bits. This allows representation
* of up to 35-bit penta-coded character sequences. If some symbol contains one or
* more ineligible characters or does not fit into 35-bit penta, then it is not
* subject to penta-coding and is left as a string. The resulting penta-coded value
* can be serialized as defined below or encoded into the 32-bit cipher if possible.
* Please note that penta code 0 is a valid code as it represents empty character
* sequence - do not confuse it wich cipher value 0, which means 'void' or 'null'.
* 
* The following table defines used serial format (the first byte is given in bits
* with 'x' representing payload bit; the remaining bytes are given in bit count):
* 
* 0xxxxxxx  8x - for 15-bit pentas
* 10xxxxxx 24x - for 30-bit pentas
* 110xxxxx ??? - reserved (payload TBD)
* 1110xxxx 16x - for 20-bit pentas
* 11110xxx 32x - for 35-bit pentas
* 111110xx ??? - reserved (payload TBD)
* 11111100 zzz - for UTF-8 string with length in bytes
* 11111101 zzz - for CESU-8 string with length in characters
* 11111110     - for 0-bit penta (empty symbol)
* 11111111     - for void (null)
* 
* See "http://www.unicode.org/unicode/reports/tr26/tr26-2.html"
* for format basics and writeUTFString and writeCharArray
* for details of string encoding.
*/

////////////////////////////////////////////////////////////////////////////////
// ========== Implementation Details ==========
////////////////////////////////////////////////////////////////////////////////

//#define PENTA_LENGTH 128
//
///**
//* Pentas for ASCII characters. Invalid pentas are set to 0.
//*/
//dx_int_t pentas[PENTA_LENGTH];
//
///**
//* Lengths (in bits) of pentas for ASCII characters. Invalid lengths are set to 64.
//*/
//dx_int_t plength[PENTA_LENGTH];
//
///**
//* ASCII characters for pentas. Invalid characters are set to 0.
//*/
//dx_char_t characters[1024];
//
//dx_int_t WILDCARD_CIPHER = encode("*"); // need initialize in init function
//
//
//////////////////////////////////////////////////////////////////////////////////
//void initPenta(dx_int_t c, dx_int_t _penta, dx_int_t _plen) {
//    pentas[c] = _penta;
//    plength[c] = _plen;
//    characters[_penta] = (dx_char_t)c;
//}
//
//////////////////////////////////////////////////////////////////////////////////
//dx_result_t initSymbolCodec() {
//    // initialization
//    dx_int_t i = PENTA_LENGTH;
//    for (; --i >= 0;) {
//        plength[i] = 64;
//    }
//    memset(pentas, 0, PENTA_LENGTH * sizeof(dx_int_t));
//    memset(characters, 0, 1024 * sizeof(dx_char_t));
//
//
//    for (i = (dx_int_t)(L'A'); i <= (dx_int_t)(L'Z'); ++i) {
//        initPenta(i, i - 'A' + 1, 5);
//    }
//
//    initPenta(dx_int_t(L'.'), 27, 5);
//    initPenta(dx_int_t(L'/'), 28, 5);
//    initPenta(dx_int_t(L'$'), 29, 5);
//    dx_int_t penta = 0x03C0;
//    for (dx_int_t i = 32; i <= 126; i++){
//        if (pentas[i] == 0 && i != (dx_int_t)(L'\'') && i != (dx_int_t)(L'`')){
//            initPenta(i, penta++, 10);
//        }
//    }
//
//    if (penta != 0x0400) {
//        return setParseError(pr_internal_error);
//    }
//}
//
//////////////////////////////////////////////////////////////////////////////////
///**
//* Encodes penta into cipher. Shall return 0 if encoding impossible.
//* The specified penta must be valid (no more than 35 bits).
//*/
//dx_int_t encodePenta(dx_long_t penta, dx_int_t plen) {
//    dx_int_t c;
//    if (plen <= 30)
//        return (dx_int_t)penta + 0x40000000;
//    c = (dx_int_t)((unsigned dx_long_t)penta >> 30);
//    if (c == pentas[L'/']) // Also checks that plen == 35 (high bits == 0).
//        return ((dx_int_t)penta & 0x3FFFFFFF) + 0x80000000;
//    if (c == pentas[L'$']) // Also checks that plen == 35 (high bits == 0).
//        return ((dx_int_t)penta & 0x3FFFFFFF) + 0xC0000000;
//    return 0;
//}
//
//////////////////////////////////////////////////////////////////////////////////
///**
//* Decodes cipher into penta code. The specified cipher must not be 0.
//* The returning penta code must be valid (no more than 35 bits).
//*/
//dx_result_t decodeCipher(dx_int_t cipher, OUT dx_long_t* c) {
//    switch ((unsigned dx_int_t)cipher >> 30) {
//        case 0:
//            return setParseError(pr_illegal_argument);
//        case 1:
//            *c = cipher & 0x3FFFFFFF;
//            break;
//        case 2:
//            *c = ((dx_long_t)pentas[L'/'] << 30) + (cipher & 0x3FFFFFFF);
//            break;
//        case 3:
//            *c = ((dx_long_t)pentas[L'$'] << 30) + (cipher & 0x3FFFFFFF);
//            break;
//        default:
//            return setParseError(pr_internal_error);
//    }
//
//    return parseSuccessful();
//}
//
//////////////////////////////////////////////////////////////////////////////////
///**
//* Converts penta into string.
//* The specified penta must be valid (no more than 35 bits).
//*/
//dx_string_t toString(dx_long_t penta) {
//    dx_int_t plen = 0;
//    dx_string_t chars;
//    dx_int_t length = 0;
//    while (((unsigned dx_long_t)penta >> plen) != 0) {
//        plen += 5;
//    }
//
//    chars = new dx_char_t[plen/5 + 1];
//    while (plen > 0) {
//        dx_int_t code;
//        plen -= 5;
//        code = (dx_int_t)((unsigned dx_long_t)penta >> plen) & 0x1F;
//        if (code >= 30 && plen > 0) {
//            plen -= 5;
//            code = (dx_int_t)((unsigned dx_long_t)penta >> plen) & 0x3FF;
//        }
//        chars[length++] = characters[code];
//    }
//
//    chars[length] = 0; // end of string
//    return chars;
//}
//
//////////////////////////////////////////////////////////////////////////////////
//dx_int_t getChartAt(dx_long_t penta, dx_int_t i) {
//    dx_int_t plen = 0;
//    while (((unsigned dx_long_t)penta >> plen) != 0) {
//        plen += 5;
//    }
//    while (i >= 0 && plen > 0) {
//        int code;
//        plen -= 5;
//        code = (dx_int_t)((unsigned dx_long_t)penta >> plen) & 0x1F;
//        if (code >= 30 && plen > 0) {
//            plen -= 5;
//            code = (dx_int_t)((unsigned dx_long_t)penta >> plen) & 0x3FF;
//        }
//        if (i == 0) {
//            return characters[code];
//        }
//        --i;
//    }
//    return -1;
//}
//
//////////////////////////////////////////////////////////////////////////////////
//// ========== SymbolCodec Implementation ==========
//
//////////////////////////////////////////////////////////////////////////////////
//dx_int_t encode(dx_string_t symbol) {
//    dx_long_t penta = 0;
//    dx_int_t plen = 0;
//    dx_int_t i;
//    if (symbol == null) {
//        return 0;
//    }
//    dx_int_t length = wcslen(symbol);
//    if (length > 7) {
//        return 0;
//    }
//    for (i = 0; i < length; ++i) {
//        dx_int_t c = symbol[i];
//        if (c >= 128)
//            return 0;
//        dx_int_t l = pentas[c];
//        penta = (penta << l) + pentas[c];
//        plen += l;
//    }
//    if (plen > 35)
//        return 0;
//    return encodePenta(penta, plen);
//}
//
//////////////////////////////////////////////////////////////////////////////////
//dx_result_t readSymbol(char[] buffer, String[] result) throws IOException {
//    dx_int_t i = readUnsignedByte();
//    long penta;
//    if (i < 0x80) { // 15-bit
//        penta = (i << 8) + in.readUnsignedByte();
//    } else if (i < 0xC0) { // 30-bit
//        penta = ((i & 0x3F) << 24) + (in.readUnsignedByte() << 16) + in.readUnsignedShort();
//    } else if (i < 0xE0) { // reserved (first diapason)
//        throw new IOException("Reserved bit sequence.");
//    } else if (i < 0xF0) { // 20-bit
//        penta = ((i & 0x0F) << 16) + in.readUnsignedShort();
//    } else if (i < 0xF8) { // 35-bit
//        penta = ((long)(i & 0x07) << 32) + (in.readInt() & 0xFFFFFFFFL);
//    } else if (i < 0xFC) { // reserved (second diapason)
//        throw new IOException("Reserved bit sequence.");
//    } else if (i == 0xFC) { // UTF-8
//        // todo This is inefficient, but currently UTF-8 is not used (compatibility mode). Shall be rewritten when UTF-8 become active.
//        result[0] = in.readUTFString();
//        return 0;
//    } else if (i == 0xFD) { // CESU-8
//        long length = in.readCompactLong();
//        if (length < -1 || length > Integer.MAX_VALUE)
//            throw new IOException("Illegal length.");
//        if (length == -1) {
//            result[0] = null;
//            return 0;
//        } else if (length == 0) {
//            result[0] = "";
//            return 0;
//        }
//        char[] chars = length <= buffer.length ? buffer : new char[(int)length];
//        for (int k = 0; k < length; k++) {
//            int codePoint = in.readUTFChar();
//            if (codePoint > 0xFFFF)
//                throw new UTFDataFormatException("Code point is beyond BMP.");
//            chars[k] = (char)codePoint;
//        }
//        if (length <= buffer.length && (length & VALID_CIPHER) == 0)
//            return (int)length;
//        result[0] = new String(chars, 0, (int)length);
//        return 0;
//    } else if (i == 0xFE) { // 0-bit
//        penta = 0;
//    } else { // void (null)
//        result[0] = null;
//        return 0;
//    }
//    int plen = 0;
//    while ((penta >>> plen) != 0)
//        plen += 5;
//    int cipher = encodePenta(penta, plen);
//    if (cipher == 0)
//        result[0] = toString(penta); // Generally this is inefficient, but this use-case shall not actually happen.
//    return cipher;
//}
//
