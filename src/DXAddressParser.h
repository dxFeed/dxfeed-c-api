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

#ifndef DX_ADDRESS_PARSER_H_INCLUDED
#define DX_ADDRESS_PARSER_H_INCLUDED

#include <stddef.h>
#include "PrimitiveTypes.h"

typedef struct {
	bool enabled;
	char* key_store;
	char* key_store_password;
	char* trust_store;
	char* trust_store_password;
} dx_codec_tls_t;

typedef struct {
	bool enabled;
} dx_codec_gzip_t;

typedef struct {
	char* host;
	const char* port;
	char* username;
	char* password;
	dx_codec_tls_t tls;
	dx_codec_gzip_t gzip;
} dx_address_t;

typedef struct {
	dx_address_t* elements;
	size_t size;
	size_t capacity;
} dx_address_array_t;

void dx_clear_address_array(dx_address_array_t* addresses);
bool dx_codec_gzip_copy(const dx_codec_gzip_t* src, OUT dx_codec_gzip_t* dest);
bool dx_codec_tls_copy(const dx_codec_tls_t* src, OUT dx_codec_tls_t* dest);
void dx_codec_tls_free(dx_codec_tls_t* tls);
bool dx_get_addresses_from_collection(const char* collection, OUT dx_address_array_t* addresses);

#endif /* DX_ADDRESS_PARSER_H_INCLUDED */