#ifndef STEGIFY_OPS_H
#define STEGIFY_OPS_H

#include <stddef.h>
#include <stdint.h>

#include "stegify/core.h"

/*
 * Operations layer: UI-agnostic, path-based building blocks shared by the CLI
 * and any other frontend (e.g. a GUI). These functions own all file access:
 * they read the container image, write the result, and read/write payload
 * files, so the core stays free of file I/O. They never read arguments, print,
 * or exit; the caller decides how to present results and errors. Outcomes are
 * reported through stegify_status_t.
 */

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Load image_path, embed the payload buffer, and save the result to
 * output_path (its extension selects the PNG or BMP encoder). If
 * capacity_remaining is non-NULL it receives the container's leftover payload
 * capacity. Returns STEGIFY_OK or a status from the underlying load/embed/save.
 */
stegify_status_t
stegify_ops_embed(const char *image_path, const uint8_t *payload,
  size_t payload_size, const char *output_path, size_t *capacity_remaining);

/*
 * Like stegify_ops_embed, but the payload is read from data_path. If
 * payload_size is non-NULL it receives the number of bytes embedded. Returns
 * STEGIFY_OK, FILE_IO/MEMORY_ALLOC (reading the payload), or a status from the
 * underlying embed.
 */
stegify_status_t
stegify_ops_embed_file(const char *image_path, const char *data_path,
  const char *output_path, size_t *payload_size, size_t *capacity_remaining);

/*
 * Load image_path and extract its payload. If output_path is non-NULL the
 * payload is written there. If out_data is non-NULL it receives a newly
 * allocated buffer with the payload (free with free()); otherwise the payload
 * is only written to output_path. If out_size is non-NULL it receives the
 * payload length. Returns STEGIFY_OK, CORRUPTED_DATA (no valid payload), or
 * another core status.
 */
stegify_status_t
stegify_ops_extract(const char *image_path, const char *output_path,
  uint8_t **out_data, uint32_t *out_size);

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
