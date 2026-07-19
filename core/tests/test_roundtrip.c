/*
 * Library-level tests for the stegify steganography core.
 *
 * Each test drives stegify_embed/stegify_extract (and, for the file tests,
 * stegify_image_export/stegify_image_load) and checks the outcome. Run with a
 * single test name to execute just that case (used by CTest), or with no
 * arguments to run the whole suite. Exit code is non-zero if any check fails.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stegify/core.h"

static int g_failures;

#define CHECK(cond, msg)                      \
  do {                                        \
    if (!(cond)) {                            \
      fprintf(stderr, "  FAIL: %s\n", (msg)); \
      g_failures++;                           \
    }                                         \
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

/* Append the encoded image bytes to the file passed as ctx. */
static void
file_sink(void *ctx, void *data, int size)
{
  if (size > 0)
    fwrite(data, 1, (size_t)size, (FILE *)ctx);
}

/* Encode img (per img->format) and write it to path. */
static int
save_image(const char *path, stegify_image_t *img)
{
  FILE *file;
  stegify_status_t s;

  file = fopen(path, "wb");
  if (file == NULL)
    return 0;

  s = stegify_image_export(img, file_sink, file);
  fclose(file);
  return s == STEGIFY_OK;
}

/* Read path into memory and decode it into img. */
static int
load_image(const char *path, stegify_image_t *img)
{
  FILE *file;
  long size;
  uint8_t *buffer;
  size_t read_bytes;
  stegify_status_t s;

  file = fopen(path, "rb");
  if (file == NULL)
    return 0;

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return 0;
  }
  size = ftell(file);
  if (size < 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return 0;
  }

  buffer = (uint8_t *)malloc(size == 0 ? 1 : (size_t)size);
  if (buffer == NULL) {
    fclose(file);
    return 0;
  }

  read_bytes = fread(buffer, 1, (size_t)size, file);
  fclose(file);
  if (read_bytes != (size_t)size) {
    free(buffer);
    return 0;
  }

  s = stegify_image_load(buffer, (size_t)size, img);
  free(buffer);
  return s == STEGIFY_OK;
}

static void
test_roundtrip_header(void)
{
  stegify_image_t img;
  const char *payload =
    "Hello, stegify! The quick brown fox jumps over the lazy dog.";
  uint32_t plen;
  uint8_t out[128];
  uint32_t outsize;
  stegify_status_t s;

  img = make_image(64, 64, 3);
  plen = (uint32_t)strlen(payload);
  outsize = sizeof(out);

  s = stegify_embed(&img, (const uint8_t *)payload, plen);
  CHECK(s == STEGIFY_OK, "header embed returns OK");

  s = stegify_extract(&img, out, &outsize);
  CHECK(s == STEGIFY_OK, "header extract returns OK");
  CHECK(outsize == plen, "header extract reports the embedded size");
  CHECK(
    memcmp(out, payload, plen) == 0, "header payload round-trips byte-exact");

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

  s = stegify_embed(&img, payload, (uint32_t)cap);
  CHECK(s == STEGIFY_OK, "embed at exact capacity succeeds");

  s = stegify_embed(&img, payload, (uint32_t)(cap + 1));
  CHECK(s == STEGIFY_ERR_INSUFFICIENT_CAPACITY,
    "embed one byte over capacity is rejected");

  free(payload);
  free_image(&img);
}

static void
test_no_payload_detected(void)
{
  stegify_image_t img;
  uint8_t out[128];
  uint32_t outsize;
  stegify_status_t s;

  /* An image with nothing embedded must be reported as carrying no payload
   * (magic mismatch) rather than yielding random bytes as "data". */
  img = make_image(64, 64, 3);
  outsize = sizeof(out);
  s = stegify_extract(&img, out, &outsize);
  CHECK(
    s == STEGIFY_ERR_CORRUPTED_DATA, "extract without a payload is detected");
  free_image(&img);
}

static void
test_capacity_underflow(void)
{
  stegify_image_t img;

  /* 1x1x3 = 3 bytes: below the header size. Capacity must clamp to 0 rather
   * than wrap around (regression for the size_t underflow). */
  img = make_image(1, 1, 3);
  CHECK(stegify_get_max_capacity(&img) == 0,
    "tiny image capacity is 0 (no underflow)");
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
  s = stegify_embed(&img, &byte, 0);
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
  s = stegify_embed(&img, (const uint8_t *)payload, 1);
  CHECK(s == STEGIFY_ERR_INSUFFICIENT_CAPACITY,
    "tiny image header-mode embed is rejected");

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
  s = stegify_extract(&img, out, &outsize);
  CHECK(s == STEGIFY_ERR_INSUFFICIENT_CAPACITY,
    "tiny image header-mode extract is rejected");

  free_image(&img);
}

static void
test_invalid_input(void)
{
  stegify_image_t img;
  const uint8_t junk[16] = { 0 };
  const uint8_t jpeg[4] = { 0xFF, 0xD8, 0xFF, 0xE0 };
  stegify_status_t s;

  memset(&img, 0, sizeof(img));

  s = stegify_image_load(NULL, 0, &img);
  CHECK(s == STEGIFY_ERR_INVALID_INPUT, "load with a NULL buffer is rejected");

  s = stegify_image_load(junk, 0, &img);
  CHECK(s == STEGIFY_ERR_INVALID_INPUT, "load with a zero length is rejected");

  s = stegify_image_load(junk, sizeof(junk), &img);
  CHECK(s == STEGIFY_ERR_UNSUPPORTED_FORMAT,
    "load of an unrecognised signature is rejected");

  s = stegify_image_load(jpeg, sizeof(jpeg), &img);
  CHECK(s == STEGIFY_ERR_UNSUPPORTED_FORMAT,
    "load of a JPEG signature is rejected (JPEG unsupported)");
}

static void
test_load_errors(void)
{
  stegify_image_t img;
  uint8_t bogus_png[32];

  memset(&img, 0, sizeof(img));

  /* A valid PNG signature followed by a garbage body: recognised as PNG, then
   * stb fails to decode it -> INVALID_IMAGE. */
  memset(bogus_png, 0, sizeof(bogus_png));
  bogus_png[0] = 0x89;
  bogus_png[1] = 0x50;
  bogus_png[2] = 0x4E;
  bogus_png[3] = 0x47;
  bogus_png[4] = 0x0D;
  bogus_png[5] = 0x0A;
  bogus_png[6] = 0x1A;
  bogus_png[7] = 0x0A;

  CHECK(stegify_image_load(bogus_png, sizeof(bogus_png), &img) ==
          STEGIFY_ERR_INVALID_IMAGE,
    "a PNG signature with a garbage body is INVALID_IMAGE");
}

static void
run_file_roundtrip(
  const char *path, stegify_image_format_t fmt, const char *label)
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

  s = stegify_embed(&img, (const uint8_t *)payload, plen);
  snprintf(msg, sizeof(msg), "%s: embed returns OK", label);
  CHECK(s == STEGIFY_OK, msg);

  snprintf(msg, sizeof(msg), "%s: save returns OK", label);
  CHECK(save_image(path, &img), msg);
  free_image(&img);

  memset(&loaded, 0, sizeof(loaded));
  snprintf(msg, sizeof(msg), "%s: load returns OK", label);
  CHECK(load_image(path, &loaded), msg);

  outsize = sizeof(out);
  s = stegify_extract(&loaded, out, &outsize);
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
test_export_format(void)
{
  stegify_image_t img;
  const char *bmp_path = "stegify_test_export.bmp";
  FILE *f;
  unsigned char magic[2];
  stegify_status_t s;

  /* export encodes according to image->format, regardless of how the image was
   * decoded: format BMP must yield a BMP file. */
  img = make_image(16, 16, 3);
  img.format = STEGIFY_FORMAT_BMP;
  CHECK(save_image(bmp_path, &img), "export as BMP succeeds");
  free_image(&img);

  magic[0] = 0;
  magic[1] = 0;
  f = fopen(bmp_path, "rb");
  CHECK(f != NULL, "exported file exists");
  if (f != NULL) {
    fread(magic, 1, sizeof(magic), f);
    fclose(f);
  }
  CHECK(magic[0] == 'B' && magic[1] == 'M',
    "exported file carries a BMP signature");
  remove(bmp_path);

  /* an unknown output format is rejected before any bytes are produced */
  img = make_image(16, 16, 3);
  img.format = STEGIFY_FORMAT_UNKNOWN;
  s = stegify_image_export(&img, file_sink, NULL);
  CHECK(s == STEGIFY_ERR_UNSUPPORTED_FORMAT,
    "export with an unknown format is rejected");
  free_image(&img);
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

static const struct test_case TESTS[] = { { "roundtrip_header",
                                            test_roundtrip_header },
  { "capacity_boundary", test_capacity_boundary },
  { "capacity_underflow", test_capacity_underflow },
  { "no_payload_detected", test_no_payload_detected },
  { "zero_payload", test_zero_payload },
  { "tiny_image_embed", test_tiny_image_embed },
  { "tiny_image_extract", test_tiny_image_extract },
  { "invalid_input", test_invalid_input }, { "load_errors", test_load_errors },
  { "export_format", test_export_format }, { "png_file", test_png_file },
  { "bmp_file", test_bmp_file } };

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
