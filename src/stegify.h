#ifndef STEGIFY_H
#define STEGIFY_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    STEGIFY_OK = 0,
    STEGIFY_ERR_INVALID_INPUT,
    STEGIFY_ERR_INVALID_IMAGE,
    STEGIFY_ERR_UNSUPPORTED_FORMAT,
    STEGIFY_ERR_INSUFFICIENT_CAPACITY,
    STEGIFY_ERR_FILE_IO,
    STEGIFY_ERR_CORRUPTED_DATA
} stegify_status_t;

typedef enum {
    STEGIFY_FORMAT_UNKNOWN = -1,
    STEGIFY_FORMAT_PNG = 0,
    STEGIFY_FORMAT_BMP
} stegify_image_format_t;

/*
 * Flags for the `attributes` bitmask taken by stegify_embed and
 * stegify_extract (passed as an int). STEGIFY_ATTR_WITH_SIZE writes/reads the
 * payload header; pass 0 for a raw payload with no header.
 */
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

/*
 * Load and decode an image (PNG or BMP) from filepath into image. On success
 * image->data is owned by the library and must be released with
 * stegify_image_free. Returns STEGIFY_OK, INVALID_INPUT (NULL argument),
 * UNSUPPORTED_FORMAT (extension not PNG/BMP), FILE_IO (cannot open the file),
 * or INVALID_IMAGE (not a decodable image, or larger than the size limit).
 */
stegify_status_t
stegify_image_load(
    const char *filepath,
    stegify_image_t *image);

/* Free the pixel buffer owned by image and set image->data to NULL. */
void
stegify_image_free(stegify_image_t *image);

/*
 * Encode and write image to filepath. The encoder is chosen from the filepath
 * extension (PNG or BMP), independent of how the image was decoded. Returns
 * STEGIFY_OK, INVALID_INPUT, UNSUPPORTED_FORMAT, or FILE_IO.
 */
stegify_status_t
stegify_image_save(
    const char *filepath,
    const stegify_image_t *image);

/*
 * Maximum payload size, in bytes, that fits in image for the given attributes.
 * With STEGIFY_ATTR_WITH_SIZE the fixed header is reserved; with 0 the whole
 * LSB space is available. Returns 0 for a NULL or too-small image.
 */
size_t
stegify_get_max_capacity(const stegify_image_t *image, int attributes);

/*
 * Embed data_size bytes of data into image's pixel LSBs, in place. With
 * STEGIFY_ATTR_WITH_SIZE a header (magic + version + length) is written before
 * the payload so extraction can recover the length and detect a missing
 * payload; with 0 only the raw bytes are written. Returns STEGIFY_OK,
 * INVALID_INPUT (NULL or empty), or INSUFFICIENT_CAPACITY.
 */
stegify_status_t
stegify_embed(
    stegify_image_t *image,
    const uint8_t *data,
    uint32_t data_size,
    int attributes);

/*
 * Extract a payload from image into data. *data_size is in/out: on entry it is
 * the capacity of the data buffer, on success it is the number of bytes
 * written. Pass the same attributes used for embedding; with
 * STEGIFY_ATTR_WITH_SIZE the header is read and validated, with 0 exactly
 * *data_size bytes are read. Returns STEGIFY_OK, INVALID_INPUT,
 * INSUFFICIENT_CAPACITY, or CORRUPTED_DATA (no valid header / no payload).
 */
stegify_status_t
stegify_extract(
    const stegify_image_t *image,
    uint8_t *data,
    uint32_t *data_size,
    int attributes);

/* Return a static, human-readable description of a status code. */
const char *
stegify_error_string(stegify_status_t status);

#ifdef __cplusplus
}
#endif

#endif
