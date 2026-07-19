#include <stdio.h>
#include <string.h>

#include "stegify.h"
#include "stb_image.h"
#include "stb_image_write.h"

#define BITS_IN_BYTE (8)

/* Upper bound on a decoded image, so a small but highly compressible file
 * cannot force a huge allocation. Override with -DSTEGIFY_MAX_IMAGE_BYTES. */
#ifndef STEGIFY_MAX_IMAGE_BYTES
#define STEGIFY_MAX_IMAGE_BYTES ((size_t)256 * 1024 * 1024)
#endif

/*
 * In size-header mode the payload is preceded by a fixed header:
 *   4-byte magic "STGF" | 1-byte format version | 4-byte payload size.
 * The magic lets extract detect that an image carries no stegify payload
 * instead of returning random bytes as if they were data.
 */
#define STEGIFY_MAGIC_LEN 4
#define STEGIFY_FORMAT_VERSION 1
#define STEGIFY_HEADER_BYTES (STEGIFY_MAGIC_LEN + 1 + sizeof(uint32_t))

static const uint8_t STEGIFY_MAGIC[STEGIFY_MAGIC_LEN] = { 'S', 'T', 'G', 'F' };

typedef struct {
  size_t total_bytes;
  size_t step;
  size_t current_pos;
} stegify_position_iter_t;

static stegify_image_format_t
stegify_format_from_path(const char *filepath)
{
  const char *ext;

  ext = strrchr(filepath, '.');

  if (ext == NULL)
    return STEGIFY_FORMAT_PNG;

  ext++;

  if (strcmp(ext, "png") == 0 || strcmp(ext, "PNG") == 0)
    return STEGIFY_FORMAT_PNG;

  if (strcmp(ext, "bmp") == 0 || strcmp(ext, "BMP") == 0)
    return STEGIFY_FORMAT_BMP;

  return STEGIFY_FORMAT_UNKNOWN;
}

stegify_status_t
stegify_image_load(
  const char *filepath,
  stegify_image_t *image)
{
  int width;
  int height;
  int channels;
  stbi_uc *pixels;
  stegify_image_format_t format;
  FILE *probe;

  if (filepath == NULL || image == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  format = stegify_format_from_path(filepath);
  if (format == STEGIFY_FORMAT_UNKNOWN)
    return STEGIFY_ERR_UNSUPPORTED_FORMAT;

  /* stbi_load returns NULL for both a missing file and an unreadable image,
   * so probe the file first to tell "cannot open" from "not a valid image". */
  probe = fopen(filepath, "rb");
  if (probe == NULL)
    return STEGIFY_ERR_FILE_IO;
  fclose(probe);

  width = 0;
  height = 0;
  channels = 0;

  pixels = stbi_load(filepath, &width, &height, &channels, 0);
  if (pixels == NULL)
    return STEGIFY_ERR_INVALID_IMAGE;

  if ((size_t)width * height * channels > STEGIFY_MAX_IMAGE_BYTES) {
    stbi_image_free(pixels);
    return STEGIFY_ERR_INVALID_IMAGE;
  }

  image->data = (uint8_t *)pixels;
  image->width = (uint32_t)width;
  image->height = (uint32_t)height;
  image->channels = (uint8_t)channels;
  image->format = format;

  return STEGIFY_OK;
}

void
stegify_image_free(stegify_image_t *image)
{
  if (image == NULL || image->data == NULL)
    return;

  stbi_image_free((void *)image->data);
  image->data = NULL;
}

stegify_status_t
stegify_image_save(
  const char *filepath,
  const stegify_image_t *image)
{
  int written;
  int stride;
  stegify_image_format_t out_format;

  if (filepath == NULL || image == NULL ||
      image->data == NULL || image->channels == 0)
    return STEGIFY_ERR_INVALID_INPUT;

  /* The encoder is chosen from the output path, not from image->format (which
   * only records how the image was decoded), so the output extension is
   * honoured. */
  out_format = stegify_format_from_path(filepath);
  stride = (int)(image->width * image->channels);

  switch (out_format)
  {
    case STEGIFY_FORMAT_PNG:
      written = stbi_write_png(filepath, (int)image->width,
        (int)image->height, (int)image->channels, image->data, stride);
      break;
    case STEGIFY_FORMAT_BMP:
      written = stbi_write_bmp(filepath, (int)image->width,
        (int)image->height, (int)image->channels, image->data);
      break;
    default:
      return STEGIFY_ERR_UNSUPPORTED_FORMAT;
  }

  return written == 0 ? STEGIFY_ERR_FILE_IO : STEGIFY_OK;
}

size_t
stegify_get_max_capacity(const stegify_image_t *image, int attributes)
{
  size_t total_bytes;
  size_t bits_capacity;

  if (image == NULL || image->data == NULL)
    return 0;

  total_bytes = (size_t)image->width * image->height * image->channels;
  bits_capacity = total_bytes / BITS_IN_BYTE;

  if (attributes & STEGIFY_ATTR_WITH_SIZE) {
    if (bits_capacity < STEGIFY_HEADER_BYTES)
      return 0;
    return bits_capacity - STEGIFY_HEADER_BYTES;
  }

  return bits_capacity;
}

static void
embed_bit(uint8_t *target, uint8_t bit)
{
  *target = (*target & 0xFE) | (bit & 0x01);
}

static uint8_t
extract_bit(uint8_t source)
{
  return source & 0x01;
}

static void
stegify_iter_init(stegify_position_iter_t *iter)
{
  /* Starts one before position 0; the first stegify_iter_next() wraps to 0. */
  iter->current_pos = (size_t)-1;
  iter->step = 1;
}

static size_t
stegify_iter_next(stegify_position_iter_t *iter)
{
    iter->current_pos += iter->step;
    return iter->current_pos;
}

static void
stegify_write_buffer_to_image_lsb(
  const uint8_t *buffer,
  size_t buffer_size,
  uint8_t *image_data,
  stegify_position_iter_t *iter)
{
  size_t byte_idx;
  uint8_t bit;
  int bit_idx;
  size_t pos;

  for (byte_idx = 0; byte_idx < buffer_size; byte_idx++) {
    for (bit_idx = 0; bit_idx < BITS_IN_BYTE; bit_idx++) {
      bit = (buffer[byte_idx] >> bit_idx) & 0x01;
      pos = stegify_iter_next(iter);
      embed_bit(&image_data[pos], bit);
    }
  }
}

stegify_status_t
stegify_embed(
    stegify_image_t *image,
    const uint8_t *data,
    uint32_t data_size,
    int attributes)
{
  size_t max_capacity;
  uint8_t version;
  stegify_position_iter_t iter;

  if (image == NULL || image->data == NULL || data == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  if (data_size == 0)
    return STEGIFY_ERR_INVALID_INPUT;

  max_capacity = stegify_get_max_capacity(image, attributes);

  if (data_size > max_capacity)
    return STEGIFY_ERR_INSUFFICIENT_CAPACITY;

  stegify_iter_init(&iter);

  if (attributes & STEGIFY_ATTR_WITH_SIZE) {
    version = STEGIFY_FORMAT_VERSION;
    stegify_write_buffer_to_image_lsb(STEGIFY_MAGIC, STEGIFY_MAGIC_LEN, image->data, &iter);
    stegify_write_buffer_to_image_lsb(&version, 1, image->data, &iter);
    stegify_write_buffer_to_image_lsb((uint8_t *)&data_size, sizeof(data_size), image->data, &iter);
  }

  stegify_write_buffer_to_image_lsb(data, data_size, image->data, &iter);

  return STEGIFY_OK;
}

static void
stegify_read_buffer_from_image_lsb(
  uint8_t *buffer,
  size_t buffer_size,
  uint8_t *image_data,
  stegify_position_iter_t *iter)
{
  size_t byte_idx;
  uint8_t bit;
  int bit_idx;
  size_t pos;
  uint8_t byte;

  for (byte_idx = 0; byte_idx < buffer_size; byte_idx++) {

    byte = 0;

    for (bit_idx = 0; bit_idx < BITS_IN_BYTE; bit_idx++) {
      pos = stegify_iter_next(iter);
      bit = extract_bit(image_data[pos]);
      byte |= (bit << bit_idx);
    }

    buffer[byte_idx] = byte;
  }
}

stegify_status_t
stegify_extract(
  const stegify_image_t *image,
  uint8_t *data,
  uint32_t *data_size,
  int attributes)
{
  size_t total_bytes;
  size_t required_bits;
  size_t out_buffer_size;
  uint8_t magic[STEGIFY_MAGIC_LEN];
  uint8_t version;
  stegify_position_iter_t iter;

  if (image == NULL || image->data == NULL || data == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  if (data_size == NULL || *data_size == 0)
    return STEGIFY_ERR_INVALID_INPUT;

  stegify_iter_init(&iter);
    
  out_buffer_size = *data_size;
  total_bytes = (size_t)image->width * image->height * image->channels;

  /* The header occupies one LSB per image byte; refuse to read it from an
   * image too small to hold it, otherwise the read runs past the buffer. */
  if ((attributes & STEGIFY_ATTR_WITH_SIZE) &&
      total_bytes < STEGIFY_HEADER_BYTES * BITS_IN_BYTE)
    return STEGIFY_ERR_INSUFFICIENT_CAPACITY;

  if (attributes & STEGIFY_ATTR_WITH_SIZE) {
    stegify_read_buffer_from_image_lsb(magic, STEGIFY_MAGIC_LEN, image->data, &iter);
    if (memcmp(magic, STEGIFY_MAGIC, STEGIFY_MAGIC_LEN) != 0)
      return STEGIFY_ERR_CORRUPTED_DATA;

    stegify_read_buffer_from_image_lsb(&version, 1, image->data, &iter);
    if (version != STEGIFY_FORMAT_VERSION)
      return STEGIFY_ERR_CORRUPTED_DATA;

    stegify_read_buffer_from_image_lsb((uint8_t *)data_size, sizeof(*data_size), image->data, &iter);
  }

  if (*data_size > out_buffer_size)
    return STEGIFY_ERR_INSUFFICIENT_CAPACITY;

  required_bits = (size_t)(*data_size) * BITS_IN_BYTE;
  if (attributes & STEGIFY_ATTR_WITH_SIZE)
    required_bits += STEGIFY_HEADER_BYTES * BITS_IN_BYTE;

  if (required_bits > total_bytes)
    return STEGIFY_ERR_INSUFFICIENT_CAPACITY;

  stegify_read_buffer_from_image_lsb(data, *data_size, image->data, &iter);

  return STEGIFY_OK;
}

const char *
stegify_error_string(stegify_status_t status)
{
  switch (status) {
  case STEGIFY_OK:
    return "ok";
  case STEGIFY_ERR_INVALID_INPUT:
    return "invalid input";
  case STEGIFY_ERR_INVALID_IMAGE:
    return "invalid image";
  case STEGIFY_ERR_UNSUPPORTED_FORMAT:
    return "unsupported format";
  case STEGIFY_ERR_INSUFFICIENT_CAPACITY:
    return "insufficient capacity";
  case STEGIFY_ERR_FILE_IO:
    return "file i/o error";
  case STEGIFY_ERR_CORRUPTED_DATA:
    return "corrupted data";
  default:
    return "unknown error";
  }
}
