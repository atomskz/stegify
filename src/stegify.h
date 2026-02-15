#ifndef STEGIFY_H
#define STEGIFY_H

#include <stddef.h>
#include <stdint.h>

typedef enum {
    STEGIFY_OK = 0,
    STEGIFY_ERR_INVALID_INPUT,
    STEGIFY_ERR_INVALID_IMAGE,
    STEGIFY_ERR_UNSUPPORTED_FORMAT,
    STEGIFY_ERR_INSUFFICIENT_CAPACITY,
    STEGIFY_ERR_MEMORY_ALLOC,
    STEGIFY_ERR_FILE_IO,
    STEGIFY_ERR_CORRUPTED_DATA
} stegify_status_t;

typedef enum {
    STEGIFY_FORMAT_UNKNOWN = -1,
    STEGIFY_FORMAT_PNG = 0,
    STEGIFY_FORMAT_BMP,
    STEGIFY_FORMAT_JPEG
} stegify_image_format_t;

typedef enum {
    STEGIFY_ATTR_WITH_SIZE = 1
} stegify_attribute_t;

typedef struct {
    uint8_t *data;
    uint32_t width;
    uint32_t height;
    uint8_t channels;
    stegify_image_format_t format;
} stegify_image_t;

stegify_status_t
stegify_image_load(
    const char *filepath,
    stegify_image_t *image);

void
stegify_image_free(stegify_image_t *image);

stegify_status_t
stegify_image_save(
    const char *filepath,
    const stegify_image_t *image);

size_t
stegify_get_max_capacity(const stegify_image_t *image);

stegify_status_t
stegify_embed(
    stegify_image_t *image,
    const uint8_t *data,
    uint32_t data_size,
    int attributes);

stegify_status_t
stegify_extract(
    const stegify_image_t *image,
    uint8_t *data,
    uint32_t *data_size,
    int attributes);

const char *
stegify_error_string(stegify_status_t status);

#endif
