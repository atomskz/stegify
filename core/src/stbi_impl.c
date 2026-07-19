/* Reject absurd per-axis dimensions before decoding (default is 1 << 24). */
#define STBI_MAX_DIMENSIONS 32768

/* We only decode 8-bit PNG/BMP via stbi_load. Disable stb's HDR and linear
 * float loaders: they are the only stb code that calls pow(), so turning them
 * off drops the libm dependency entirely (and removes unused code). */
#define STBI_NO_HDR
#define STBI_NO_LINEAR

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
