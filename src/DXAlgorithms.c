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

#ifdef _WIN32
#include <Windows.h>
#else
#include <time.h>
#include <stdlib.h>
#include <wctype.h>
#endif /* _WIN32 */

#include "DXAlgorithms.h"
#include "DXMemory.h"

#include <time.h>
#include <stdint.h>
#include <string.h>
#include <limits.h>

/* -------------------------------------------------------------------------- */
/*
 *	Bit operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_is_only_single_bit_set (int value) {
	return (value != 0 && (value & (value - 1)) == 0);
}

/* -------------------------------------------------------------------------- */
/*
 *	Random number generation functions implementation
 */
/* -------------------------------------------------------------------------- */

static dxf_ulong_t dx_seed;

static void dx_init_randomizer (void) {
	static bool is_randomizer_initialized = false;

	if (!is_randomizer_initialized) {
		is_randomizer_initialized = true;

		dx_seed = time(NULL);
		srand((unsigned int)dx_seed);
	}
}

/* -------------------------------------------------------------------------- */

int dx_random_integer (int max_value) {
	dx_init_randomizer();

	return (int)((long long)max_value * (unsigned int)rand() / RAND_MAX);
}

/* -------------------------------------------------------------------------- */

double dx_random_double (double max_value) {
	dx_init_randomizer();

	return max_value / RAND_MAX * rand();
}

/* -------------------------------------------------------------------------- */

size_t dx_random_size(size_t max_value) {
	dx_init_randomizer();
	//use 64bit xorshift64star
	dx_seed ^= dx_seed >> 12;
	dx_seed ^= dx_seed << 25;
	dx_seed ^= dx_seed >> 27;
	return (size_t)(max_value * ((double)(dx_seed * UINT64_C(2685821657736338717)) / ULLONG_MAX));
}

/* -------------------------------------------------------------------------- */
/*
 *	Array functions implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_capacity_manager_halfer (size_t new_size, size_t* capacity) {
	if (new_size > *capacity) {
		*capacity = (size_t)((double)*capacity * 1.5) + 1;

		return true;
	}

	if (new_size < *capacity && ((double)new_size / *capacity) < 0.5) {
		*capacity = new_size;

		return true;
	}

	return false;
}

/* -------------------------------------------------------------------------- */
/*
 *	String functions implementation
 */
/* -------------------------------------------------------------------------- */

dxf_string_t dx_create_string (size_t size) {
	return (dxf_string_t)dx_calloc(size + 1, sizeof(dxf_char_t));
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_create_string_src (dxf_const_string_t src) {
	return dx_create_string_src_len(src, dx_string_length(src));
}

/* -------------------------------------------------------------------------- */

char* dx_ansi_create_string(size_t size) {
	return (char*)dx_calloc(size + 1, sizeof(char));
}

/* -------------------------------------------------------------------------- */

char* dx_ansi_create_string_src(const char* src) {
	return dx_ansi_create_string_src_len(src, strlen(src));
}

/* -------------------------------------------------------------------------- */

char* dx_ansi_create_string_src_len(const char* src, size_t len) {
	char* res = NULL;
	if (len == 0) {
		return res;
	}

	res = dx_ansi_create_string(strlen(src));

	if (res == NULL) {
		return NULL;
	}

	return strncpy(res, src, len);
}

char* dx_ansi_copy_string_len(char* dest, const char* src, size_t len) {
	return strncpy(dest, src, len);
}

size_t dx_ansi_string_length(const char* str) {
	return strlen(str);
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_create_string_src_len(dxf_const_string_t src, size_t len) {
	dxf_string_t res = dx_create_string(len);

	if (res == NULL || len == 0) {
		return res;
	}

	return dx_copy_string_len(res, src, len);
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_copy_string (dxf_string_t dest, dxf_const_string_t src) {
	return wcscpy(dest, src);
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_copy_string_len (dxf_string_t dest, dxf_const_string_t src, size_t len) {
	return wcsncpy(dest, src, len);
}

/* -------------------------------------------------------------------------- */

size_t dx_string_length (dxf_const_string_t str) {
	return wcslen(str);
}

bool dx_string_null_or_empty(dxf_const_string_t str) {
	return str == NULL || dx_string_length(str) == 0;
}

/* -------------------------------------------------------------------------- */

int dx_compare_strings (dxf_const_string_t s1, dxf_const_string_t s2) {
	return wcscmp(s1, s2);
}

int dx_compare_strings_num(dxf_const_string_t s1, dxf_const_string_t s2, size_t num) {
	return wcsncmp(s1, s2, num);
}

/* -------------------------------------------------------------------------- */

dxf_char_t dx_toupper (dxf_char_t c) {
	return towupper(c);
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_ansi_to_unicode (const char* ansi_str) {
#ifdef _WIN32
	size_t len = strlen(ansi_str);
	dxf_string_t wide_str = NULL;

	// get required size
	int wide_size = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, 0);

	if (wide_size > 0) {
		wide_str = dx_create_string(wide_size);
		MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED | MB_ERR_INVALID_CHARS, ansi_str, (int)len, wide_str, wide_size);
	}

	return wide_str;
#else /* _WIN32 */
	mbstate_t state;
	dxf_string_t wide_str = NULL;
	// We trust it
	size_t len = strlen(ansi_str);

	memset(&state, 0, sizeof(state));
	mbrlen(NULL, 0, &state);

	const char *p = ansi_str;
	size_t wide_size = mbsrtowcs(NULL, &p, 0, &state);

	if (wide_size > 0) {
		wide_str = dx_create_string(wide_size);
		p = ansi_str;
		mbrlen(NULL, 0, &state);
		mbsrtowcs(wide_str, &p, wide_size, &state);
	}

	return wide_str;
#endif /* _WIN32 */
}

/* -------------------------------------------------------------------------- */

dxf_string_t dx_decode_from_integer (dxf_long_t code) {
	dxf_char_t decoded[8] = { 0 };
	int offset = 0;

	while (code != 0) {
		dxf_char_t c = (dxf_char_t)(code >> 56);

		if (c != 0) {
			decoded[offset++] = c;
		}

		code <<= 8;
	}

	return dx_create_string_src_len(decoded, offset);
}

dxf_string_t dx_concatenate_strings(dxf_string_t dest, dxf_const_string_t src) {
	return wcscat(dest, src);
}

/* -------------------------------------------------------------------------- */
/*
 *	Time functions implementation
 */
/* -------------------------------------------------------------------------- */

int dx_millisecond_timestamp (void) {

#ifdef _WIN32
	return (int)GetTickCount();
#else
	struct timespec ts;
	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (int)(ts.tv_sec * 1000 + ts.tv_sec / 1000000);
#endif
}

/* -------------------------------------------------------------------------- */

int dx_millisecond_timestamp_diff (int newer, int older) {
	long long res = 0;

	if ((unsigned)older > (unsigned)newer) {
		res += UINT_MAX;
	}

	return (int)(res + (unsigned)newer - (unsigned)older);
}

/* -------------------------------------------------------------------------- */

/**
 * Returns correct number of seconds with proper handling negative values and overflows.
 * Idea is that number of milliseconds shall be within [0..999]
 * as that the following equation always holds
 * dx_get_seconds_from_time(millis) * 1000L + dx_get_millis_from_time(millis) == millis
 */
dxf_int_t dx_get_seconds_from_time(dxf_long_t millis) {
	return millis >= 0 ? (dxf_int_t)MIN(millis / DX_TIME_SECOND, INT_MAX) :
		(dxf_int_t)MAX((millis + 1) / DX_TIME_SECOND - 1, INT_MIN);
}

/* -------------------------------------------------------------------------- */

/**
 * Returns correct number of milliseconds with proper handling negative values.
 * Idea is that number of milliseconds shall be within [0..999]
 * as that the following equation always holds
 * dx_get_seconds_from_time(millis) * 1000L + dx_get_millis_from_time(millis) == millis
 */
dxf_int_t dx_get_millis_from_time(dxf_long_t millis) {
	dxf_int_t r = (dxf_int_t)(millis % DX_TIME_SECOND);
	return r >= 0 ? r : r + (dxf_int_t)DX_TIME_SECOND;
}

/* -------------------------------------------------------------------------- */
/*
 *	Base64 encoding operations implementation
 */
/* -------------------------------------------------------------------------- */

#define WHITESPACE 64
#define EQUALS     65
#define INVALID    66

bool dx_base64_encode(const char* in, size_t in_len, char* out, size_t out_len) {
	const char base64chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
	const uint8_t *data = (const uint8_t *)in;
	size_t resultIndex = 0;
	size_t x;
	uint32_t n = 0;
	int padCount = in_len % 3;
	uint8_t n0, n1, n2, n3;

	/* increment over the length of the string, three characters at a time */
	for (x = 0; x < in_len; x += 3) {
		/* these three 8-bit (ASCII) characters become one 24-bit number */
		n = ((uint32_t)data[x]) << 16; //parenthesis needed, compiler depending on flags can do the shifting before conversion to uint32_t, resulting to 0

		if ((x + 1) < in_len)
			n += ((uint32_t)data[x + 1]) << 8;//parenthesis needed, compiler depending on flags can do the shifting before conversion to uint32_t, resulting to 0

		if ((x + 2) < in_len)
			n += data[x + 2];

		/* this 24-bit number gets separated into four 6-bit numbers */
		n0 = (uint8_t)(n >> 18) & 63;
		n1 = (uint8_t)(n >> 12) & 63;
		n2 = (uint8_t)(n >> 6) & 63;
		n3 = (uint8_t)n & 63;

		/*
		* if we have one byte available, then its encoding is spread
		* out over two characters
		*/
		if (resultIndex >= out_len)
			return false;   /* indicate failure: buffer too small */
		out[resultIndex++] = base64chars[n0];
		if (resultIndex >= out_len)
			return false;   /* indicate failure: buffer too small */
		out[resultIndex++] = base64chars[n1];

		/*
		* if we have only two bytes available, then their encoding is
		* spread out over three chars
		*/
		if ((x + 1) < in_len) {
			if (resultIndex >= out_len)
				return false;   /* indicate failure: buffer too small */
			out[resultIndex++] = base64chars[n2];
		}

		/*
		* if we have all three bytes available, then their encoding is spread
		* out over four characters
		*/
		if ((x + 2) < in_len) {
			if (resultIndex >= out_len)
				return false;   /* indicate failure: buffer too small */
			out[resultIndex++] = base64chars[n3];
		}
	}

	/*
	* create and add padding that is required if we did not have a multiple of 3
	* number of characters available
	*/
	if (padCount > 0) {
		for (; padCount < 3; padCount++) {
			if (resultIndex >= out_len)
				return false;   /* indicate failure: buffer too small */
			out[resultIndex++] = '=';
		}
	}
	if (out_len > resultIndex)
		out[resultIndex] = 0;
	return true;   /* indicate success */
}

static const unsigned char d[] = {
	66,66,66,66,66,66,66,66,66,66,64,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
	66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,62,66,66,66,63,52,53,
	54,55,56,57,58,59,60,61,66,66,66,65,66,66,66, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
	10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,66,66,66,66,66,66,26,27,28,
	29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,66,66,
	66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
	66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
	66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
	66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
	66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,66,
	66,66,66,66,66,66
};

bool dx_base64_decode(const char* in, size_t in_len, char* out, size_t* out_len) {
	const char *end = in + in_len;
	char iter = 0;
	uint32_t buf = 0;
	size_t len = 0;

	while (in < end) {
		unsigned char c = d[*in++];

		switch (c) {
			case WHITESPACE:
				continue;   /* skip whitespace */
			case INVALID:
				return false;   /* invalid input, return error */
			case EQUALS:                 /* pad character, end of data */
				in = end;
				continue;
			default:
				buf = buf << 6 | c;
				iter++; // increment the number of iteration
						/* If the buffer is full, split it into bytes */
				if (iter == 4) {
					if ((len += 3) > *out_len)
						return false; /* buffer overflow */
					*(out++) = (buf >> 16) & 255;
					*(out++) = (buf >> 8) & 255;
					*(out++) = buf & 255;
					buf = 0; iter = 0;

				}
		}
	}

	if (iter == 3) {
		if ((len += 2) > *out_len)
			return false; /* buffer overflow */
		*(out++) = (buf >> 10) & 255;
		*(out++) = (buf >> 2) & 255;
	} else if (iter == 2) {
		if (++len > *out_len)
			return false; /* buffer overflow */
		*(out++) = (buf >> 4) & 255;
	}

	*out_len = len; /* modify to reflect the actual output size */
	return true;
}

size_t dx_base64_length(size_t in_len) {
	return ((4 * in_len / 3) + 3) & ~3;
}

#ifdef _WIN32

__int64 atomic_read(__int64 volatile * value) {
	return InterlockedExchangeAdd64(value, 0);
}

void atomic_write(__int64 volatile * dest, __int64 src) {
	InterlockedExchange64(dest, src);
}

__int32 atomic_read32(__int32 volatile * value) {
	return InterlockedExchangeAdd(value, 0);
}

void atomic_write32(__int32 volatile * dest, __int32 src) {
	InterlockedExchange(dest, src);
}

time_t atomic_read_time(time_t volatile * value) {
    return sizeof(time_t) == sizeof(__time64_t) ?
        atomic_read((__int64 volatile *) value) : atomic_read32((__int32 volatile *)value);
}

void atomic_write_time(time_t volatile * dest, time_t src) {
    if (sizeof(time_t) == sizeof(__time64_t))
    {
        atomic_write((__int64 volatile *) dest, (__int64) src);
    }
    else
    {
        atomic_write32((__int32 volatile *)dest, (__int32)src);
    }
}

#else

long long atomic_read(long long* value) {
	return *value;
}

void atomic_write(long long* dest, long long src) {
	*dest = src;
}

long atomic_read32(long* value) {
	return *value;
}

void atomic_write32(long* dest, long src) {
	*dest = src;
}

time_t atomic_read_time(time_t* value) {
    return *value;
}

void atomic_write_time(time_t* dest, time_t src) {
    *dest = src;
}

#endif
