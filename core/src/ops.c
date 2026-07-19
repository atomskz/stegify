#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stegify/ops.h"

/*
 * Read an entire file into a newly allocated buffer. On success *buffer is
 * owned by the caller (free with free()) and *size is its length. Returns
 * STEGIFY_OK, INVALID_INPUT, FILE_IO (cannot open/read), or MEMORY_ALLOC.
 */
static stegify_status_t
read_file(const char *path, uint8_t **buffer, size_t *size)
{
  FILE *file;
  long file_size;
  size_t read_bytes;
  uint8_t *data;

  if (path == NULL || buffer == NULL || size == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  file = fopen(path, "rb");
  if (file == NULL)
    return STEGIFY_ERR_FILE_IO;

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return STEGIFY_ERR_FILE_IO;
  }

  file_size = ftell(file);
  if (file_size < 0 || fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return STEGIFY_ERR_FILE_IO;
  }

  data = (uint8_t *)malloc(file_size == 0 ? 1 : (size_t)file_size);
  if (data == NULL) {
    fclose(file);
    return STEGIFY_ERR_MEMORY_ALLOC;
  }

  read_bytes = fread(data, 1, (size_t)file_size, file);
  fclose(file);

  if (read_bytes != (size_t)file_size) {
    free(data);
    return STEGIFY_ERR_FILE_IO;
  }

  *buffer = data;
  *size = (size_t)file_size;
  return STEGIFY_OK;
}

/*
 * Write size bytes to path (overwriting any existing file). Returns STEGIFY_OK,
 * INVALID_INPUT, or FILE_IO.
 */
static stegify_status_t
write_file(const char *path, const uint8_t *data, size_t size)
{
  FILE *file;
  size_t written;

  if (path == NULL || data == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  file = fopen(path, "wb");
  if (file == NULL)
    return STEGIFY_ERR_FILE_IO;

  written = fwrite(data, 1, size, file);

  if (fclose(file) != 0)
    return STEGIFY_ERR_FILE_IO;

  return written == size ? STEGIFY_OK : STEGIFY_ERR_FILE_IO;
}

/* Map an output path's extension to the encoder to use for it. A missing
 * extension defaults to PNG; an unrecognised one yields UNKNOWN. */
static stegify_image_format_t
format_from_path(const char *path)
{
  const char *ext;

  ext = strrchr(path, '.');
  if (ext == NULL)
    return STEGIFY_FORMAT_PNG;

  ext++;

  if (strcmp(ext, "png") == 0 || strcmp(ext, "PNG") == 0)
    return STEGIFY_FORMAT_PNG;

  if (strcmp(ext, "bmp") == 0 || strcmp(ext, "BMP") == 0)
    return STEGIFY_FORMAT_BMP;

  return STEGIFY_FORMAT_UNKNOWN;
}

/* Sink that appends the encoded image bytes to an open file, remembering the
 * first write error (stb does not propagate the callback's outcome). */
typedef struct {
  FILE *file;
  int error;
} file_sink_t;

static void
file_sink_write(void *ctx, void *data, int size)
{
  file_sink_t *sink = (file_sink_t *)ctx;

  if (sink->error || size <= 0)
    return;

  if (fwrite(data, 1, (size_t)size, sink->file) != (size_t)size)
    sink->error = 1;
}

/* Read and decode the image at image_path into image. */
static stegify_status_t
load_image_from_path(const char *image_path, stegify_image_t *image)
{
  uint8_t *buffer;
  size_t size;
  stegify_status_t status;

  status = read_file(image_path, &buffer, &size);
  if (status != STEGIFY_OK)
    return status;

  status = stegify_image_load(buffer, size, image);
  free(buffer);
  return status;
}

/* Encode image and write it to output_path, choosing the encoder from the
 * output extension. */
static stegify_status_t
save_image_to_path(stegify_image_t *image, const char *output_path)
{
  stegify_image_format_t format;
  file_sink_t sink;
  stegify_status_t status;

  format = format_from_path(output_path);
  if (format == STEGIFY_FORMAT_UNKNOWN)
    return STEGIFY_ERR_UNSUPPORTED_FORMAT;

  sink.file = fopen(output_path, "wb");
  if (sink.file == NULL)
    return STEGIFY_ERR_FILE_IO;
  sink.error = 0;

  /* The output extension selects the encoder, independent of how the image was
   * decoded. */
  image->format = format;
  status = stegify_image_export(image, file_sink_write, &sink);

  if (fclose(sink.file) != 0 && status == STEGIFY_OK)
    status = STEGIFY_ERR_FILE_IO;

  if (status != STEGIFY_OK)
    return status;

  return sink.error ? STEGIFY_ERR_FILE_IO : STEGIFY_OK;
}

stegify_status_t
stegify_ops_embed(const char *image_path, const uint8_t *payload,
  size_t payload_size, const char *output_path, size_t *capacity_remaining)
{
  stegify_image_t image;
  stegify_status_t status;

  if (image_path == NULL || payload == NULL || output_path == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  if (payload_size == 0 || payload_size > UINT32_MAX)
    return STEGIFY_ERR_INVALID_INPUT;

  memset(&image, 0, sizeof(image));

  status = load_image_from_path(image_path, &image);
  if (status != STEGIFY_OK)
    return status;

  status = stegify_embed(&image, payload, (uint32_t)payload_size);
  if (status != STEGIFY_OK) {
    stegify_image_free(&image);
    return status;
  }

  status = save_image_to_path(&image, output_path);
  if (status != STEGIFY_OK) {
    stegify_image_free(&image);
    return status;
  }

  if (capacity_remaining != NULL)
    *capacity_remaining = stegify_get_max_capacity(&image) - payload_size;

  stegify_image_free(&image);
  return STEGIFY_OK;
}

stegify_status_t
stegify_ops_embed_file(const char *image_path, const char *data_path,
  const char *output_path, size_t *payload_size, size_t *capacity_remaining)
{
  uint8_t *payload;
  size_t size;
  stegify_status_t status;

  if (data_path == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  status = read_file(data_path, &payload, &size);
  if (status != STEGIFY_OK)
    return status;

  status = stegify_ops_embed(
    image_path, payload, size, output_path, capacity_remaining);
  free(payload);

  if (status == STEGIFY_OK && payload_size != NULL)
    *payload_size = size;

  return status;
}

stegify_status_t
stegify_ops_extract(const char *image_path, const char *output_path,
  uint8_t **out_data, uint32_t *out_size)
{
  stegify_image_t image;
  stegify_status_t status;
  size_t capacity;
  uint8_t *buffer;
  uint32_t data_size;

  if (image_path == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  memset(&image, 0, sizeof(image));

  status = load_image_from_path(image_path, &image);
  if (status != STEGIFY_OK)
    return status;

  capacity = stegify_get_max_capacity(&image);
  if (capacity == 0 || capacity > UINT32_MAX) {
    stegify_image_free(&image);
    return STEGIFY_ERR_INSUFFICIENT_CAPACITY;
  }

  data_size = (uint32_t)capacity;

  buffer = (uint8_t *)malloc(data_size);
  if (buffer == NULL) {
    stegify_image_free(&image);
    return STEGIFY_ERR_MEMORY_ALLOC;
  }

  status = stegify_extract(&image, buffer, &data_size);
  stegify_image_free(&image);
  if (status != STEGIFY_OK) {
    free(buffer);
    return status;
  }

  if (output_path != NULL) {
    status = write_file(output_path, buffer, data_size);
    if (status != STEGIFY_OK) {
      free(buffer);
      return status;
    }
  }

  if (out_size != NULL)
    *out_size = data_size;

  if (out_data != NULL)
    *out_data = buffer;
  else
    free(buffer);

  return STEGIFY_OK;
}

stegify_status_t
stegify_ops_capacity(const char *image_path, size_t *capacity)
{
  stegify_image_t image;
  stegify_status_t status;

  if (image_path == NULL || capacity == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  memset(&image, 0, sizeof(image));

  status = load_image_from_path(image_path, &image);
  if (status != STEGIFY_OK)
    return status;

  *capacity = stegify_get_max_capacity(&image);

  stegify_image_free(&image);
  return STEGIFY_OK;
}
