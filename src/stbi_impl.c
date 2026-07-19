/* Reject absurd per-axis dimensions before decoding (default is 1 << 24). */
#define STBI_MAX_DIMENSIONS 32768

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
