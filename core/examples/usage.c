/*
 * Minimal example of the stegify library API. This file is compiled by the
 * build to keep the README example honest; it is illustrative and not run as
 * a test (it expects a cover.png / stego.png on disk).
 */

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stegify/core.h"

static int
embed_example(void)
{
  stegify_image_t image;
  const char *payload = "hidden message";
  uint32_t payload_size = (uint32_t)strlen(payload);
  stegify_status_t status;

  if (stegify_image_load("cover.png", &image) != STEGIFY_OK)
    return 1;

  if (payload_size > stegify_get_max_capacity(&image, STEGIFY_ATTR_WITH_SIZE)) {
    stegify_image_free(&image);
    return 1;
  }

  status = stegify_embed(
    &image, (const uint8_t *)payload, payload_size, STEGIFY_ATTR_WITH_SIZE);
  if (status == STEGIFY_OK)
    status = stegify_image_save("stego.png", &image);

  stegify_image_free(&image);
  return status == STEGIFY_OK ? 0 : 1;
}

static int
extract_example(void)
{
  stegify_image_t image;
  uint8_t buffer[4096];
  uint32_t size =
    sizeof(buffer); /* in: buffer capacity, out: bytes extracted */
  stegify_status_t status;

  if (stegify_image_load("stego.png", &image) != STEGIFY_OK)
    return 1;

  status = stegify_extract(&image, buffer, &size, STEGIFY_ATTR_WITH_SIZE);
  stegify_image_free(&image);

  if (status != STEGIFY_OK) {
    fprintf(stderr, "extract failed: %s\n", stegify_error_string(status));
    return 1;
  }

  fwrite(buffer, 1, size, stdout);
  return 0;
}

int
main(void)
{
  return embed_example() + extract_example();
}
