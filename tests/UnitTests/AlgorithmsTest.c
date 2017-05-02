#include <stdio.h>

#include "AlgorithmsTest.h"
#include "DXAlgorithms.h"
#include "TestHelper.h"

typedef struct {
    const char* source;
    const char* encoded;
} base64_test_case;

typedef struct {
    size_t base64_size;
    char* base64_buf;
    size_t decoded_size;
    char* decoded_buf;
} base64_test_context;

static base64_test_case base64_cases[] = {
    { "demo:demo", "ZGVtbzpkZW1v" },
    { "demo:demo1", "ZGVtbzpkZW1vMQ==" },
    { "demo:demo12", "ZGVtbzpkZW1vMTI=" },
    { "demo:demo123", "ZGVtbzpkZW1vMTIz" },
    { "demo:demo1234", "ZGVtbzpkZW1vMTIzNA==" }
};

static size_t base64_cases_count = SIZE_OF_ARRAY(base64_cases);

static void base64_free_context(base64_test_context* context) {
    if (context->base64_buf != NULL)
        dx_free(context->base64_buf);
    if (context->decoded_buf != NULL)
        dx_free(context->decoded_buf);
    context->base64_buf = NULL;
    context->decoded_buf = NULL;
    context->base64_size = 0;
    context->decoded_size = 0;
}

/*
* Test
*
* Tries to calculate a future base64 string length and compare it with etalon value.
*
* Expected: application shouldn't crash; all checks should be passed.
*/
static bool algorithms_base64_length_test(void) {
    size_t i;
    for (i = 0; i < base64_cases_count; i++) {
        size_t expected = strlen(base64_cases[i].encoded);
        size_t actual = dx_base64_length(strlen(base64_cases[i].source));
        DX_CHECK(dx_is_equal_size_t(expected, actual));
    }
    return true;
}

/* -------------------------------------------------------------------------- */

static bool base64_run_case(const base64_test_case* test_case, base64_test_context* context) {
    size_t source_size = strlen(test_case->source);
    int i;
    for (i = 0; i < 2; i++) {
        base64_free_context(context);
        switch (i) {
            case 0:
                //allocate calculated memory
                context->base64_size = dx_base64_length(source_size);
                break;
            default:
                //allocate redundant memory 
                context->base64_size = dx_base64_length(source_size) + 10;
                break;
        }
        context->base64_buf = dx_ansi_create_string(context->base64_size);
        context->decoded_size = context->base64_size;
        context->decoded_buf = dx_ansi_create_string(context->decoded_size);
        //encoding
        DX_CHECK(dx_is_true(dx_base64_encode(test_case->source, source_size, context->base64_buf, context->base64_size)));
        DX_CHECK(dx_is_equal_int(0, strcmp(context->base64_buf, test_case->encoded)));
        //decoding
        DX_CHECK(dx_is_true(dx_base64_decode((const char*)context->base64_buf, strlen(context->base64_buf), context->decoded_buf, &context->decoded_size)));
        DX_CHECK(dx_is_equal_int(0, strcmp(context->decoded_buf, test_case->source)));
    }
    base64_free_context(context);
    return true;
}

/*
 * Test
 *
 * Tries to encode and decode various cases of string.
 *
 * Expected: application shouldn't crash; all checks should be passed.
 */
static bool algorithms_base64_encode_decode_test(void) {
    size_t i;
    base64_test_context context = { 0 };
    for (i = 0; i < base64_cases_count; i++) {
        bool res = base64_run_case((const base64_test_case*)&base64_cases[i], &context);
        base64_free_context(&context);
        DX_CHECK(res);
    }
    return true;
}

/* -------------------------------------------------------------------------- */

bool algorithms_all_tests(void) {
    bool res = true;

    if (!algorithms_base64_length_test() ||
        !algorithms_base64_encode_decode_test()) {

        res = false;
    }
    return res;
}