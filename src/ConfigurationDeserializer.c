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

#include <stdint.h>
#include <string.h>
#include "ConfigurationDeserializer.h"
#include "DXAlgorithms.h"
#include "DXErrorHandling.h"

typedef int dx_byte_array_pos_t;

static const dxf_short_t STREAM_MAGIC = 0xACED;
static const dxf_short_t STREAM_VERSION = 5;
#define TC_NULL         0x70
#define TC_STRING       0x74
#define TC_LONGSTRING   0x7C

#define READ_MULTIMBYTE_VALUE_BODY(multibyte_type, alias) \
static bool read_##alias(dxf_byte_array_t* object, dx_byte_array_pos_t *pos, \
						OUT multibyte_type *value) { \
	dx_byte_array_pos_t last = *pos + sizeof(dxf_short_t); \
	if (last >= object->size) \
		return false; \
	*value = 0; \
	for (; *pos < last; (*pos)++) { \
		*value = ((*value) << 8) | (dxf_ubyte_t)object->elements[*pos]; \
	} \
	return true; \
}

READ_MULTIMBYTE_VALUE_BODY(dxf_short_t, short)
READ_MULTIMBYTE_VALUE_BODY(dxf_long_t, long)

static bool read_byte(dxf_byte_array_t* object, dx_byte_array_pos_t *pos,
					OUT dxf_byte_t *value) {
	dx_byte_array_pos_t last = *pos + sizeof(dxf_byte_t);
	if (last >= object->size)
		return false;
	*value = object->elements[(*pos)++];
	return true;
}

static bool read_string(dxf_byte_array_t* object, dx_byte_array_pos_t *pos,
						size_t length, OUT dxf_string_t *value) {
	char *buf = NULL;
	buf = dx_calloc(length + 1, sizeof(char));
	if (buf == NULL)
		return false;
	strncpy(buf, (void*)&(object->elements[*pos]), length);
	*value = dx_ansi_to_unicode(buf);
	dx_free(buf);

	return true;
}

bool dx_configuration_deserialize_string(dxf_byte_array_t* object,
										OUT dxf_string_t* string) {
	dx_byte_array_pos_t pos = 0;
	dxf_short_t short_value;
	dxf_byte_t serialize_type;
	dxf_long_t long_value;
	if (!read_short(object, &pos, &short_value)
		|| (short_value != STREAM_MAGIC)) {

		return dx_set_error_code(dx_csdec_protocol_error);
	}
	if (!read_short(object, &pos, &short_value)
		|| (short_value != STREAM_VERSION)) {

		return dx_set_error_code(dx_csdec_unsupported_version);
	}
	if (!read_byte(object, &pos, &serialize_type))
		return dx_set_error_code(dx_csdec_protocol_error);
	switch (serialize_type)
	{
	case TC_STRING:
		if (!read_short(object, &pos, &short_value)
			|| !read_string(object, &pos, short_value, string)) {
			return dx_set_error_code(dx_csdec_protocol_error);
		}
		break;
	case TC_LONGSTRING:
		if (!read_long(object, &pos, &long_value))
			return dx_set_error_code(dx_csdec_protocol_error);
		if (long_value >= SIZE_MAX)
			return dx_set_error_code(dx_ec_invalid_func_param_internal);
		if (!read_string(object, &pos, (size_t)long_value, string))
			return false;
		break;
	case TC_NULL:
		/* go through */
	default:
		/* In case of TC_NULL or unsupported types just exit and remain
		OUT string empty */
		break;
	}

	return true;
}