#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stegify/ops.h"

stegify_status_t
stegify_read_file(const char *path, uint8_t **buffer, size_t *size)
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

stegify_status_t
stegify_write_file(const char *path, const uint8_t *data, size_t size)
{
  FILE *file;
  size_t written;

  if (path == NULL || data == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  file = fopen(path, "wb");
  if (file == NULL)
    return STEGIFY_ERR_FILE_IO;

  written = fwrite(data, 1, size, file);
  fclose(file);

  return written == size ? STEGIFY_OK : STEGIFY_ERR_FILE_IO;
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

  status = stegify_image_load(image_path, &image);
  if (status != STEGIFY_OK)
    return status;

  status = stegify_embed(&image, payload, (uint32_t)payload_size);
  if (status != STEGIFY_OK) {
    stegify_image_free(&image);
    return status;
  }

  status = stegify_image_save(output_path, &image);
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
stegify_ops_extract(
  const char *image_path, uint8_t **out_data, uint32_t *out_size)
{
  stegify_image_t image;
  stegify_status_t status;
  size_t capacity;
  uint8_t *buffer;
  uint32_t data_size;

  if (image_path == NULL || out_data == NULL || out_size == NULL)
    return STEGIFY_ERR_INVALID_INPUT;

  memset(&image, 0, sizeof(image));

  status = stegify_image_load(image_path, &image);
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
  if (status != STEGIFY_OK) {
    free(buffer);
    stegify_image_free(&image);
    return status;
  }

  stegify_image_free(&image);

  *out_data = buffer;
  *out_size = data_size;
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

  status = stegify_image_load(image_path, &image);
  if (status != STEGIFY_OK)
    return status;

  *capacity = stegify_get_max_capacity(&image);

  stegify_image_free(&image);
  return STEGIFY_OK;
}
