#include "DXFeed.h"

#include "BufferedInput.h"
#include "BufferedOutput.h"
#include "DXMemory.h"


struct test_data_t {
	dxf_bool_t   bool_data;
	dxf_byte_t   byte_data;
	dxf_char_t   char_data;
	dxf_short_t  short_data;
	dxf_int_t    int_data;
	dxf_long_t   long_data;
	dxf_float_t  float_data;
	dxf_double_t double_data;
	//dx_byte_t*  bytes_data;
	//dx_char_t*  chars_data;
	dxf_int_t    compact_int_data;
	dxf_long_t   compact_long_data;
};

/* -------------------------------------------------------------------------- */

struct test_data_t test_data = {true, 120, L'A', 10000, 100000, 5000000000, 1.6f, 1.6e200, 25, 400};

/* -------------------------------------------------------------------------- */

void dx_test_write(const struct test_data_t* data) {
	//dx_write_boolean(data->bool_data);
	//dx_write_byte(data->byte_data);
	//dx_write_char(data->char_data);
	//dx_write_short(data->short_data);
	//dx_write_int(data->int_data);
	//dx_write_long(data->long_data);
	//dx_write_float(data->float_data);
	//dx_write_double(data->double_data);
	//dx_write_compact_int(data->compact_int_data);
	//dx_write_compact_long(data->compact_long_data);
}

/* -------------------------------------------------------------------------- */

void dx_test_read(OUT struct test_data_t* data) {
	//dx_read_boolean(&data->bool_data);
	//dx_read_byte(&data->byte_data);
	//dx_read_char(&data->char_data);
	//dx_read_short(&data->short_data);
	//dx_read_int(&data->int_data);
	//dx_read_long(&data->long_data);
	//dx_read_float(&data->float_data);
	//dx_read_double(&data->double_data);
	//dx_read_compact_int(&data->compact_int_data);
	//dx_read_compact_long(&data->compact_long_data);
}

/* -------------------------------------------------------------------------- */

bool dx_compare(const struct test_data_t* data_1, const struct test_data_t* data_2) {
	//return data_1->bool_data == data_2->bool_data
	//    && data_1->byte_data == data_2->byte_data
	//    && data_1->char_data == data_2->char_data
	//    && data_1->short_data == data_2->short_data
	//    && data_1->int_data == data_2->int_data
	//    && data_1->long_data == data_2->long_data
	//    && data_1->float_data == data_2->float_data
	//    && data_1->double_data == data_2->double_data
	//    && data_1->compact_int_data == data_2->compact_int_data
	//    && data_1->compact_long_data == data_2->compact_long_data;
	return 0;
}

/* -------------------------------------------------------------------------- */

bool test() {
/*    struct test_data_t read_data;

	dx_byte_t* buffer = dx_malloc(10);
	dx_set_out_buffer(buffer, 10);

	dx_test_write(&test_data);

	dx_test_read(&read_data);

	return dx_compare(&test_data, &read_data);*/
	return false;
}
