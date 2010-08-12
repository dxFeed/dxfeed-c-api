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

#ifndef PRIMITIVE_TYPES_H
#define PRIMITIVE_TYPES_H

#include "wtypes.h"

////////////////////////////////////////////////////////////////////////////////
// WARNING! The types below must have the sizes specified
// Use appropriated types with the same size on your platform if necessary
typedef unsigned char jBool;   // 8 bit
typedef char          jByte;   // 8 bit
typedef wchar_t       jChar;   // 16 bit
typedef short int     jShort;  // 16 bit
typedef int           jInt;    // 32 bit
typedef float         jFloat;  // 32 bit
typedef long long     jLong;   // 64 bit
typedef double        jDouble; // 64 bit

typedef jChar*        jstring;


#endif // PRIMITIVE_TYPES_H