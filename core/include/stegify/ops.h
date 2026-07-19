#ifndef STEGIFY_OPS_H
#define STEGIFY_OPS_H

#include <stddef.h>
#include <stdint.h>

#include "stegify/core.h"

/*
 * Operations layer: UI-agnostic building blocks shared by the CLI
 * and any other frontend (e.g. a GUI). These functions perform the full
 * file-level workflows on top of the stegify core and report their outcome
 * through stegify_status_t. They never read arguments, print, or exit; the
 * caller decides how to present results and errors.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Read an entire file into a newly allocated buffer. On success *buffer is
 * owned by the caller (free with free()) and *size is its length. Returns
 * STEGIFY_OK, INVALID_INPUT (NULL argument), FILE_IO (cannot open/read), or
 * MEMORY_ALLOC.
 */
stegify_status_t
stegify_read_file(const char *path, uint8_t **buffer, size_t *size);

/*
 * Write size bytes to path (overwriting any existing file). Returns
 * STEGIFY_OK, INVALID_INPUT, or FILE_IO.
 */
stegify_status_t
stegify_write_file(const char *path, const uint8_t *data, size_t size);

/*
 * Load image_path, embed the payload buffer, and save the result to
 * output_path. If capacity_remaining is non-NULL it receives the leftover
 * payload capacity of the container. Returns STEGIFY_OK or a status from the
 * underlying load/embed/save.
 */
stegify_status_t
stegify_ops_embed(const char *image_path, const uint8_t *payload,
  size_t payload_size, const char *output_path, size_t *capacity_remaining);

/*
 * Load image_path and extract a payload into a newly allocated buffer. On
 * success *out_data is owned by the caller (free with free()) and *out_size is
 * its length. The stored size header gives the payload length. Returns
 * STEGIFY_OK, CORRUPTED_DATA (no valid payload), or another core status.
 */
stegify_status_t
stegify_ops_extract(
  const char *image_path, uint8_t **out_data, uint32_t *out_size);

/*
 * Load image_path and report its maximum payload capacity into *capacity.
 * Returns STEGIFY_OK or a load status.
 */
stegify_status_t
stegify_ops_capacity(const char *image_path, size_t *capacity);

#ifdef __cplusplus
}
#endif

#endif
