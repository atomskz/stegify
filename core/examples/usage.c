/*
 * Minimal example of the stegify library API. This file is compiled by the
 * build to keep the README example honest; it is illustrative and not run as
 * a test (it expects a cover.png on disk).
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stegify/ops.h"

static int
embed_example(void)
{
  const char *payload = "hidden message";
  stegify_status_t status;

  status = stegify_ops_embed(
    "cover.png", (const uint8_t *)payload, strlen(payload), "stego.png", NULL);
  return status == STEGIFY_OK ? 0 : 1;
}

static int
extract_example(void)
{
  uint8_t *payload;
  uint32_t size;
  stegify_status_t status;

  status = stegify_ops_extract("stego.png", NULL, &payload, &size);
  if (status != STEGIFY_OK) {
    fprintf(stderr, "extract failed: %s\n", stegify_error_string(status));
    return 1;
  }

  fwrite(payload, 1, size, stdout);
  free(payload);
  return 0;
}

int
main(void)
{
  return embed_example() + extract_example();
}
