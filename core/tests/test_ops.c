/*
 * Tests for the stegify operations layer. These exercise the
 * file-level workflows (read/write file, embed, extract, capacity) that the
 * CLI and any future frontend build on. Run a single test by name, or with no
 * arguments to run the whole suite.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stegify/core.h"
#include "stegify/ops.h"

static int g_failures;

#define CHECK(cond, msg)                      \
  do {                                        \
    if (!(cond)) {                            \
      fprintf(stderr, "  FAIL: %s\n", (msg)); \
      g_failures++;                           \
    }                                         \
  } while (0)

/* Create a small cover PNG on disk for the workflow tests. */
static int
make_cover(const char *path)
{
  stegify_image_t img;
  size_t n = 64 * 64 * 3;
  size_t i;
  stegify_status_t s;

  memset(&img, 0, sizeof(img));
  img.data = (uint8_t *)malloc(n);
  img.width = 64;
  img.height = 64;
  img.channels = 3;
  img.format = STEGIFY_FORMAT_PNG;
  for (i = 0; i < n; i++)
    img.data[i] = (uint8_t)(i * 31u + 7u);

  s = stegify_image_save(path, &img);
  free(img.data);
  return s == STEGIFY_OK;
}

static void
test_file_io(void)
{
  const char *path = "stegify_ops_test.bin";
  const uint8_t data[] = { 1, 2, 3, 4, 5, 250, 128, 0 };
  uint8_t *read_back;
  size_t size;
  stegify_status_t s;

  s = stegify_write_file(path, data, sizeof(data));
  CHECK(s == STEGIFY_OK, "write_file returns OK");

  s = stegify_read_file(path, &read_back, &size);
  CHECK(s == STEGIFY_OK, "read_file returns OK");
  CHECK(size == sizeof(data), "read_file reports the right size");
  CHECK(memcmp(read_back, data, sizeof(data)) == 0,
    "read_file returns the bytes written");
  free(read_back);
  remove(path);

  s = stegify_read_file("stegify_ops_missing.bin", &read_back, &size);
  CHECK(s == STEGIFY_ERR_FILE_IO, "read_file of a missing file is FILE_IO");
}

static void
test_ops_roundtrip(void)
{
  const char *cover = "stegify_ops_cover.png";
  const char *stego = "stegify_ops_stego.png";
  const char *payload = "application layer payload";
  size_t payload_size = strlen(payload);
  size_t remaining;
  uint8_t *out;
  uint32_t out_size;
  stegify_status_t s;

  CHECK(make_cover(cover), "created cover image");

  remaining = 0;
  s = stegify_ops_embed(
    cover, (const uint8_t *)payload, payload_size, stego, &remaining);
  CHECK(s == STEGIFY_OK, "ops_embed returns OK");
  CHECK(remaining > 0, "ops_embed reports remaining capacity");

  s = stegify_ops_extract(stego, &out, &out_size);
  CHECK(s == STEGIFY_OK, "ops_extract returns OK");
  CHECK(out_size == payload_size, "extracted size matches");
  CHECK(memcmp(out, payload, payload_size) == 0,
    "payload round-trips through files");
  free(out);

  remove(cover);
  remove(stego);
}

static void
test_ops_capacity(void)
{
  const char *cover = "stegify_ops_cap.png";
  size_t cap = 0;
  stegify_status_t s;

  CHECK(make_cover(cover), "created cover image");
  s = stegify_ops_capacity(cover, &cap);
  CHECK(s == STEGIFY_OK, "ops_capacity returns OK");
  CHECK(cap > 0, "capacity is non-zero");
  remove(cover);

  s = stegify_ops_capacity("stegify_ops_missing.png", &cap);
  CHECK(s == STEGIFY_ERR_FILE_IO, "capacity of a missing file is FILE_IO");
}

static void
test_ops_no_payload(void)
{
  const char *cover = "stegify_ops_nopayload.png";
  uint8_t *out;
  uint32_t out_size;
  stegify_status_t s;

  CHECK(make_cover(cover), "created cover image");
  s = stegify_ops_extract(cover, &out, &out_size);
  CHECK(s == STEGIFY_ERR_CORRUPTED_DATA,
    "extract from an image with no payload is CORRUPTED_DATA");
  remove(cover);
}

typedef void (*test_fn)(void);

struct test_case {
  const char *name;
  test_fn fn;
};

static const struct test_case TESTS[] = { { "file_io", test_file_io },
  { "ops_roundtrip", test_ops_roundtrip },
  { "ops_capacity", test_ops_capacity },
  { "ops_no_payload", test_ops_no_payload } };

int
main(int argc, char **argv)
{
  size_t count = sizeof(TESTS) / sizeof(TESTS[0]);
  size_t i;

  if (argc >= 2) {
    for (i = 0; i < count; i++) {
      if (strcmp(argv[1], TESTS[i].name) == 0) {
        TESTS[i].fn();
        if (g_failures == 0)
          printf("PASS: %s\n", argv[1]);
        return g_failures == 0 ? 0 : 1;
      }
    }
    fprintf(stderr, "unknown test: %s\n", argv[1]);
    return 2;
  }

  for (i = 0; i < count; i++) {
    printf("[ %s ]\n", TESTS[i].name);
    TESTS[i].fn();
  }

  if (g_failures == 0)
    printf("\nall checks passed\n");
  else
    printf("\n%d checks FAILED\n", g_failures);

  return g_failures == 0 ? 0 : 1;
}
