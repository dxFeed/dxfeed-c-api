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

#include "DXFeed.h"

#include "DataStructures.h"
#include "BufferedIOCommon.h"
#include "DXAlgorithms.h"
#include "ConnectionContextData.h"
#include "DXErrorHandling.h"
#include "Logger.h"
#include "ServerMessageProcessor.h"

/* -------------------------------------------------------------------------- */
/*
 *	Trade data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_trade[] = { 
	{ dx_fid_compact_int, L"Last.Time", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, time) },
	{ dx_fid_utf_char, L"Last.Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, exchange_code), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, exchange_code) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Last.Price", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, price) },
	{ dx_fid_compact_int, L"Last.Size", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, size) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Volume", DX_RECORD_FIELD_SETTER_NAME(dx_trade_t, day_volume), DX_RECORD_FIELD_DEF_VAL_NAME(dx_trade_t, day_volume) }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Quote data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_quote[] = {
    { dx_fid_compact_int, L"Bid.Time", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_time) },
    { dx_fid_utf_char, L"Bid.Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_exchange_code), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_exchange_code) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Price", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_price) },
    { dx_fid_compact_int, L"Bid.Size", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, bid_size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, bid_size) },
    { dx_fid_compact_int, L"Ask.Time", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_time) },
    { dx_fid_utf_char, L"Ask.Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_exchange_code), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_exchange_code) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Price", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_price) },
    { dx_fid_compact_int, L"Ask.Size", DX_RECORD_FIELD_SETTER_NAME(dx_quote_t, ask_size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_quote_t, ask_size) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Fundamental data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_fundamental[] = { 
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"High.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, day_high_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, day_high_price) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Low.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, day_low_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, day_low_price) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Open.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, day_open_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, day_open_price) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Close.Price", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, prev_day_close_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, prev_day_close_price) },
	{ dx_fid_compact_int, L"OpenInterest", DX_RECORD_FIELD_SETTER_NAME(dx_fundamental_t, open_interest), DX_RECORD_FIELD_DEF_VAL_NAME(dx_fundamental_t, open_interest) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Profile data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_profile[] = { 
	{ dx_fid_utf_char_array, L"Description", DX_RECORD_FIELD_SETTER_NAME(dx_profile_t, description), DX_RECORD_FIELD_DEF_VAL_NAME(dx_profile_t, description) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Market maker data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_market_maker[] = { 
	{ dx_fid_utf_char, L"MMExchange", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mm_exchange), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mm_exchange) },
	{ dx_fid_compact_int, L"MMID", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mm_id), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mm_id) },
	{ dx_fid_compact_int, L"MMBid.Time", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmbid_time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmbid_time) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"MMBid.Price", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmbid_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmbid_price) },
	{ dx_fid_compact_int, L"MMBid.Size", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmbid_size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmbid_size) },
	{ dx_fid_compact_int, L"MMAsk.Time", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmask_time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmask_time) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"MMAsk.Price", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmask_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmask_price) },
	{ dx_fid_compact_int, L"MMAsk.Size", DX_RECORD_FIELD_SETTER_NAME(dx_market_maker_t, mmask_size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_market_maker_t, mmask_size) }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Order data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_order[] = { 
	{ dx_fid_compact_int, L"Index", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, index), DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, index) },
	{ dx_fid_compact_int, L"Time", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, time) },
	{ dx_fid_compact_int, L"Sequence", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, sequence), DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, sequence) },
	{ dx_fid_compact_int | dx_fid_flag_decimal, L"Price", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, price) },
	{ dx_fid_compact_int, L"Size", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, size) },
	{ dx_fid_compact_int, L"Flags", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, flags), DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, flags) },
	{ dx_fid_compact_int, L"MMID", DX_RECORD_FIELD_SETTER_NAME(dx_order_t, mmid), DX_RECORD_FIELD_DEF_VAL_NAME(dx_order_t, mmid) }
}; 

/* -------------------------------------------------------------------------- */
/*
 *	Time and Sale data fields
 */
/* -------------------------------------------------------------------------- */

static const dx_field_info_t dx_fields_time_and_sale[] = {
    { dx_fid_compact_int, L"Time", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, time), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, time) },
    { dx_fid_compact_int, L"Sequence", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, sequence), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, sequence) },
    { dx_fid_utf_char, L"Exchange", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, exchange_code), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, exchange_code) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Price", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, price) },
    { dx_fid_compact_int, L"Size", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, size), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, size) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Bid.Price", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, bid_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, bid_price) },
    { dx_fid_compact_int | dx_fid_flag_decimal, L"Ask.Price", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, ask_price), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, ask_price) },
    { dx_fid_compact_int, L"ExchangeSaleConditions", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, exch_sale_conds), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, exch_sale_conds) },
    { dx_fid_compact_int, L"Flags", DX_RECORD_FIELD_SETTER_NAME(dx_time_and_sale_t, type), DX_RECORD_FIELD_DEF_VAL_NAME(dx_time_and_sale_t, type) }
};

/* -------------------------------------------------------------------------- */
/*
 *	Records
 */
/* -------------------------------------------------------------------------- */

static const int g_record_field_counts[dx_rid_count] = {
    sizeof(dx_fields_trade) / sizeof(dx_fields_trade[0]),
    sizeof(dx_fields_quote) / sizeof(dx_fields_quote[0]),
    sizeof(dx_fields_fundamental) / sizeof(dx_fields_fundamental[0]),
    sizeof(dx_fields_profile) / sizeof(dx_fields_profile[0]),
    sizeof(dx_fields_market_maker) / sizeof(dx_fields_market_maker[0]),
    sizeof(dx_fields_order) / sizeof(dx_fields_order[0]),
    sizeof(dx_fields_time_and_sale) / sizeof(dx_fields_time_and_sale[0])
};

static const dx_record_type_t g_record_types[dx_rid_count] = {
    { L"Trade", sizeof(dx_fields_trade) / sizeof(dx_fields_trade[0]), dx_fields_trade },
    { L"Quote", sizeof(dx_fields_quote) / sizeof(dx_fields_quote[0]), dx_fields_quote },
    { L"Fundamental", sizeof(dx_fields_fundamental) / sizeof(dx_fields_fundamental[0]), dx_fields_fundamental },
    { L"Profile", sizeof(dx_fields_profile) / sizeof(dx_fields_profile[0]), dx_fields_profile },
    { L"MarketMaker", sizeof(dx_fields_market_maker) / sizeof(dx_fields_market_maker[0]), dx_fields_market_maker },
    { L"Order", sizeof(dx_fields_order) / sizeof(dx_fields_order[0]), dx_fields_order },
    { L"TimeAndSale", sizeof(dx_fields_time_and_sale) / sizeof(dx_fields_time_and_sale[0]), dx_fields_time_and_sale }
};

/* In the Java code, the exchange code is determined by the record name: it's the last symbol of
   the record name if the second last symbol is '&', otherwise it's zero.
   Here we don't have any exchange code semantics in the record names (and neither the Java code does),
   so all the record exchange codes are set to zero 
   */

//TODO: list, add freeing
static dx_record_list_t g_records_list = {NULL, 0, 0};

/* -------------------------------------------------------------------------- */
/*
 *	Data structures connection context
 */
/* -------------------------------------------------------------------------- */

#define RECORD_ID_VECTOR_SIZE   1000

typedef struct {
    dxf_int_t server_record_id;
    dx_record_id_t local_record_id;
} dx_record_id_pair_t;

typedef struct {
    dx_record_id_pair_t* elements;
    int size;
    int capacity;
} dx_record_id_map_t;

typedef struct {
    dx_record_id_t frequent_ids[RECORD_ID_VECTOR_SIZE];
    dx_record_id_map_t id_map;
} dx_server_local_record_id_map_t;

typedef struct {
    dxf_connection_t connection;
    
    dx_server_local_record_id_map_t record_id_map;
    //int record_server_support_states[dx_rid_count];
    //TODO: dynamic memory allocation
    dx_record_server_support_state_list_t record_server_support_states;
} dx_data_structures_connection_context_t;

#define CTX(context) \
    ((dx_data_structures_connection_context_t*)context)

/* -------------------------------------------------------------------------- */

void dx_clear_record_server_support_states(dx_data_structures_connection_context_t* context) {
    dx_record_server_support_state_list_t *states = NULL;
    if (context == NULL)
        return;
    states = &(context->record_server_support_states);
    if (states->elements == NULL)
        return;

    dx_free(states->elements);
    states->elements = NULL;
    states->size = 0;
    states->capacity = 0;
}

void dx_free_data_structures_context(dx_data_structures_connection_context_t** context) {
    if (context == NULL)
        return;
    dx_clear_record_server_support_states(*context);
    dx_free(*context);
    context = NULL;
}

DX_CONNECTION_SUBSYS_INIT_PROTO(dx_ccs_data_structures) {
    dx_data_structures_connection_context_t* context = dx_calloc(1, sizeof(dx_data_structures_connection_context_t));
    int i = 0;
    
    if (context == NULL) {
        return false;
    }
    
    context->connection = connection;
    context->record_server_support_states.elements = NULL;
    context->record_server_support_states.size = 0;
    context->record_server_support_states.capacity = 0;
    
    for (; i < RECORD_ID_VECTOR_SIZE; ++i) {
        context->record_id_map.frequent_ids[i] = DX_RECORD_ID_INVALID;
    }
    
    if (!dx_set_subsystem_data(connection, dx_ccs_data_structures, context)) {
        dx_free_data_structures_context(&context);

        return false;
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_DEINIT_PROTO(dx_ccs_data_structures) {
    bool res = true;
    dx_data_structures_connection_context_t* context = dx_get_subsystem_data(connection, dx_ccs_data_structures, &res);
    
    if (context == NULL) {
        return res;
    }
    
    CHECKED_FREE(context->record_id_map.id_map.elements);
    dx_free_data_structures_context(&context);
    
    return true;
}

/* -------------------------------------------------------------------------- */

DX_CONNECTION_SUBSYS_CHECK_PROTO(dx_ccs_data_structures) {
    return true;
}

/* -------------------------------------------------------------------------- */
/*
 *	Connection context functions implementation
 */
/* -------------------------------------------------------------------------- */

void* dx_get_data_structures_connection_context (dxf_connection_t connection) {
    return dx_get_subsystem_data(connection, dx_ccs_data_structures, NULL);
}

/* -------------------------------------------------------------------------- */
/*
 *	Record functions implementation
 */
/* -------------------------------------------------------------------------- */

#define RECORD_ID_PAIR_COMPARATOR(left, right) \
    DX_NUMERIC_COMPARATOR((left).server_record_id, (right).server_record_id)

dx_record_id_t dx_get_record_id(void* context, dxf_int_t server_record_id) {
    dx_server_local_record_id_map_t* record_id_map = &(CTX(context)->record_id_map);
        
    if (server_record_id >= 0 && server_record_id < RECORD_ID_VECTOR_SIZE) {
        return record_id_map->frequent_ids[server_record_id];
    } else {
        dx_record_id_t idx;
        bool found = false;
        dx_record_id_pair_t dummy;
        
        dummy.server_record_id = server_record_id;
        
        DX_ARRAY_SEARCH(record_id_map->id_map.elements, 0, record_id_map->id_map.size, dummy, RECORD_ID_PAIR_COMPARATOR, true, found, idx);
        
        if (!found) {
            return DX_RECORD_ID_INVALID;
        }
        
        return record_id_map->id_map.elements[idx].local_record_id;
    }
}

/* -------------------------------------------------------------------------- */

bool dx_assign_server_record_id(void* context, dx_record_id_t record_id, dxf_int_t server_record_id) {
    dx_server_local_record_id_map_t* record_id_map = &(CTX(context)->record_id_map);
        
    if (server_record_id >= 0 && server_record_id < RECORD_ID_VECTOR_SIZE) {
        record_id_map->frequent_ids[server_record_id] = record_id;
    } else {
        int idx;
        bool found = false;
        bool failed = false;
        dx_record_id_pair_t rip;
        
        rip.server_record_id = server_record_id;
        rip.local_record_id = record_id;
        
        DX_ARRAY_SEARCH(record_id_map->id_map.elements, 0, record_id_map->id_map.size, rip, RECORD_ID_PAIR_COMPARATOR, true, found, idx);
        
        if (found) {
            record_id_map->id_map.elements[idx].local_record_id = record_id;
            
            return true;
        }
        
        DX_ARRAY_INSERT(record_id_map->id_map, dx_record_id_pair_t, rip, idx, dx_capacity_manager_halfer, failed);
        
        if (failed) {
            return false;
        }
    }
    
    return true;
}

/* -------------------------------------------------------------------------- */

const dx_new_record_info_t* dx_get_record_by_id(dx_record_id_t record_id) {
    return &g_records_list.elements[record_id];
}

/* -------------------------------------------------------------------------- */

dx_record_id_t dx_get_record_id_by_name(dxf_const_string_t record_name) {
    dx_record_id_t record_id = 0;

    for (; record_id < g_records_list.size; ++record_id) {
        if (dx_compare_strings(g_records_list.elements[record_id].name, record_name) == 0) {
            return record_id;
        }
    }

    return DX_RECORD_ID_INVALID;
}

/* -------------------------------------------------------------------------- */

int dx_find_record_field (const dx_new_record_info_t* record_info, dxf_const_string_t field_name,
                          dxf_int_t field_type) {
    int cur_field_index = 0;
    dx_field_info_t* fields = (dx_field_info_t*)record_info->fields;
    
    for (; cur_field_index < record_info->field_count; ++cur_field_index) {
        if (dx_compare_strings(fields[cur_field_index].name, field_name) == 0 &&
            (fields[cur_field_index].type & dx_fid_mask_serialization)== (field_type & dx_fid_mask_serialization)) {
            
            return cur_field_index;
        }
    }
    
    return INVALID_INDEX;
}

/* -------------------------------------------------------------------------- */

dxf_char_t dx_get_record_exchange_code(dx_record_id_t record_id) {
    if (record_id < 0 || record_id > g_records_list.size)
        return '\0';
    return g_records_list.elements[record_id].exchange_code;
}

bool dx_set_record_exchange_code(dx_record_id_t record_id, dxf_char_t exchange_code) {
    if (record_id < 0 || record_id > g_records_list.size)
        return false;
    g_records_list.elements[record_id].exchange_code = exchange_code;
    return true;
}

/* -------------------------------------------------------------------------- */

dx_record_server_support_state_list_t* dx_get_record_server_support_states(void* context) {
    return &(CTX(context)->record_server_support_states);
}

bool dx_get_record_server_support_state_value(dx_record_server_support_state_list_t* states, 
    dx_record_id_t record_id,
                                              OUT dx_record_server_support_state_t **value) {
    if (record_id < 0 || record_id >= states->size)
        return false;
    *value = &(states->elements[record_id]);
    return true;
}

/* -------------------------------------------------------------------------- */

/* Functions for working with records list */

bool dx_add_record_to_list(dxf_connection_t connection, dx_new_record_info_t record, dx_record_id_t index) {
    bool failed = false;
    dx_new_record_info_t new_record;
    dx_record_server_support_state_t new_state;
    dx_data_structures_connection_context_t* dscc = NULL;

    // Update records list
    new_record.name = dx_create_string_src(record.name);
    new_record.field_count = record.field_count;
    new_record.fields = record.fields;
    new_record.type_id = record.type_id;
    dx_memcpy(new_record.suffix, record.suffix, sizeof(record.suffix));
    new_record.exchange_code = record.exchange_code;

    DX_ARRAY_INSERT(g_records_list, dx_new_record_info_t, new_record, index, dx_capacity_manager_halfer, failed);

    if (failed) {
        return dx_set_error_code(dx_sec_not_enough_memory);
    }

    // Update record server support states
    dscc = dx_get_data_structures_connection_context(connection);
    if (dscc == NULL) {
        DX_ARRAY_DELETE(g_records_list, dx_new_record_info_t, index, dx_capacity_manager_halfer, failed);
        return dx_set_error_code(dx_cec_connection_context_not_initialized);
    }

    new_state = 0;
    DX_ARRAY_INSERT(dscc->record_server_support_states, dx_record_server_support_state_t, new_state, index, dx_capacity_manager_halfer, failed);

    if (failed) {
        DX_ARRAY_DELETE(g_records_list, dx_new_record_info_t, index, dx_capacity_manager_halfer, failed);
        return dx_set_error_code(dx_sec_not_enough_memory);
    }

    //Update record digests
    if (!dx_add_record_digest_to_list(connection, index)) {
        DX_ARRAY_DELETE(g_records_list, dx_new_record_info_t, index, dx_capacity_manager_halfer, failed);
        DX_ARRAY_DELETE(dscc->record_server_support_states, dx_record_server_support_state_t, index, dx_capacity_manager_halfer, failed);
        return false;
    }

    return true;
}

dx_record_type_id_t dx_string_to_record_type(dxf_const_string_t name)
{
    if (dx_compare_strings_num(name, g_record_types[dx_rid_trade].name, dx_string_length(g_record_types[dx_rid_trade].name)) == 0)
        return dx_rid_trade;
    else if (dx_compare_strings_num(name, g_record_types[dx_rid_quote].name, dx_string_length(g_record_types[dx_rid_quote].name)) == 0)
        return dx_rid_quote;
    else if (dx_compare_strings_num(name, g_record_types[dx_rid_fundamental].name, dx_string_length(g_record_types[dx_rid_fundamental].name)) == 0)
        return dx_rid_fundamental;
    else if (dx_compare_strings_num(name, g_record_types[dx_rid_profile].name, dx_string_length(g_record_types[dx_rid_profile].name)) == 0)
        return dx_rid_profile;
    else if (dx_compare_strings_num(name, g_record_types[dx_rid_market_maker].name, dx_string_length(g_record_types[dx_rid_market_maker].name)) == 0)
        return dx_rid_market_maker;
    else if (dx_compare_strings_num(name, g_record_types[dx_rid_order].name, dx_string_length(g_record_types[dx_rid_order].name)) == 0)
        return dx_rid_order;
    else if (dx_compare_strings_num(name, g_record_types[dx_rid_time_and_sale].name, dx_string_length(g_record_types[dx_rid_time_and_sale].name)) == 0)
        return dx_rid_time_and_sale;
    else
        return dx_rid_invalid;
}

bool init_record_info(dx_new_record_info_t *record, dxf_const_string_t name) {
    dx_record_type_id_t record_type_id;
    dx_record_type_t record_type;
    int suffix_index;
    int name_length = dx_string_length(name);

    record_type_id = dx_string_to_record_type(name);
    if (record_type_id == dx_rid_invalid) {
        //TODO: make other error code
        dx_set_last_error(dx_ec_invalid_func_param);
        return false;
    }
    record_type = g_record_types[record_type_id];

    record->name = dx_create_string_src(name);
    record->field_count = g_record_types[record_type_id].field_count;
    record->fields = g_record_types[record_type_id].fields;
    record->type_id = record_type_id;
    dx_memset(record->suffix, '\0', sizeof(record->suffix));
    record->exchange_code = '\0';

    suffix_index = dx_string_length(record_type.name);
    if (name_length < suffix_index + 1)
        return true;
    if (record_type_id == dx_rid_order) {
        if (record->name[suffix_index] != L'#')
            return true;
        dx_copy_string_len(record->suffix, &record->name[suffix_index + 1], name_length - suffix_index);
    } else if (record_type_id == dx_rid_trade || record_type_id == dx_rid_quote) {
        if (record->name[suffix_index] != L'&')
            return true;
        dx_copy_string_len(record->suffix, &record->name[suffix_index + 1], 1);
    }

    return true;
}

void clear_record_info(dx_new_record_info_t *record) {
    dx_free(record->name);
    record->name = NULL;
}

#define DX_RECORDS_COMPARATOR(l, r) (dx_compare_strings(l.name, r.name))

dx_record_id_t dx_add_or_get_record_id(dxf_connection_t connection, dxf_const_string_t name) {
    bool result = true;
    bool found = false;
    dx_record_id_t index = DX_RECORD_ID_INVALID;
    dx_new_record_info_t record;

    if (!init_record_info(&record, name))
        return DX_RECORD_ID_INVALID;
    
    if (g_records_list.elements == NULL) {
        index = 0;
        result = dx_add_record_to_list(connection, record, index);
    } else {
        DX_ARRAY_SEARCH(g_records_list.elements, 0, g_records_list.size, record, DX_RECORDS_COMPARATOR, false, found, index);
        if (!found) {
            result = dx_add_record_to_list(connection, record, index);
        }
    }

    clear_record_info(&record);

    if (!result) {
        dx_logging_last_error();
        return DX_RECORD_ID_INVALID;
    }
        
    return index;
}

void dx_clear_records_list() {
    dx_record_id_t i = 0;
    dx_new_record_info_t *record = g_records_list.elements;
    for (; i < g_records_list.size; i++) {
        clear_record_info(record);
        record++;
    }
    dx_free(g_records_list.elements);
    g_records_list.elements = NULL;
    g_records_list.capacity = 0;
    g_records_list.size = 0;
}

int dx_get_records_list_count() {
    return g_records_list.size;
}