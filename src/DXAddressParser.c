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

#include <stdio.h>
#include <string.h>

#include "DXAddressParser.h"
#include "DXAlgorithms.h"
#include "DXErrorCodes.h"
#include "DXErrorHandling.h"
#include "Logger.h"

#ifdef ADDRESS_CODEC_TLS_ENABLED
#define DX_CODEC_TLS_STATUS true
#else
#define DX_CODEC_TLS_STATUS false
#endif

#ifdef ADDRESS_CODEC_GZIP_ENABLED
#define DX_CODEC_GZIP_STATUS true
#else
#define DX_CODEC_GZIP_STATUS false
#endif

typedef bool(*dx_codec_parser_t)(const char* codec, size_t size, dx_address_t* addr);

typedef struct {
    const char* name;
    bool supported;
    dx_codec_parser_t parser;
} dx_codec_info_t;

typedef struct {
    char* key;
    char* value;
} dx_address_property_t;

//function forward declaration
bool dx_codec_tls_parser(const char* codec, size_t size, OUT dx_address_t* addr);
//function forward declaration
bool dx_codec_gzip_parser(const char* codec, size_t size, OUT dx_address_t* addr);

static const dx_codec_info_t codecs[] = {
    { "tls", DX_CODEC_TLS_STATUS, dx_codec_tls_parser },
    { "gzip", DX_CODEC_GZIP_STATUS, dx_codec_gzip_parser }
};

static const char null_symbol = 0;
static const int port_min = 0;
static const int port_max = 65535;
static const char host_port_splitter = ':';
static const char entry_begin_symbol = '(';
static const char entry_end_symbol = ')';
static const char whitespace = ' ';
static const char codec_splitter = '+';
static const char properties_begin_symbol = '[';
static const char properties_end_symbol = ']';
static const char properties_splitter = ',';
static const char property_value_splitter = '=';

/* -------------------------------------------------------------------------- */

static bool dx_is_empty_entry(const char* entry_begin, const char* entry_end) {
    if (entry_begin > entry_end)
        return true;
    while (*entry_begin == whitespace) {
        if (entry_begin == entry_end)
            return true;
        entry_begin++;
    }
    return false;
}

/* -------------------------------------------------------------------------- */

static char* dx_find_first(const char* from, const char* to, int ch) {
    char* pos = strchr(from, ch);
    if (pos > to)
        return NULL;
    return pos;
}

/* -------------------------------------------------------------------------- */

static char* dx_find_last(const char* from, const char* to, int ch) {
    char* substr = dx_ansi_create_string_src_len(from, to - from);
    char* pos = NULL;
    size_t offset;
    if (substr == NULL)
        return pos;
    pos = strrchr(substr, ch);
    if (pos == NULL) {
        dx_free(substr);
        return pos;
    }
    offset = pos - substr;
    dx_free(substr);
    return (char*)(from + offset);
}

/* -------------------------------------------------------------------------- */

static bool dx_has_next(const char* next) {
    return next != NULL && *next != null_symbol;
}

/* -------------------------------------------------------------------------- */

static bool dx_get_next_entry(OUT char** next, OUT char** entry, OUT size_t* size) {
    char* begin = NULL;
    char* end = NULL;
    char* collection = NULL;
    if (entry == NULL || size == NULL || next == NULL)
        return dx_set_error_code(dx_ec_invalid_func_param_internal);

    *entry = NULL;
    *size = 0;
    collection = *next;
    if (*collection == null_symbol)
        return true;
    while (*entry == NULL && *collection != null_symbol) {
        begin = strchr(collection, entry_begin_symbol);
        end = strchr(collection, entry_end_symbol);

        if (begin == NULL && end == NULL) {
            if (!dx_is_empty_entry(collection, collection + strlen(collection))) {
                *entry = collection;
                *size = strlen(collection);
            }
            *next += *size;
            return true;
        } else if (begin != NULL && end != NULL) {
            collection = begin + 1;
            if (dx_is_empty_entry(collection, end)) {
                collection = end + 1;
                continue;
            }
            *entry = collection;
            *size = (end - collection) / sizeof(char);
            *next = end + 1;
            return true;
        } else {
            return dx_set_error_code(dx_ec_invalid_func_param);
        }
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_get_next_codec(OUT char** next, OUT char** codec, OUT size_t* size) {
    char* end = NULL;
    char* collection = NULL;
    if (codec == NULL || size == NULL || next == NULL)
        return dx_set_error_code(dx_ec_invalid_func_param_internal);

    *codec = NULL;
    *size = 0;
    collection = *next;
    while (*codec == NULL && *collection != null_symbol) {
        end = strchr(collection, codec_splitter);
        if (end == NULL)
            return true;
        if (dx_is_empty_entry(collection, end)) {
            collection = end + 1;
            continue;
        }
        *codec = collection;
        *size = (end - collection) / sizeof(char);
        *next = end + 1;
        return true;
    }

    return true;
}

/* -------------------------------------------------------------------------- */


//TODO: fix warnings

static bool dx_get_codec_name(const char* codec, size_t codec_size, OUT char** name, OUT size_t* name_size) {
    char* end = NULL;
    if (codec == NULL || name == NULL || name_size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }

    *name = NULL;
    *name_size = 0;
    if (codec_size == 0)
        return true;

    end = strchr(codec, properties_begin_symbol);
    *name = codec;
    if (end == NULL || end >= codec + codec_size) {
        *name_size = codec_size;
    } else {
        *name_size = (end - codec) / sizeof(char);
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_get_codec_properties(const char* codec, size_t codec_size, OUT char** props, OUT size_t* props_size) {
    char* name;
    size_t name_size;
    char* props_begin;
    char* props_end;
    if (codec == NULL || props == NULL || props_size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    *props = NULL;
    *props_size = 0;
    if (!dx_get_codec_name(codec, codec_size, &name, &name_size))
        return false;
    if (codec_size == name_size)
        return true;
    //check proprties starts with '[' and finishes with ']' symbol
    props_begin = codec + name_size;
    props_end = codec + codec_size;
    if (*props_begin != properties_begin_symbol || *props_end != properties_end_symbol)
        return dx_set_error_code(dx_ec_invalid_func_param);
    if (!dx_is_empty_entry(props_begin, props_end)) {
        *props = props_begin;
        *props_size = props_end - props_begin;
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_get_next_property(OUT char** next, OUT size_t* next_size, OUT char** prop, OUT size_t* prop_size) {
    if (next == NULL || next_size == NULL || prop == NULL || prop_size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    *prop = NULL;
    *prop_size = 0;
    while (**next != null_symbol && next_size > 0) {
        //points to special symbol ',' or '[' that indicates begining of next property entry
        char* collection_begin = *next;
        //points to special symbol ']' that indicates ending of all properties
        char* collection_end = collection_begin + *next_size;
        //points to useful data of current property
        char* begin = collection_begin;
        //points to to special symbol ']' that indicates ending of current property
        char* end;
        //skip first ',' or '[' symbol
        if (*begin == properties_begin_symbol || *begin == properties_splitter)
            begin++;
        end = MIN(
            dx_find_first(begin, collection_end, properties_end_symbol),
            dx_find_first(begin, collection_end, properties_splitter));
        if (begin == NULL || end == NULL) {
            return dx_set_error_code(dx_ec_invalid_func_param);
        } else {
            *next = (*end == properties_end_symbol) ? end + 1 : end;
            *next_size -= *next - collection_begin;
            if (dx_is_empty_entry((const char*)begin, (const char*)end)) {
                continue;
            }
            *prop = begin;
            *prop_size = end - begin - 1;
        }
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_get_host_port_string(const char* entry, size_t entry_size, OUT char** address, OUT size_t* size) {
    char* begin;
    char* end;
    char* entry_end = entry + entry_size;
    if (entry == NULL || address == NULL || size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    begin = dx_find_last(entry, entry_end, codec_splitter);
    if (begin == NULL) {
        begin = entry;
    } else {
        begin++;
    }
    end = dx_find_first(begin, entry_end, properties_begin_symbol);
    *size = (end == NULL) ? entry_end - begin : end - begin - 1;
    *address = begin;
    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_get_properties(const char* entry, size_t entry_size, OUT char** props, OUT size_t* props_size) {
    char* begin;
    char* end;
    char* address;
    size_t address_size;
    if (entry == NULL || *props == NULL || props_size == NULL) {
        return dx_set_error_code(dx_ec_invalid_func_param_internal);
    }
    if (!dx_get_host_port_string(entry, entry_size, &address, &address_size))
        return false;
    begin = address + address_size;
    end = entry + entry_size;
    if (*begin != properties_begin_symbol || *end != properties_end_symbol)
        return dx_set_error_code(dx_ec_invalid_func_param);
    if (!dx_is_empty_entry(begin, end)) {
        *props = begin;
        *props_size = end - begin;
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static void dx_free_property(OUT dx_address_property_t* prop) {
    if (prop == NULL)
        return;
    if (prop->key != NULL)
        dx_free(prop->key);
    if (prop->value != NULL)
        dx_free(prop->value);
    prop->key = NULL;
    prop->value = NULL;
}

/* -------------------------------------------------------------------------- */

//Note: free allocated memory for prop!
static bool dx_parse_property(const char* str, size_t size, OUT dx_address_property_t* prop) {
    char* splitter = dx_find_first(str, str + size, property_value_splitter);
    size_t key_size;
    size_t value_size;
    if (splitter == NULL || dx_is_empty_entry(str, splitter - 1) || dx_is_empty_entry(splitter + 1, str + size)) {
        dx_logging_info(L"Invalid property from: ...%s", str);
        return dx_set_error_code(dx_nec_invalid_function_arg);
    }
    key_size = splitter - str;
    value_size = size - key_size - 1;
    dx_free_property(prop);
    prop->key = dx_ansi_create_string_src_len(str, key_size);
    prop->value = dx_ansi_create_string_src_len(splitter + 1, value_size);
    if (prop->key == NULL || prop->value == NULL) {
        dx_free_property(prop);
        return false;
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static void dx_codec_tls_free(OUT dx_codec_tls_t* tls) {
    if (tls->key_store != NULL)
        dx_free(tls->key_store);
    if (tls->key_store_password != NULL)
        dx_free(tls->key_store_password);
    if (tls->trust_store != NULL)
        dx_free(tls->trust_store);
    if (tls->trust_store_password != NULL)
        dx_free(tls->trust_store_password);
    tls->key_store = NULL;
    tls->key_store_password = NULL;
    tls->trust_store = NULL;
    tls->trust_store_password = NULL;
}

/* -------------------------------------------------------------------------- */

static bool dx_codec_tls_parser(const char* codec, size_t size, OUT dx_address_t* addr) {
    char* next;
    size_t next_size;
    dx_codec_tls_t tls = { true, NULL, NULL, NULL, NULL };
    if (!dx_get_codec_properties(codec, size, &next, &next_size))
        return false;
    do {
        char* str;
        size_t str_size;
        dx_address_property_t prop;
        if (!dx_get_next_property(&next, &next_size, &str, &str_size)) {
            dx_codec_tls_free(&tls);
            return false;
        }
        if (str == NULL)
            continue;
        if (!dx_parse_property((const char*)str, str_size, &prop)) {
            dx_codec_tls_free(&tls);
            return false;
        }

        if (strcmp(prop.key, "keyStore") == 0) {
            DX_SWAP(char*, tls.key_store, prop.value);
        } else if (strcmp(prop.key, "keyStorePassword") == 0) {
            DX_SWAP(char*, tls.key_store_password, prop.value);
        } else if (strcmp(prop.key, "trustStore") == 0) {
            DX_SWAP(char*, tls.trust_store, prop.value);
        } else if (strcmp(prop.key, "trustStorePassword") == 0) {
            DX_SWAP(char*, tls.trust_store_password, prop.value);
        } else {
            dx_logging_info(L"Unknown property for TLS: %s=%s", prop.key, prop.value);
            dx_free_property(&prop);
            dx_codec_tls_free(&tls);
            return dx_set_error_code(dx_nec_invalid_function_arg);
        }
        dx_free_property(&prop);

    } while (dx_has_next(next));

    addr->tls = tls;
    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_codec_gzip_parser(const char* codec, size_t size, OUT dx_address_t* addr) {
    addr->gzip.enabled = true;
    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_codec_parse(const char* codec, size_t codec_size, OUT dx_address_t* addr) {
    char* codec_name;
    size_t codec_name_size;
    size_t count = sizeof(codecs) / sizeof(codecs[0]);
    size_t i;
    if (!dx_get_codec_name(codec, codec_size, OUT &codec_name, OUT &codec_name_size))
        return false;
    for (i = 0; i < count; ++i) {
        dx_codec_info_t info = codecs[i];
        if (strlen(info.name) != codec_name_size || stricmp(info.name, codec_name) != 0)
            continue;
        if (!info.supported)
            return dx_set_error_code(dx_nec_unknown_codec);
        if (!info.parser(codec, codec_size, addr))
            return false;
        break;
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static bool dx_parse_host_port(const char* host, size_t size, OUT dx_address_t* addr) {
    char* port_start = NULL;

    addr->host = dx_ansi_create_string_src_len(host, size);
    addr->port = NULL;

    if (addr->host == NULL) {
        return false;
    }

    port_start = strrchr(host, (int)host_port_splitter);

    if (port_start != NULL) {
        int port;

        if (sscanf(port_start + 1, "%d", &port) == 1) {
            if (port < port_min || port > port_max) {
                dx_free((void*)addr->host);

                return dx_set_error_code(dx_nec_invalid_port_value);
            }

            addr->port = addr->host + (port_start - host);

            /* separating host from port with zero symbol and advancing to the first port symbol */
            *(char*)(addr->port++) = 0;
        }
    }

    return true;
}

/* -------------------------------------------------------------------------- */

static void dx_free_address(OUT dx_address_t* addr) {
    if (addr == NULL)
        return;

    /* note that port buffer is not freed, because it's in fact just a pointer to a position
    within the host buffer */
    if (addr->host != NULL)
        dx_free(addr->host);
    if (addr->username != NULL)
        dx_free(addr->username);
    if (addr->password != NULL)
        dx_free(addr->password);
    dx_codec_tls_free(&addr->tls);
    memset((void*)addr, 0, sizeof(dx_address_t));
}

/* -------------------------------------------------------------------------- */

//TODO: free address everywhere...
static bool dx_parse_address(const char* entry, size_t entry_size, OUT dx_address_t* addr) {
    char* next = entry;
    size_t next_size;
    char* address;
    size_t address_size;
    dx_free_address(addr);
    //get codecs
    do {
        char* codec;
        size_t codec_size;
        if (!dx_get_next_codec(OUT &next, OUT &codec, OUT &codec_size)) {
            dx_free_address(addr);
            return false;
        }
        if (codec == NULL)
            continue;
        if (!dx_codec_parse(codec, codec_size, addr)) {
            dx_free_address(addr);
            return false;
        }
    } while (dx_has_next(next));

    //get host and port
    if (!dx_get_host_port_string(entry, entry_size, &address, &address_size) ||
        !dx_parse_host_port(address, address_size, addr)) {
        dx_free_address(addr);
        return false;
    }

    //get properties
    if (!dx_get_properties(entry, entry_size, OUT &next, OUT &next_size)) {
        dx_free_address(addr);
        return false;
    }
    do {
        char* str;
        size_t str_size;
        dx_address_property_t prop;
        if (!dx_get_next_property(OUT &next, OUT &next_size, OUT &str, OUT &str_size)) {
            dx_free_address(addr);
            return false;
        }
        if (str == NULL)
            continue;
        if (!dx_parse_property((const char*)str, str_size, &prop)) {
            dx_free_address(addr);
            return false;
        }
        if (strcmp(prop.key, "username") == 0) {
            DX_SWAP(char*, addr->username, prop.value);
        } else if (strcmp(prop.key, "password") == 0) {
            DX_SWAP(char*, addr->password, prop.value);
        } else {
            dx_logging_info(L"Unknown property: %s=%s", prop.key, prop.value);
            dx_free_property(&prop);
            dx_free_address(addr);
            return dx_set_error_code(dx_nec_invalid_function_arg);
        }
        dx_free_property(&prop);
    } while (dx_has_next(next));

    return true;
}

/* -------------------------------------------------------------------------- */

void dx_clear_address_array(dx_address_array_t* addresses) {
    size_t i = 0;

    if (addresses->elements == NULL) {
        return;
    }

    for (; i < addresses->size; ++i) {
        dx_free_address(&(addresses->elements[i]));
    }

    dx_free(addresses->elements);
}

/* -------------------------------------------------------------------------- */

bool dx_get_addresses_from_collection(const char* collection, OUT dx_address_array_t* addresses) {
    char* collection_copied = NULL;
    char* cur_address = NULL;
    char* splitter_pos = NULL;
    const char* last_port = NULL;
    size_t i = 0;

    char* next = (char*)collection;

    if (collection == NULL || addresses == NULL) {
        dx_set_last_error(dx_ec_invalid_func_param_internal);

        return false;
    }

    cur_address = collection_copied = dx_ansi_create_string_src(collection);

    if (collection_copied == NULL) {
        return false;
    }

    do {
        dx_address_t addr;
        bool failed;

        char* entry;
        size_t entry_size;

        if (!dx_get_next_entry(&next, &entry, &entry_size) ||
            !dx_parse_address(entry, entry_size, OUT &addr)) {

            dx_clear_address_array(addresses);
            dx_free(collection_copied);
            return false;
        }

        DX_ARRAY_INSERT(*addresses, dx_address_t, addr, addresses->size, dx_capacity_manager_halfer, failed);

        if (failed) {
            dx_clear_address_array(addresses);
            dx_free(collection_copied);
            dx_free_address(&addr);

            return false;
        }

        cur_address = splitter_pos;
    } while (next != null_symbol);

    dx_free(collection_copied);

    if ((last_port = addresses->elements[addresses->size - 1].port) == NULL) {
        dx_clear_address_array(addresses);

        return false;
    }

    for (; i < addresses->size; ++i) {
        if (addresses->elements[i].port == NULL) {
            addresses->elements[i].port = last_port;
        }
    }

    return true;
}
