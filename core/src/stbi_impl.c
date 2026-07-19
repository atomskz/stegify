/* Compile in only the PNG and BMP decoders. The JPEG/GIF/PSD/PIC/TGA/PNM/HDR
 * paths are removed from the build entirely, so a crafted buffer can never
 * reach them; the format whitelist enforced above stb is defense in depth. */
#define STBI_ONLY_PNG
#define STBI_ONLY_BMP

/* Disable stb's HDR and linear float loaders: they are the only stb code that
 * calls pow(), so turning them off drops the libm dependency entirely. This is
 * already implied by STBI_ONLY_* above; kept explicit as the documented source
 * of the "no libm" guarantee. */
#define STBI_NO_HDR
#define STBI_NO_LINEAR

/* Reject absurd per-axis dimensions before decoding (default is 1 << 24). */
#define STBI_MAX_DIMENSIONS 32768

/* stb's failure-reason strings are never read (the library reports its own
 * status codes), so leave them out of the build. */
#define STBI_NO_FAILURE_STRINGS

/* All file access lives in the operations layer: the core feeds stb an
 * in-memory buffer and drains the encoder through a callback, so stb never
 * touches paths or stdio. */
#define STBI_NO_STDIO

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STBI_WRITE_NO_STDIO
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
