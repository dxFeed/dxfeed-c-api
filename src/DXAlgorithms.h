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

/*
 *	Contains various algorithms which may be used across the project
 */

#ifndef DX_ALGORITHMS_H_INCLUDED
#define DX_ALGORITHMS_H_INCLUDED

#include <time.h>
#include "PrimitiveTypes.h"
#include "DXMemory.h"

/* -------------------------------------------------------------------------- */
/*
 *	Generic values
 */
/* -------------------------------------------------------------------------- */

#define INVALID_INDEX   (-1)

/* -------------------------------------------------------------------------- */
/*
 *	Generic operations
 */
/* -------------------------------------------------------------------------- */

#define DX_SWAP(type, e1, e2) \
	do { \
		type _tmp = e1; \
		\
		e1 = e2; \
		e2 = _tmp; \
	} while (false)

#define DX_CHECKED_SET_VAL_TO_PTR(ptr, val) \
	do { \
		if (ptr != NULL) { \
			*ptr = val; \
		} \
	} while (false)

#define MAX(a,b) (((a)>(b))?(a):(b))
#define MIN(a,b) (((a)<(b))?(a):(b))

#define CHECKED_CALL(func, param) \
	do { \
		if (!func(param)) { \
			return false; \
		} \
	} while (false)

#define CHECKED_CALL_0(func) \
	do { \
		if (!func()) { \
			return false; \
		} \
	} while (false)

#define CHECKED_CALL_1(func, param1) \
	do { \
		if (!func(param1)) { \
			return false; \
		} \
	} while (false)

#define CHECKED_CALL_2(func, param1, param2) \
	do { \
		if (!func(param1, param2)) { \
			return false; \
		} \
	} while (false)

#define CHECKED_CALL_3(func, param1, param2, param3) \
	do { \
		if (!func(param1, param2, param3)) { \
			return false; \
		} \
	} while (false)

#define CHECKED_CALL_4(func, param1, param2, param3, param4) \
	do { \
		if (!func(param1, param2, param3, param4)) { \
			return false; \
		} \
	} while (false)

#define CHECKED_CALL_5(func, param1, param2, param3, param4, param5) \
	do { \
		if (!func(param1, param2, param3, param4, param5)) { \
			return false; \
		} \
	} while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Memory operations
 */
/* -------------------------------------------------------------------------- */

#define CHECKED_FREE(ptr) \
	do { \
		if (ptr != NULL) { \
			dx_free((void*)(ptr)); \
		} \
	} while (false)

#define FREE_ARRAY(array_ptr, size) \
	do { \
		size_t _array_ind = 0; \
		\
		if (array_ptr == NULL) { \
			break; \
		} \
		\
		for (; _array_ind < size; ++_array_ind) { \
			if (array_ptr[_array_ind] != NULL) { \
				dx_free((void*)array_ptr[_array_ind]); \
			} \
		} \
		\
		dx_free((void*)array_ptr); \
	} while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Bit operations
 */
/* -------------------------------------------------------------------------- */

#define INDEX_BITMASK(index) \
	(1 << (index))

#define IS_FLAG_SET(flags, flag) \
	((flags & flag) != 0)

#define UNSIGNED_TYPE_dxf_int_t \
	dxf_uint_t

#define UNSIGNED_TYPE_dxf_long_t \
	dxf_ulong_t

#define UNSIGNED_TYPE(type) \
	UNSIGNED_TYPE_##type

#define UNSIGNED_RIGHT_SHIFT(value, offset, type) \
	((type)((UNSIGNED_TYPE(type))value >> offset))

bool dx_is_only_single_bit_set (int value);

/* -------------------------------------------------------------------------- */
/*
 *	Random number generation functions
 */
/* -------------------------------------------------------------------------- */

int dx_random_integer (int max_value);
double dx_random_double (double max_value);
size_t dx_random_size(size_t max_value);

/* -------------------------------------------------------------------------- */
/*
 *	Array constats
 */
/* -------------------------------------------------------------------------- */

/*
 *  Sets the empty array value.
 *  Returns the array descriptor structure. this structure must contain at
	least three fields:
		elements - a pointer to the array buffer (of type 'elem_type*')
		size - an integer value containing the number of elements in the buffer
		capacity - the current buffer capacity, may be greater than size
 */
#define DX_EMPTY_ARRAY { NULL, 0, 0 }

/* -------------------------------------------------------------------------- */
/*
 *	Array operations and functions

 *  A set of operations implementing various array algorithms: search,
 *  insertion, deletion etc
 */
/* -------------------------------------------------------------------------- */

/*
 *	Binary search

 *  a - array object (most likely of type 'type*' or 'type[]')
 *  start_index - the index of the first element which can contain the key
 *  end_index - the index of the last element which can contain the key increased by 1
 *  elem - the key we're looking for
 *  comparator - a binary ordering predicate:
		comparator(a, b) > 0 => a > b
		comparator(a, b) = 0 => a = b
		comparator(a, b) < 0 => a < b
 *  found - a boolean return value, if it's non-zero then the key was found in the array
 *  res_index - an integer return value, contains the index of an element containing the key,
				or the index of an element where the key might be inserted without breaking
				the ordering, if the element wasn't found
 */
#define DX_ARRAY_BINARY_SEARCH(a, start_index, end_index, elem, comparator, found, res_index) \
	do { \
		size_t _begin = start_index; \
		size_t _end = end_index; \
		size_t _mid; \
		int _comp_res; \
		\
		found = false; \
		\
		while (_begin < _end && !found) { \
			_mid = (_begin + _end) / 2; \
			_comp_res = comparator((a)[_mid], (elem)); \
			\
			if (_comp_res == 0) { \
				res_index = _mid; \
				found = true; \
			} else if (_comp_res < 0) { \
				_begin = _mid + 1; \
			} else { \
				_end = _mid; \
			} \
		} \
		\
		if (!found) { \
			res_index = _end; \
		} \
	} while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Search

 *  a - array object (most likely of type 'type*' or 'type[]')
 *  start_index - the index of the first element which can contain the key
 *  end_index - the index of the last element which can contain the key increased by 1
 *  elem - the key we're looking for
 *  comparator - a binary ordering predicate:
		comparator(a, b) > 0 => a > b
		comparator(a, b) = 0 => a = b
		comparator(a, b) < 0 => a < b
 *  is_binary - a boolean value: if it's non-zero then the the array is sorted by the order
				set by comparator
 *  found - a boolean return value, if it's non-zero then the key was found in the array
 *  res_index - an integer return value, contains the index of an element containing the key,
				or the index of an element where the key might be inserted without breaking
				the ordering, if the element wasn't found
 */
#define DX_ARRAY_SEARCH(a, start_index, end_index, elem, comparator, is_binary, found, res_index) \
	do { \
		found = false; \
		\
		if (is_binary) { \
			DX_ARRAY_BINARY_SEARCH(a, start_index, end_index, elem, comparator, found, res_index); \
		} else { \
			size_t _index = start_index; \
			\
			for (; _index < end_index; ++_index) { \
				if (comparator((a)[_index], (elem)) == 0) { \
					found = true; \
					\
					break; \
				} \
			} \
			\
			res_index = _index; \
		} \
	} while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Insertion

 *  array_obj - an array descriptor structure. this structure must contain
				at least three fields:
		elements - a pointer to the array buffer (of type 'elem_type*')
		size - an integer value containing the number of elements in the buffer
		capacity - the current buffer capacity, may be greater than size
 *  elem_type - type of elements in the buffer
 *  new_elem - an element to insert
 *  position - an index of a position where the new element is to be inserted,
			is determined by a previous call of DX_ARRAY_SEARCH
 *  capacity_mgr - a function with two parameters:
		new_size - an size_t parameter passed by value, contains the desired
				new size of the array
		capacity - an size_t parameter passed by pointer, contains the current
				array capacity

		Return value:
			true - reallocation is required, in this case the new capacity
				value is stored in the capacity parameter
			false - no reallocation is required
 *  error - a boolean return value, is true if some error occurred during insertion,
			is false otherwise
 */
#define DX_ARRAY_INSERT(array_obj, elem_type, new_elem, position, capacity_mgr, error) \
	do { \
		void* _buffer_to_free = NULL; \
		elem_type* _new_elem_buffer = (array_obj).elements; \
		\
		error = false; \
		\
		if (capacity_mgr((array_obj).size + 1, &((array_obj).capacity))) { \
			_new_elem_buffer = (elem_type*)dx_calloc((array_obj).capacity, sizeof(elem_type)); \
			\
			if (_new_elem_buffer == NULL) { \
				error = true; \
				\
				break; \
			} \
			\
			dx_memcpy((void*)_new_elem_buffer, (const void*)(array_obj).elements, position * sizeof(elem_type)); \
			\
			_buffer_to_free = (void*)(array_obj).elements; \
		} \
		\
		if (_buffer_to_free == NULL) { \
			dx_memmove((void*)(_new_elem_buffer + position + 1), (const void*)((elem_type*)(array_obj).elements + position), \
					((array_obj).size - position) * sizeof(elem_type)); \
		} else { \
			dx_memcpy((void*)(_new_elem_buffer + position + 1), (const void*)((elem_type*)(array_obj).elements + position), \
					((array_obj).size - position) * sizeof(elem_type)); \
			dx_free(_buffer_to_free); \
		} \
		_new_elem_buffer[position] = new_elem; \
		\
		(array_obj).elements = _new_elem_buffer; \
		++((array_obj).size); \
	} while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Deletion

 *  array_obj - an array descriptor structure. this structure must contain
				at least three fields:
		elements - a pointer to the array buffer (of type 'elem_type*')
		size - an integer value containing the number of elements in the buffer
		capacity - the current buffer capacity, may be greater than size
 *  elem_type - type of elements in the buffer
 *  position - an index of a position where the new element is to be inserted,
			is determined by a previous call of DX_ARRAY_SEARCH
 *  capacity_mgr - a function with two parameters:
		new_size - an size_t parameter passed by value, contains the desired
				new size of the array
		capacity - an size_t parameter passed by pointer, contains the current
				array capacity

		Return value:
			true - reallocation is required, in this case the new capacity
				value is stored in the capacity parameter
			false - no reallocation is required
 *  error - a boolean return value, is true if some error occurred during insertion,
			is false otherwise
 */
#define DX_ARRAY_DELETE(array_obj, elem_type, position, capacity_mgr, error) \
	do { \
		void* _buffer_to_free = NULL; \
		elem_type* _new_elem_buffer = (array_obj).elements; \
		\
		error = false; \
		\
		if (capacity_mgr((array_obj).size - 1, &((array_obj).capacity))) { \
			_new_elem_buffer = (elem_type*)dx_calloc((array_obj).capacity, sizeof(elem_type)); \
			\
			if (_new_elem_buffer == NULL) { \
				error = true; \
				\
				break; \
			} \
			\
			dx_memcpy((void*)_new_elem_buffer, (const void*)(array_obj).elements, position * sizeof(elem_type)); \
			\
			_buffer_to_free = (void*)(array_obj).elements; \
		} \
		\
		if (_buffer_to_free == NULL) { \
			dx_memmove((void*)(_new_elem_buffer + position), (const void*)((elem_type*)(array_obj).elements + position + 1), \
					((array_obj).size - position - 1) * sizeof(elem_type)); \
		} else { \
			dx_memcpy((void*)(_new_elem_buffer + position), (const void*)((elem_type*)(array_obj).elements + position + 1), \
					((array_obj).size - position - 1) * sizeof(elem_type)); \
			dx_free(_buffer_to_free); \
		} \
		\
		(array_obj).elements = _new_elem_buffer; \
		--((array_obj).size); \
	} while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Shuffle
 *  Shuffles the array elements in a random order.

 *  a - array object (most likely of type 'type*' or 'type[]')
 *  elem_type - type of elements in the array
 *  size - the number of elements in the array
 */
#define DX_ARRAY_SHUFFLE(a, elem_type, size) \
	do { \
		size_t _idx = (size) - 1; \
		size_t _rand_idx; \
		\
		for (; _idx > 0; --_idx) { \
			_rand_idx = dx_random_size(_idx); \
			\
			DX_SWAP(elem_type, (a)[_idx], (a)[_rand_idx]); \
		} \
	} while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Reservation

 *  array_obj - an array descriptor structure. this structure must contain
				at least three fields:
		elements - a pointer to the array buffer (of type 'elem_type*')
		size - an integer value containing the number of elements in the buffer
		capacity - the current buffer capacity, may be greater than size
 *  elem_type - type of elements in the buffer
 *  reserve_size - a new size to reservation
 *  error - a boolean return value, is true if some error occurred during
			reservation, is false otherwise
 */
#define DX_ARRAY_RESERVE(array_obj, elem_type, reserve_size, error) \
	do { \
		elem_type* _new_elem_buffer = (array_obj).elements; \
		error = false; \
		if (reserve_size <= (array_obj).size) { \
			break; \
		} \
		\
		if (reserve_size > (array_obj).capacity) { \
			_new_elem_buffer = (elem_type*)dx_calloc(reserve_size, sizeof(elem_type)); \
			\
			if (_new_elem_buffer == NULL) { \
				error = true; \
				\
				break; \
			} \
			\
			dx_memcpy((void*)_new_elem_buffer, (const void*)(array_obj).elements, (array_obj).size * sizeof(elem_type)); \
			(array_obj).capacity = reserve_size; \
			dx_free((void*)(array_obj).elements); \
		} \
		\
		(array_obj).elements = _new_elem_buffer; \
		(array_obj).size = reserve_size; \
	} while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Functions and predicates used in array operations
 */
#define DX_NUMERIC_COMPARATOR(l, r) (((l)>(r)?1:((l)<(r)?-1:0)))

/* -------------------------------------------------------------------------- */

bool dx_capacity_manager_halfer (size_t new_size, size_t* capacity);

/* -------------------------------------------------------------------------- */
/*
 *	String functions
 */
/* -------------------------------------------------------------------------- */

dxf_string_t dx_create_string (size_t size);
dxf_string_t dx_create_string_src (dxf_const_string_t src);

char* dx_ansi_create_string (size_t size);
char* dx_ansi_create_string_src (const char* src);
char* dx_ansi_create_string_src_len(const char* src, size_t len);
char* dx_ansi_copy_string_len(char* dest, const char* src, size_t len);
size_t dx_ansi_string_length(const char* str);

dxf_string_t dx_create_string_src_len (dxf_const_string_t src, size_t len);
dxf_string_t dx_copy_string (dxf_string_t dest, dxf_const_string_t src);
dxf_string_t dx_copy_string_len(dxf_string_t dest, dxf_const_string_t src, size_t len);
size_t dx_string_length(dxf_const_string_t str);
bool dx_string_null_or_empty(dxf_const_string_t str);
int dx_compare_strings (dxf_const_string_t s1, dxf_const_string_t s2);
int dx_compare_strings_num (dxf_const_string_t s1, dxf_const_string_t s2, size_t num);
dxf_char_t dx_toupper (dxf_char_t c);
dxf_string_t dx_ansi_to_unicode (const char* ansi_str);
dxf_string_t dx_decode_from_integer (dxf_long_t code);
dxf_string_t dx_concatenate_strings(dxf_string_t dest, dxf_const_string_t src);

/* -------------------------------------------------------------------------- */
/*
 *	Time constants
 */
/* -------------------------------------------------------------------------- */

/**
 * Number of milliseconds in a second.
 */
static const dxf_long_t DX_TIME_SECOND = 1000L;

/* -------------------------------------------------------------------------- */
/*
 *	Time functions
 */
/* -------------------------------------------------------------------------- */

int dx_millisecond_timestamp (void);
int dx_millisecond_timestamp_diff (int newer, int older);

/**
 * Returns correct number of seconds with proper handling negative values and overflows.
 * Idea is that number of milliseconds shall be within [0..999]
 * as that the following equation always holds
 * dx_get_seconds_from_time(millis) * 1000L + dx_get_millis_from_time(millis) == millis
 */
dxf_int_t dx_get_seconds_from_time(dxf_long_t millis);

/**
 * Returns correct number of milliseconds with proper handling negative values.
 * Idea is that number of milliseconds shall be within [0..999]
 * as that the following equation always holds
 * dx_get_seconds_from_time(millis) * 1000L + dx_get_millis_from_time(millis) == millis
 */
dxf_int_t dx_get_millis_from_time(dxf_long_t millis);

/* -------------------------------------------------------------------------- */
/*
 *	Base64 encoding operations implementation
 */
/* -------------------------------------------------------------------------- */

bool dx_base64_encode(const char* in, size_t in_len, char* out, size_t out_len);
bool dx_base64_decode(const char* in, size_t in_len, char* out, size_t *out_len);
size_t dx_base64_length(size_t in_len);

#ifdef _WIN32

// Following functions provide generation of full memory barrier (or fence) to ensure that memory
// operations are completed in order. Also 64 bits functions provide atomic read/write on 32 bit platforms

__int64 atomic_read(__int64 volatile * value);
void atomic_write(__int64 volatile * dest, __int64 src);
__int32 atomic_read32(__int32 volatile * value);
void atomic_write32(__int32 volatile * dest, __int32 src);
time_t atomic_read_time(time_t volatile * value);
void atomic_write_time(time_t volatile * dest, time_t src);

#else

#warning "no fence, no atomic read/write for 64 bit variables on 32 bit platforms, additional synchronization is needed";

long long atomic_read(long long* value);
void atomic_write(long long* dest, long long src);
long atomic_read32(long* value);
void atomic_write32(long* dest, long src);
time_t atomic_read_time(time_t* value);
void atomic_write_time(time_t* dest, time_t src);

#endif

#endif /* DX_ALGORITHMS_H_INCLUDED */
