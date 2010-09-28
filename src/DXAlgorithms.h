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

#include "PrimitiveTypes.h"
#include "DXMemory.h"

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
/* -------------------------------------------------------------------------- */

#define DX_ARRAY_BINARY_SEARCH(a, start_index, end_index, elem, comparator, found, res_index) \
    do { \
        int _begin = start_index; \
        int _end = end_index; \
        int _mid; \
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
/* -------------------------------------------------------------------------- */

#define DX_ARRAY_SEARCH(a, start_index, end_index, elem, comparator, is_binary, found, res_index) \
    do { \
        found = false; \
        \
        if (is_binary) { \
            DX_ARRAY_BINARY_SEARCH(a, start_index, end_index, elem, comparator, found, res_index); \
        } else { \
            int _index = start_index; \
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
        new_size - an integer parameter passed by value, contains the desired
                   new size of the array
        capacity - an integer parameter passed by pointer, contains the current
                   array capacity
        
        Return value:
            true - reallocation is required, in this case the new capacity
                   value is stored in the capacity parameter
            false - no reallocation is required
 *  error - a boolean return value, is true if some error occurred during insertion,
            is false otherwise
 */
/* -------------------------------------------------------------------------- */

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
        _new_elem_buffer[position] = new_elem; \
        \
        if (_buffer_to_free == NULL) { \
            dx_memmove((void*)(_new_elem_buffer + position + 1), (const void*)((array_obj).elements + position), \
                       ((array_obj).size - position) * sizeof(elem_type)); \
        } else { \
            dx_memcpy((void*)(_new_elem_buffer + position + 1), (const void*)((array_obj).elements + position), \
                      ((array_obj).size - position) * sizeof(elem_type)); \
            dx_free(_buffer_to_free); \
        } \
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
        new_size - an integer parameter passed by value, contains the desired
                   new size of the array
        capacity - an integer parameter passed by pointer, contains the current
                   array capacity

        Return value:
            true - reallocation is required, in this case the new capacity
                   value is stored in the capacity parameter
            false - no reallocation is required
 *  error - a boolean return value, is true if some error occurred during insertion,
            is false otherwise
*/
/* -------------------------------------------------------------------------- */

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
            dx_memmove((void*)(_new_elem_buffer + position), (const void*)((array_obj).elements + position + 1), \
                       ((array_obj).size - position - 1) * sizeof(elem_type)); \
        } else { \
            dx_memcpy((void*)(_new_elem_buffer + position), (const void*)((array_obj).elements + position + 1), \
                      ((array_obj).size - position - 1) * sizeof(elem_type)); \
            dx_free(_buffer_to_free); \
        } \
        \
        (array_obj).elements = _new_elem_buffer; \
        --((array_obj).size); \
    } while (false)

/* -------------------------------------------------------------------------- */
/*
 *	Functions and predicates used in array operations
 */
/* -------------------------------------------------------------------------- */

#define DX_NUMERIC_COMPARATOR(l, r) (((l)>(r)?1:((l)<(r)?-1:0)))

/* -------------------------------------------------------------------------- */

bool dx_capacity_manager_halfer (int new_size, int* capacity);

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
    
/* -------------------------------------------------------------------------- */
/*
 *	String functions
 */
/* -------------------------------------------------------------------------- */

dx_string_t dx_create_string (int size);
dx_string_t dx_create_string_src (dx_const_string_t src);
dx_string_t dx_create_string_src_len (dx_const_string_t src, int len);
dx_string_t dx_copy_string (dx_string_t dest, dx_const_string_t src);
dx_string_t dx_copy_string_len (dx_string_t dest, dx_const_string_t src, int len);
int dx_string_length (dx_const_string_t str);
int dx_compare_strings (dx_const_string_t s1, dx_const_string_t s2);
dx_char_t dx_toupper (dx_char_t c);
dx_string_t dx_ansi_to_unicode (const char* ansi_str);
dx_string_t dx_decode_from_integer (dx_long_t code);

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
    
/* -------------------------------------------------------------------------- */
/*
 *	Bit operations
 */
/* -------------------------------------------------------------------------- */

#define UNSIGNED_TYPE_dx_int_t \
    dx_uint_t
    
#define UNSIGNED_TYPE_dx_long_t \
    dx_ulong_t
    
#define UNSIGNED_TYPE(type) \
    UNSIGNED_TYPE_##type

#define UNSIGNED_LEFT_SHIFT(value, offset, type) \
    ((type)((UNSIGNED_TYPE(type))value >> offset))

bool dx_is_only_single_bit_set (int value);

#endif /* DX_ALGORITHMS_H_INCLUDED */