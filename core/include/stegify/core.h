#ifndef STEGIFY_CORE_H
#define STEGIFY_CORE_H

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
  STEGIFY_ERR_MEMORY_ALLOC,
  STEGIFY_ERR_FILE_IO,
  STEGIFY_ERR_CORRUPTED_DATA
} stegify_status_t;

typedef enum {
  STEGIFY_FORMAT_UNKNOWN = -1,
  STEGIFY_FORMAT_PNG = 0,
  STEGIFY_FORMAT_BMP
} stegify_image_format_t;

typedef struct {
  uint8_t *data;
  uint32_t width;
  uint32_t height;
  uint8_t channels;
  stegify_image_format_t format;
} stegify_image_t;

/*
 * Sink for encoded image bytes, matching stb's write callback. It is invoked
 * one or more times with successive chunks of the encoded image (PNG arrives in
 * a single call, BMP in several), so the callback must append them in order.
 * size is always non-negative.
 */
typedef void (*stegify_write_fn)(void *ctx, void *data, int size);

/*
 * Detect the image format of an encoded buffer from its leading signature.
 * Returns STEGIFY_FORMAT_PNG, STEGIFY_FORMAT_BMP, or STEGIFY_FORMAT_UNKNOWN
 * (NULL, too short, or not one of the supported formats).
 */
stegify_image_format_t
stegify_image_format(const uint8_t *buffer, size_t size);

/*
 * Decode an encoded image (PNG or BMP) held in buffer into image. The function
 * does no file I/O: the caller supplies the encoded bytes. On success
 * image->data is owned by the library and must be released with
 * stegify_image_free. Returns STEGIFY_OK, INVALID_INPUT (NULL, empty, or a
 * buffer too large for the decoder), UNSUPPORTED_FORMAT (not PNG/BMP by
 * signature), or INVALID_IMAGE (not a decodable image, or larger than the size
 * limit).
 */
stegify_status_t
stegify_image_load(const uint8_t *buffer, size_t size, stegify_image_t *image);

/* Free the pixel buffer owned by image and set image->data to NULL. */
void
stegify_image_free(stegify_image_t *image);

/*
 * Encode image and deliver the encoded bytes through cb. The encoder is chosen
 * from image->format (PNG or BMP). cb is called with successive chunks and the
 * opaque ctx; where they go (a file, a growing buffer) is the caller's
 * responsibility. The function does no file I/O and cannot observe a sink
 * failure, so a caller writing to a file must track that itself through ctx.
 * Returns STEGIFY_OK, INVALID_INPUT, UNSUPPORTED_FORMAT, or MEMORY_ALLOC (the
 * encoder failed to allocate).
 */
stegify_status_t
stegify_image_export(
  const stegify_image_t *image, stegify_write_fn cb, void *ctx);

/*
 * Maximum payload size, in bytes, that fits in image (the fixed header is
 * always reserved). Returns 0 for a NULL or too-small image.
 */
size_t
stegify_get_max_capacity(const stegify_image_t *image);

/*
 * Embed data_size bytes of data into image's pixel LSBs, in place. A header
 * (magic + version + length) is written before the payload so extraction can
 * recover the length and detect a missing payload. Returns STEGIFY_OK,
 * INVALID_INPUT (NULL or empty), or INSUFFICIENT_CAPACITY.
 */
stegify_status_t
stegify_embed(stegify_image_t *image, const uint8_t *data, uint32_t data_size);

/*
 * Extract a payload from image into data. *data_size is in/out: on entry it is
 * the capacity of the data buffer, on success it is the number of bytes
 * written. The header is read and validated to recover the payload length.
 * Returns STEGIFY_OK, INVALID_INPUT, INSUFFICIENT_CAPACITY, or CORRUPTED_DATA
 * (no valid header / no payload).
 */
stegify_status_t
stegify_extract(
  const stegify_image_t *image, uint8_t *data, uint32_t *data_size);

/* Return a static, human-readable description of a status code. */
const char *
stegify_error_string(stegify_status_t status);

#ifdef __cplusplus
}
#endif

#endif
