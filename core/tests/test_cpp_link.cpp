// Verifies that stegify/core.h can be included and linked from C++: without
// the extern "C" guard the C symbols would be name-mangled and fail to link.

#include "stegify/core.h"

int
main()
{
  const char *msg = stegify_error_string(STEGIFY_OK);
  stegify_image_t image;
  size_t capacity;

  image.data = 0;
  image.width = 0;
  image.height = 0;
  image.channels = 0;
  image.format = STEGIFY_FORMAT_PNG;
  capacity = stegify_get_max_capacity(&image, STEGIFY_ATTR_WITH_SIZE);

  return (msg != 0 && capacity == 0) ? 0 : 1;
}
