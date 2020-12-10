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
#include "DXTypes.h"
#include "Version.h"

#ifdef DXFEED_CODEC_TLS_ENABLED
#	define DX_LIB_OPT_TLS L"With"
#else
#	define DX_LIB_OPT_TLS L"Without"
#endif

dxf_const_string_t DX_LIBRARY_OPTIONS = DX_LIB_OPT_TLS L" TLS";
