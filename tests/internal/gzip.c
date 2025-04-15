/* -*- Mode: C; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

#include <fluent-bit/flb_info.h>
#include <fluent-bit/flb_mem.h>
#include <fluent-bit/flb_gzip.h>

#include "flb_tests_internal.h"

/* Sample data */
char *morpheus = "This is your last chance. After this, there is no "
    "turning back. You take the blue pill - the story ends, you wake up in "
    "your bed and believe whatever you want to believe. You take the red pill,"
    "you stay in Wonderland and I show you how deep the rabbit-hole goes.";

void test_compress()
{
    int ret;
    int sample_len;
    char *in_data = morpheus;
    size_t in_len;
    void *str;
    size_t len;

    sample_len = strlen(morpheus);
    in_len = sample_len;
    ret = flb_gzip_compress(in_data, in_len, &str, &len);
    TEST_CHECK(ret == 0);

    in_data = str;
    in_len = len;

    ret = flb_gzip_uncompress(in_data, in_len, &str, &len);
    TEST_CHECK(ret == 0);

    TEST_CHECK(sample_len == len);
    ret = memcmp(morpheus, str, sample_len);
    TEST_CHECK(ret == 0);

    flb_free(in_data);
    flb_free(str);
}

/* Multiple gzip buffers concatenated together */
void test_compress_multi()
{
    int ret = 0;

    char *original = morpheus;
    size_t original_len = strlen(morpheus);

    char *original2 = flb_malloc(original_len);
    size_t original_len2 = original_len;

    void *compressed = NULL;
    size_t compressed_len = 0;

    void *compressed2 = NULL;
    size_t compressed_len2 = 0;

    void *concatenated = NULL;
    size_t concatenated_len = 0;

    void *uncompressed = NULL;
    size_t uncompressed_len = 0;

    size_t in_remaining = 0;

    // apply simple rot 16 to lowercase letters
    for (int i = 0; i < original_len; i++) {
        char c = original[i];

        if (c >= 'a' && c <= 'z') {
            c += 16;

            if (c > 'z') {
                c -= 26;
            }
        }

        original2[i] = c;
    }

    ret = flb_gzip_compress(original, original_len, &compressed, &compressed_len);
    TEST_CHECK(ret == 0);

    ret = flb_gzip_compress(original2, original_len2, &compressed2, &compressed_len2);
    TEST_CHECK(ret == 0);

    // concatenate the buffers together
    concatenated_len = compressed_len + compressed_len2;
    concatenated = flb_malloc(concatenated_len);

    memcpy(concatenated, compressed, compressed_len);
    memcpy(concatenated + compressed_len, compressed2, compressed_len2);

    flb_free(compressed);
    flb_free(compressed2);

    // uncompressed and verify payload 1
    ret = flb_gzip_uncompress_multi(concatenated, concatenated_len, &uncompressed, &uncompressed_len, &in_remaining);
    TEST_CHECK(ret == 0);

    TEST_CHECK(uncompressed_len == original_len);
    TEST_CHECK(in_remaining == compressed_len2);

    ret = memcmp(original, uncompressed, original_len);
    TEST_CHECK(ret == 0);

    flb_free(uncompressed);

    // uncompressed and verify payload 2
    ret = flb_gzip_uncompress_multi(concatenated + concatenated_len - in_remaining, in_remaining, &uncompressed, &uncompressed_len, &in_remaining);
    TEST_CHECK(ret == 0);

    TEST_CHECK(uncompressed_len == original_len2);
    TEST_CHECK(in_remaining == 0);

    ret = memcmp(original2, uncompressed, original_len2);
    TEST_CHECK(ret == 0);

    flb_free(concatenated);
    flb_free(uncompressed);
    flb_free(original2);
}

/* Uncompressed data is more than FLB_GZIP_BUFFER_SIZE */
void test_compress_large()
{
    int ret = 0;
    const int original_len = 10 * 1000 * 1000;
    char *original = flb_malloc(original_len);
    void *compressed = NULL;
    size_t compressed_len = 0;
    void *uncompressed = NULL;
    size_t uncompressed_len = 0;
    size_t in_remaining = 0;

    for (int i = 0; i < original_len; i++) {
        original[i] = i % 256;
    }

    ret = flb_gzip_compress(original, original_len, &compressed, &compressed_len);
    TEST_CHECK(ret == 0);

    ret = flb_gzip_uncompress_multi(compressed, compressed_len, &uncompressed, &uncompressed_len, &in_remaining);
    TEST_CHECK(ret == 0);

    TEST_CHECK(in_remaining == 0);
    TEST_CHECK(uncompressed_len == original_len);

    ret = memcmp(original, uncompressed, original_len);
    TEST_CHECK(ret == 0);

    flb_free(original);
    flb_free(compressed);
    flb_free(uncompressed);
}

/* Uncompressed data is more than FLB_GZIP_BUFFER_SIZE * FLB_GZIP_MAX_BUFFERS */
void test_compress_too_large()
{
    int ret = 0;
    const int original_len = 150 * 1000 * 1000;
    char *original = flb_malloc(original_len);
    void *compressed = NULL;
    size_t compressed_len = 0;
    void *uncompressed = NULL;
    size_t uncompressed_len = 0;
    size_t in_remaining = 0;

    ret = flb_gzip_compress(original, original_len, &compressed, &compressed_len);
    TEST_CHECK(ret == 0);

    ret = flb_gzip_uncompress_multi(compressed, compressed_len, &uncompressed, &uncompressed_len, &in_remaining);
    TEST_CHECK(ret != 0);

    flb_free(compressed);
    flb_free(original);
}

/* When compressed the gzip body contains a valid gzip header */
void test_header_in_gzip_body()
{
    char original[] = {
        0x06, 0x03, 0x00, 0x00, 0x07, 0x01, 0x05, 0x04, 0x07, 0x07, 0x02, 0x03, 0x00, 0x01, 0x00, 0x04, 0x06, 0x02,
        0x02, 0x02, 0x06, 0x02, 0x00, 0x06, 0x04, 0x06, 0x00, 0x06, 0x07, 0x00, 0x03, 0x05, 0x03, 0x04, 0x06, 0x03,
        0x05, 0x03, 0x07, 0x05, 0x02, 0x01, 0x00, 0x02, 0x02, 0x00, 0x06, 0x01, 0x03, 0x00, 0x03, 0x01, 0x02, 0x03,
        0x07, 0x07, 0x01, 0x07, 0x05, 0x01, 0x00, 0x00, 0x06, 0x03, 0x04, 0x04, 0x06, 0x02, 0x07, 0x05, 0x07, 0x02,
        0x06, 0x07, 0x04, 0x01, 0x00, 0x03, 0x02, 0x03, 0x03, 0x05, 0x04, 0x06, 0x00, 0x03, 0x05, 0x02, 0x02, 0x02,
        0x03, 0x02, 0x02, 0x01, 0x06, 0x07, 0x06, 0x04, 0x01, 0x05
    };
    size_t original_len = sizeof(original);
    void *compressed = NULL;
    size_t compressed_len = 0;
    void *uncompressed = NULL;
    size_t uncompressed_len = 0;
    int ret = 0;

    ret = flb_gzip_compress(&original, original_len, &compressed, &compressed_len);
    TEST_CHECK(ret == 0);

    ret = flb_gzip_uncompress(compressed, compressed_len, &uncompressed, &uncompressed_len);
    TEST_CHECK(ret == 0);

    TEST_CHECK(uncompressed_len == original_len);

    ret = memcmp(original, uncompressed, original_len);
    TEST_CHECK(ret == 0);

    flb_free(compressed);
    flb_free(uncompressed);
}

/* When compressed the gzip body contains a valid gzip header */
void test_header_in_gzip_body_multi()
{
    char original[] = {
        0x06, 0x03, 0x00, 0x00, 0x07, 0x01, 0x05, 0x04, 0x07, 0x07, 0x02, 0x03, 0x00, 0x01, 0x00, 0x04, 0x06, 0x02,
        0x02, 0x02, 0x06, 0x02, 0x00, 0x06, 0x04, 0x06, 0x00, 0x06, 0x07, 0x00, 0x03, 0x05, 0x03, 0x04, 0x06, 0x03,
        0x05, 0x03, 0x07, 0x05, 0x02, 0x01, 0x00, 0x02, 0x02, 0x00, 0x06, 0x01, 0x03, 0x00, 0x03, 0x01, 0x02, 0x03,
        0x07, 0x07, 0x01, 0x07, 0x05, 0x01, 0x00, 0x00, 0x06, 0x03, 0x04, 0x04, 0x06, 0x02, 0x07, 0x05, 0x07, 0x02,
        0x06, 0x07, 0x04, 0x01, 0x00, 0x03, 0x02, 0x03, 0x03, 0x05, 0x04, 0x06, 0x00, 0x03, 0x05, 0x02, 0x02, 0x02,
        0x03, 0x02, 0x02, 0x01, 0x06, 0x07, 0x06, 0x04, 0x01, 0x05
    };
    size_t original_len = sizeof(original);
    void *compressed = NULL;
    size_t compressed_len = 0;
    void *uncompressed = NULL;
    size_t uncompressed_len = 0;
    size_t in_remaining = 0;
    int ret = 0;

    ret = flb_gzip_compress(&original, original_len, &compressed, &compressed_len);
    TEST_CHECK(ret == 0);

    ret = flb_gzip_uncompress_multi(compressed, compressed_len, &uncompressed, &uncompressed_len, &in_remaining);
    TEST_CHECK(ret == 0);

    TEST_CHECK(in_remaining == 0);
    TEST_CHECK(uncompressed_len == original_len);

    ret = memcmp(original, uncompressed, original_len);
    TEST_CHECK(ret == 0);

    flb_free(compressed);
    flb_free(uncompressed);
}

TEST_LIST = {
    {"compress", test_compress},
    {"compress_multi", test_compress_multi},
    {"compress_large", test_compress_large},
    {"header_in_data", test_header_in_gzip_body},
    {"header_in_data_multi", test_header_in_gzip_body_multi},
    { 0 }
};
