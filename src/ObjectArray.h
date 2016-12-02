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
*	Contains the functionality for managing the memory required to store
*  the string data
*/

#ifndef OBJECT_ARRAY_H_INCLUDED
#define OBJECT_ARRAY_H_INCLUDED

#include "DXTypes.h"
#include "PrimitiveTypes.h"

#define DX_OBJECT_ARRAY_NAME(alias) \
dx_##alias##_array_t

#define DX_OBJECT_ARRAY_NAME_PTR(alias) \
dx_##alias##_array_ptr_t

#define DX_OBJECT_ARRAY_STRUCT(type, alias) \
typedef struct { \
    type* elements; \
    int size; \
    int capacity; \
} DX_OBJECT_ARRAY_NAME(alias), *DX_OBJECT_ARRAY_NAME_PTR(alias); 
//} dx_##alias##_array_t, *dx_##alias##_array_ptr_t; 

#define DX_OBJECT_ARRAY_ADD_PROTOTYPE(type, alias) \
bool dx_##alias##_array_add(DX_OBJECT_ARRAY_NAME(alias)* object_array, type obj)

#define DX_OBJECT_ARRAY_FREE_PROTOTYPE(alias) \
void dx_##alias##_array_free(DX_OBJECT_ARRAY_NAME(alias)* object_array)

//TODO: remove commented code
//TODO: possible need to declare structs without macro

//typedef struct {
//    dxf_const_string_t* elements;
//    int size;
//    int capacity;
//} dx_string_array_t, *dx_string_array_ptr_t;
DX_OBJECT_ARRAY_STRUCT(dxf_const_string_t, string)

//bool dx_string_array_add(dx_string_array_t* string_array, dxf_const_string_t str);
//void dx_string_array_free(dx_string_array_t* string_array);

DX_OBJECT_ARRAY_ADD_PROTOTYPE(dxf_const_string_t, string);
DX_OBJECT_ARRAY_FREE_PROTOTYPE(string);

DX_OBJECT_ARRAY_STRUCT(dxf_byte_array_t, byte_buffer)
DX_OBJECT_ARRAY_ADD_PROTOTYPE(dxf_byte_array_t, byte_buffer);
DX_OBJECT_ARRAY_FREE_PROTOTYPE(byte_buffer);

#endif /* OBJECT_ARRAY_H_INCLUDED */