/*
 * Tests for the stegify operations layer. These exercise the path-based
 * file workflows (embed, embed-from-file, extract, capacity) that the CLI and
 * any future frontend build on. Run a single test by name, or with no
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

/* Append the encoded image bytes to the file passed as ctx. */
static void
file_sink(void *ctx, void *data, int size)
{
  if (size > 0)
    fwrite(data, 1, (size_t)size, (FILE *)ctx);
}

/* Create a small cover PNG on disk for the workflow tests. */
static int
make_cover(const char *path)
{
  stegify_image_t img;
  size_t n = 64 * 64 * 3;
  size_t i;
  FILE *file;
  stegify_status_t s;

  memset(&img, 0, sizeof(img));
  img.data = (uint8_t *)malloc(n);
  img.width = 64;
  img.height = 64;
  img.channels = 3;
  img.format = STEGIFY_FORMAT_PNG;
  for (i = 0; i < n; i++)
    img.data[i] = (uint8_t)(i * 31u + 7u);

  file = fopen(path, "wb");
  if (file == NULL) {
    free(img.data);
    return 0;
  }

  s = stegify_image_export(&img, file_sink, file);
  fclose(file);
  free(img.data);
  return s == STEGIFY_OK;
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

  s = stegify_ops_extract(stego, NULL, &out, &out_size);
  CHECK(s == STEGIFY_OK, "ops_extract returns OK");
  CHECK(out_size == payload_size, "extracted size matches");
  CHECK(memcmp(out, payload, payload_size) == 0,
    "payload round-trips through files");
  free(out);

  remove(cover);
  remove(stego);
}

/* embed a payload file, extract it back to another file, compare byte-for-byte
 * -- exercises the internal read/write helpers end to end. */
static void
test_ops_file_workflow(void)
{
  const char *cover = "stegify_ops_fw_cover.png";
  const char *data = "stegify_ops_fw_data.bin";
  const char *stego = "stegify_ops_fw_stego.png";
  const char *recovered = "stegify_ops_fw_out.bin";
  const uint8_t payload[] = { 'o', 'p', 's', 0, 1, 2, 250, 128 };
  size_t embedded = 0;
  size_t remaining = 0;
  uint32_t out_size = 0;
  FILE *file;
  uint8_t got[32];
  size_t got_n;
  stegify_status_t s;

  CHECK(make_cover(cover), "created cover image");

  file = fopen(data, "wb");
  CHECK(file != NULL, "created payload file");
  if (file != NULL) {
    fwrite(payload, 1, sizeof(payload), file);
    fclose(file);
  }

  s = stegify_ops_embed_file(cover, data, stego, &embedded, &remaining);
  CHECK(s == STEGIFY_OK, "ops_embed_file returns OK");
  CHECK(embedded == sizeof(payload), "ops_embed_file reports the payload size");

  s = stegify_ops_extract(stego, recovered, NULL, &out_size);
  CHECK(s == STEGIFY_OK, "ops_extract to a file returns OK");
  CHECK(out_size == sizeof(payload), "extracted size matches");

  got_n = 0;
  file = fopen(recovered, "rb");
  CHECK(file != NULL, "opened the recovered file");
  if (file != NULL) {
    got_n = fread(got, 1, sizeof(got), file);
    fclose(file);
  }
  CHECK(got_n == sizeof(payload), "recovered file length matches");
  CHECK(memcmp(got, payload, sizeof(payload)) == 0,
    "payload round-trips through files");

  remove(cover);
  remove(data);
  remove(stego);
  remove(recovered);
}

/* The output extension selects the encoder, and an unsupported one is rejected
 * before anything is written. */
static void
test_ops_output_format(void)
{
  const char *cover = "stegify_ops_of_cover.png";
  const char *stego = "stegify_ops_of_stego.bmp";
  const char *payload = "pick the encoder from the .bmp output";
  FILE *file;
  unsigned char magic[2];
  stegify_status_t s;

  CHECK(make_cover(cover), "created cover image");

  s = stegify_ops_embed(
    cover, (const uint8_t *)payload, strlen(payload), stego, NULL);
  CHECK(s == STEGIFY_OK, "ops_embed to a .bmp output succeeds");

  magic[0] = 0;
  magic[1] = 0;
  file = fopen(stego, "rb");
  CHECK(file != NULL, "stego file exists");
  if (file != NULL) {
    fread(magic, 1, sizeof(magic), file);
    fclose(file);
  }
  CHECK(magic[0] == 'B' && magic[1] == 'M',
    "the output extension selected the BMP encoder");

  s = stegify_ops_embed(cover, (const uint8_t *)payload, strlen(payload),
    "stegify_ops_of_out.jpg", NULL);
  CHECK(s == STEGIFY_ERR_UNSUPPORTED_FORMAT,
    "an unsupported output extension is rejected");

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
  s = stegify_ops_extract(cover, NULL, &out, &out_size);
  CHECK(s == STEGIFY_ERR_CORRUPTED_DATA,
    "extract from an image with no payload is CORRUPTED_DATA");
  remove(cover);
}

typedef void (*test_fn)(void);

struct test_case {
  const char *name;
  test_fn fn;
};

static const struct test_case TESTS[] = { { "ops_roundtrip",
                                            test_ops_roundtrip },
  { "ops_file_workflow", test_ops_file_workflow },
  { "ops_output_format", test_ops_output_format },
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
