/*
 * Library-level tests for the stegify steganography core.
 *
 * Each test drives stegify_embed/stegify_extract (and, for the file tests,
 * stegify_image_save/stegify_image_load) and checks the outcome. Run with a
 * single test name to execute just that case (used by CTest), or with no
 * arguments to run the whole suite. Exit code is non-zero if any check fails.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stegify.h"

static int g_failures;

#define CHECK(cond, msg) \
  do { \
    if (!(cond)) { \
      fprintf(stderr, "  FAIL: %s\n", (msg)); \
      g_failures++; \
    } \
  } while (0)

static stegify_image_t
make_image(uint32_t w, uint32_t h, uint8_t c)
{
  stegify_image_t img;
  size_t n;
  size_t i;

  memset(&img, 0, sizeof(img));
  n = (size_t)w * h * c;
  img.data = (uint8_t *)malloc(n ? n : 1);
  img.width = w;
  img.height = h;
  img.channels = c;
  img.format = STEGIFY_FORMAT_PNG;

  for (i = 0; i < n; i++)
    img.data[i] = (uint8_t)(i * 31u + 7u);

  return img;
}

static void
free_image(stegify_image_t *img)
{
  free(img->data);
  img->data = NULL;
}

static void
test_roundtrip_header(void)
{
  stegify_image_t img;
  const char *payload = "Hello, stegify! The quick brown fox jumps over the lazy dog.";
  uint32_t plen;
  uint8_t out[128];
  uint32_t outsize;
  stegify_status_t s;

  img = make_image(64, 64, 3);
  plen = (uint32_t)strlen(payload);
  outsize = sizeof(out);

  s = stegify_embed(&img, (const uint8_t *)payload, plen, STEGIFY_ATTR_WITH_SIZE);
  CHECK(s == STEGIFY_OK, "header embed returns OK");

  s = stegify_extract(&img, out, &outsize, STEGIFY_ATTR_WITH_SIZE);
  CHECK(s == STEGIFY_OK, "header extract returns OK");
  CHECK(outsize == plen, "header extract reports the embedded size");
  CHECK(memcmp(out, payload, plen) == 0, "header payload round-trips byte-exact");

  free_image(&img);
}

static void
test_roundtrip_noheader(void)
{
  stegify_image_t img;
  const char *payload = "no-header payload";
  uint32_t plen;
  uint8_t out[64];
  uint32_t outsize;
  stegify_status_t s;

  img = make_image(64, 64, 3);
  plen = (uint32_t)strlen(payload);
  outsize = plen; /* caller must supply the exact length in no-header mode */

  s = stegify_embed(&img, (const uint8_t *)payload, plen, 0);
  CHECK(s == STEGIFY_OK, "no-header embed returns OK");

  s = stegify_extract(&img, out, &outsize, 0);
  CHECK(s == STEGIFY_OK, "no-header extract returns OK");
  CHECK(outsize == plen, "no-header extract preserves the requested size");
  CHECK(memcmp(out, payload, plen) == 0, "no-header payload round-trips byte-exact");

  free_image(&img);
}

static void
test_capacity_boundary(void)
{
  stegify_image_t img;
  size_t cap;
  size_t i;
  uint8_t *payload;
  stegify_status_t s;

  img = make_image(64, 64, 3);
  cap = stegify_get_max_capacity(&img);
  payload = (uint8_t *)malloc(cap + 1);
  for (i = 0; i < cap + 1; i++)
    payload[i] = (uint8_t)i;

  s = stegify_embed(&img, payload, (uint32_t)cap, STEGIFY_ATTR_WITH_SIZE);
  CHECK(s == STEGIFY_OK, "embed at exact capacity succeeds");

  s = stegify_embed(&img, payload, (uint32_t)(cap + 1), STEGIFY_ATTR_WITH_SIZE);
  CHECK(s == STEGIFY_ERR_INSUFFICIENT_CAPACITY, "embed one byte over capacity is rejected");

  free(payload);
  free_image(&img);
}

static void
test_zero_payload(void)
{
  stegify_image_t img;
  uint8_t byte;
  stegify_status_t s;

  img = make_image(16, 16, 3);
  byte = 0;
  s = stegify_embed(&img, &byte, 0, STEGIFY_ATTR_WITH_SIZE);
  CHECK(s == STEGIFY_ERR_INVALID_INPUT, "zero-length embed is rejected");

  free_image(&img);
}

static void
test_tiny_image_embed(void)
{
  stegify_image_t img;
  const char *payload = "x";
  stegify_status_t s;

  /* 2x2x1 = 4 bytes -> total/8 = 0, cannot even fit the 4-byte size header. */
  img = make_image(2, 2, 1);
  s = stegify_embed(&img, (const uint8_t *)payload, 1, STEGIFY_ATTR_WITH_SIZE);
  CHECK(s == STEGIFY_ERR_INSUFFICIENT_CAPACITY, "tiny image header-mode embed is rejected");

  free_image(&img);
}

static void
test_tiny_image_extract(void)
{
  stegify_image_t img;
  uint8_t out[16];
  uint32_t outsize;
  stegify_status_t s;

  /* 2x2x1 = 4 bytes cannot hold the 4-byte size header; extract must reject
   * it up front rather than read past the image buffer (regression for the
   * out-of-bounds header read). */
  img = make_image(2, 2, 1);
  outsize = sizeof(out);
  s = stegify_extract(&img, out, &outsize, STEGIFY_ATTR_WITH_SIZE);
  CHECK(s == STEGIFY_ERR_INSUFFICIENT_CAPACITY, "tiny image header-mode extract is rejected");

  free_image(&img);
}

static void
test_invalid_input(void)
{
  stegify_image_t img;
  stegify_status_t s;

  memset(&img, 0, sizeof(img));
  s = stegify_image_load(NULL, &img);
  CHECK(s == STEGIFY_ERR_INVALID_INPUT, "load with NULL path is rejected");

  s = stegify_image_load("does_not_exist.unknownext", &img);
  CHECK(s == STEGIFY_ERR_UNSUPPORTED_FORMAT, "load with an unknown extension is rejected");

  s = stegify_image_load("does_not_exist.jpg", &img);
  CHECK(s == STEGIFY_ERR_UNSUPPORTED_FORMAT, "load of a .jpg is rejected (JPEG unsupported)");
}

static void
run_file_roundtrip(const char *path, stegify_image_format_t fmt, const char *label)
{
  stegify_image_t img;
  stegify_image_t loaded;
  const char *payload = "file round-trip payload 0123456789";
  uint32_t plen;
  uint8_t out[128];
  uint32_t outsize;
  stegify_status_t s;
  char msg[160];

  img = make_image(48, 32, 3);
  img.format = fmt;
  plen = (uint32_t)strlen(payload);

  s = stegify_embed(&img, (const uint8_t *)payload, plen, STEGIFY_ATTR_WITH_SIZE);
  snprintf(msg, sizeof(msg), "%s: embed returns OK", label);
  CHECK(s == STEGIFY_OK, msg);

  s = stegify_image_save(path, &img);
  snprintf(msg, sizeof(msg), "%s: save returns OK", label);
  CHECK(s == STEGIFY_OK, msg);
  free_image(&img);

  memset(&loaded, 0, sizeof(loaded));
  s = stegify_image_load(path, &loaded);
  snprintf(msg, sizeof(msg), "%s: load returns OK", label);
  CHECK(s == STEGIFY_OK, msg);

  outsize = sizeof(out);
  s = stegify_extract(&loaded, out, &outsize, STEGIFY_ATTR_WITH_SIZE);
  snprintf(msg, sizeof(msg), "%s: extract returns OK", label);
  CHECK(s == STEGIFY_OK, msg);
  snprintf(msg, sizeof(msg), "%s: extracted size matches", label);
  CHECK(outsize == plen, msg);
  snprintf(msg, sizeof(msg), "%s: payload survives save/load", label);
  CHECK(memcmp(out, payload, plen) == 0, msg);

  stegify_image_free(&loaded);
  remove(path);
}

static void
test_png_file(void)
{
  run_file_roundtrip("stegify_test_tmp.png", STEGIFY_FORMAT_PNG, "png");
}

static void
test_bmp_file(void)
{
  run_file_roundtrip("stegify_test_tmp.bmp", STEGIFY_FORMAT_BMP, "bmp");
}

typedef void (*test_fn)(void);

struct test_case {
  const char *name;
  test_fn fn;
};

static const struct test_case TESTS[] = {
  { "roundtrip_header", test_roundtrip_header },
  { "roundtrip_noheader", test_roundtrip_noheader },
  { "capacity_boundary", test_capacity_boundary },
  { "zero_payload", test_zero_payload },
  { "tiny_image_embed", test_tiny_image_embed },
  { "tiny_image_extract", test_tiny_image_extract },
  { "invalid_input", test_invalid_input },
  { "png_file", test_png_file },
  { "bmp_file", test_bmp_file }
};

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
