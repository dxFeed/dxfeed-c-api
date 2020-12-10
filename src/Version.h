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

#ifndef VERSION_H
#define VERSION_H

#define DX_LSTRINGIFY(x) L ## x
#define DX_TO_LSTRING(x) DX_LSTRINGIFY(x)

#define DX_STRINGIFY(x) #x
#define DX_TO_STRING(x) DX_STRINGIFY(x)

#define DX_VERSION_MAJOR 6
#define DX_VERSION_MINOR 0
#define DX_VERSION_PATCH 1

#define DX_VER_FILE_VERSION         DX_VERSION_MAJOR, DX_VERSION_MINOR, DX_VERSION_PATCH
#define DX_VER_FILE_VERSION_STR     DX_TO_STRING(DX_VERSION_MAJOR.DX_VERSION_MINOR.DX_VERSION_PATCH)
#define DX_VER_FILE_VERSION_LSTR    DX_TO_LSTRING(DX_VER_FILE_VERSION_STR)
#define DX_VER_PRODUCT_VERSION      DX_VER_FILE_VERSION
#define DX_VER_PRODUCT_VERSION_STR  DX_VER_FILE_VERSION_STR
#define DX_VER_PRODUCT_VERSION_LSTR DX_TO_LSTRING(DX_VER_PRODUCT_VERSION_STR)

#define DX_VER_FILENAME "DXFeed_64"

#define DX_VER_ORIGINAL_FILENAME DX_VER_FILENAME ".dll"
#define DX_VER_INTERNAL_FILENAME DX_VER_ORIGINAL_FILENAME 
#define DX_VER_COMPANY_NAME      "Devexperts LLC"
#define DX_VER_FILE_DESCRIPTION  "Dynamic-link library that provides a common application programming interface (API) to real-time, delayed and historical market data feeds."
#define DX_VER_LEGAL_COPYRIGHT   "Copyright (C) 2010-2020 Devexperts LLC"
#define DX_VER_PRODUCT_NAME      "DXFeed C API"

#define DX_LIBRARY_VERSION L"DXFeed.cpp v 6.0.1 (c) Devexperts LLC"

extern dxf_const_string_t DX_LIBRARY_OPTIONS;

#endif // VERSION_H
