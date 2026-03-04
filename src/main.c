#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stegify.h"

typedef struct {
  const char *message;
  const char *data_file_path;
  const char *output_file_path;
  int print_data;
  int no_size_header;
  uint32_t extract_size;
  int has_extract_size;
} cli_options_t;

static void
print_usage(void)
{
  fprintf(stderr,
    "Usage:\n"
    "  stegify embed <image_path> (-m <data_as_string> | -f <data_file_path>) -o <output_image_path> [-p] [-n]\n"
    "  stegify extract <image_path> [-o <output_file_path>] [-p] [-s <size>]\n"
    "  stegify size <image_path>\n");
}

static int
read_file_to_buffer(const char *path, uint8_t **buffer, size_t *size)
{
  FILE *file;
  long file_size;
  size_t read_bytes;
  uint8_t *data;

  if (path == NULL || buffer == NULL || size == NULL)
    return 0;

  file = fopen(path, "rb");
  if (file == NULL)
    return 0;

  if (fseek(file, 0, SEEK_END) != 0) {
    fclose(file);
    return 0;
  }

  file_size = ftell(file);
  if (file_size < 0) {
    fclose(file);
    return 0;
  }

  if (fseek(file, 0, SEEK_SET) != 0) {
    fclose(file);
    return 0;
  }

  data = (uint8_t *)malloc((size_t)file_size);
  if (data == NULL && file_size != 0) {
    fclose(file);
    return 0;
  }

  read_bytes = fread(data, 1, (size_t)file_size, file);
  fclose(file);

  if (read_bytes != (size_t)file_size) {
    free(data);
    return 0;
  }

  *buffer = data;
  *size = (size_t)file_size;

  return 1;
}

static int
write_buffer_to_file(const char *path, const uint8_t *data, size_t size)
{
  FILE *file;
  size_t written;

  if (path == NULL || data == NULL)
    return 0;

  file = fopen(path, "wb");
  if (file == NULL)
    return 0;

  written = fwrite(data, 1, size, file);
  fclose(file);

  return written == size;
}

static void
print_hex_ascii_table(const uint8_t *data, size_t size)
{
  size_t offset;
  size_t i;
  unsigned char ch;

  for (offset = 0; offset < size; offset += 16) {
    printf("%08zx  ", offset);

    for (i = 0; i < 16; i++) {
      if (offset + i < size)
        printf("%02x ", data[offset + i]);
      else
        printf("   ");

      if (i == 7)
        putchar(' ');
    }

    printf(" |");

    for (i = 0; i < 16 && offset + i < size; i++) {
      ch = data[offset + i];
      putchar(isprint(ch) ? (int)ch : '.');
    }

    printf("|\n");
  }
}

static int
parse_u32(const char *value, uint32_t *result)
{
  char *end;
  unsigned long parsed;

  if (value == NULL || result == NULL)
    return 0;

  errno = 0;
  parsed = strtoul(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed > UINT32_MAX)
    return 0;

  *result = (uint32_t)parsed;
  return 1;
}

static size_t
get_effective_capacity(const stegify_image_t *image, int with_size_header)
{
  size_t total_bytes;

  if (image == NULL || image->data == NULL)
    return 0;

  total_bytes = (size_t)image->width * image->height * image->channels;

  if (with_size_header) {
    if (total_bytes / 8 < sizeof(uint32_t))
      return 0;
    return (total_bytes / 8) - sizeof(uint32_t);
  }

  return total_bytes / 8;
}

static int
parse_embed_options(int argc, char **argv, cli_options_t *options)
{
  int i;

  for (i = 3; i < argc; i++) {
    if (strcmp(argv[i], "-m") == 0) {
      if (i + 1 >= argc || options->data_file_path != NULL || options->message != NULL)
        return 0;
      options->message = argv[++i];
      continue;
    }

    if (strcmp(argv[i], "-f") == 0) {
      if (i + 1 >= argc || options->message != NULL || options->data_file_path != NULL)
        return 0;
      options->data_file_path = argv[++i];
      continue;
    }

    if (strcmp(argv[i], "-o") == 0) {
      if (i + 1 >= argc || options->output_file_path != NULL)
        return 0;
      options->output_file_path = argv[++i];
      continue;
    }

    if (strcmp(argv[i], "-p") == 0) {
      options->print_data = 1;
      continue;
    }

    if (strcmp(argv[i], "-n") == 0) {
      options->no_size_header = 1;
      continue;
    }

    return 0;
  }

  if ((options->message == NULL && options->data_file_path == NULL) ||
      (options->message != NULL && options->data_file_path != NULL))
    return 0;

  if (options->output_file_path == NULL)
    return 0;

  return 1;
}

static int
parse_extract_options(int argc, char **argv, cli_options_t *options)
{
  int i;

  for (i = 3; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0) {
      if (i + 1 >= argc || options->output_file_path != NULL)
        return 0;
      options->output_file_path = argv[++i];
      continue;
    }

    if (strcmp(argv[i], "-p") == 0) {
      options->print_data = 1;
      continue;
    }

    if (strcmp(argv[i], "-s") == 0) {
      if (i + 1 >= argc || options->has_extract_size)
        return 0;
      if (!parse_u32(argv[++i], &options->extract_size) || options->extract_size == 0)
        return 0;
      options->has_extract_size = 1;
      continue;
    }

    return 0;
  }

  return 1;
}

static int
handle_embed(const char *image_path, const cli_options_t *options)
{
  stegify_image_t image;
  stegify_status_t status;
  uint8_t *payload;
  size_t payload_size;
  size_t max_capacity;
  int attributes;
  int ok;

  memset(&image, 0, sizeof(image));
  payload = NULL;
  payload_size = 0;

  status = stegify_image_load(image_path, &image);
  if (status != STEGIFY_OK) {
    fprintf(stderr, "Failed to load image: %s\n", stegify_error_string(status));
    return 1;
  }

  if (options->message != NULL) {
    payload = (uint8_t *)options->message;
    payload_size = strlen(options->message);
  } else {
    ok = read_file_to_buffer(options->data_file_path, &payload, &payload_size);
    if (!ok) {
      fprintf(stderr, "Failed to read data file: %s\n", options->data_file_path);
      stegify_image_free(&image);
      return 1;
    }
  }

  if (payload_size > UINT32_MAX) {
    fprintf(stderr, "Payload is too large.\n");
    if (options->data_file_path != NULL)
      free(payload);
    stegify_image_free(&image);
    return 1;
  }

  attributes = options->no_size_header ? 0 : STEGIFY_ATTR_WITH_SIZE;
  status = stegify_embed(&image, payload, (uint32_t)payload_size, attributes);
  if (status != STEGIFY_OK) {
    fprintf(stderr, "Failed to embed data: %s\n", stegify_error_string(status));
    if (options->data_file_path != NULL)
      free(payload);
    stegify_image_free(&image);
    return 1;
  }

  status = stegify_image_save(options->output_file_path, &image);
  if (status != STEGIFY_OK) {
    fprintf(stderr, "Failed to save image: %s\n", stegify_error_string(status));
    if (options->data_file_path != NULL)
      free(payload);
    stegify_image_free(&image);
    return 1;
  }

  max_capacity = get_effective_capacity(&image, !options->no_size_header);
  fprintf(stderr,
    "Embed completed: %zu bytes embedded into '%s' and saved to '%s' (capacity remaining: %zu bytes, size header: %s).\n",
    payload_size, image_path, options->output_file_path, max_capacity - payload_size,
    options->no_size_header ? "disabled" : "enabled");

  if (options->print_data) {
    fprintf(stderr, "Embedded payload (hex+ASCII):\n");
    print_hex_ascii_table(
      options->message != NULL ? (const uint8_t *)options->message : payload,
      payload_size);
  }

  if (options->data_file_path != NULL)
    free(payload);

  stegify_image_free(&image);
  return 0;
}

static int
handle_extract(const char *image_path, const cli_options_t *options)
{
  stegify_image_t image;
  stegify_status_t status;
  size_t max_capacity;
  uint8_t *buffer;
  uint32_t data_size;
  int attributes;
  int ok;

  memset(&image, 0, sizeof(image));
  buffer = NULL;

  status = stegify_image_load(image_path, &image);
  if (status != STEGIFY_OK) {
    fprintf(stderr, "Failed to load image: %s\n", stegify_error_string(status));
    return 1;
  }

  max_capacity = get_effective_capacity(&image, !options->has_extract_size);
  if (max_capacity == 0 || max_capacity > UINT32_MAX) {
    fprintf(stderr, "Invalid image capacity.\n");
    stegify_image_free(&image);
    return 1;
  }

  if (options->has_extract_size && options->extract_size > max_capacity) {
    fprintf(stderr, "Requested extract size exceeds image capacity.\n");
    stegify_image_free(&image);
    return 1;
  }

  data_size = options->has_extract_size ? options->extract_size : (uint32_t)max_capacity;
  buffer = (uint8_t *)malloc(data_size == 0 ? 1 : data_size);
  if (buffer == NULL) {
    fprintf(stderr, "Failed to allocate extraction buffer.\n");
    stegify_image_free(&image);
    return 1;
  }

  attributes = options->has_extract_size ? 0 : STEGIFY_ATTR_WITH_SIZE;

  status = stegify_extract(&image, buffer, &data_size, attributes);
  if (status != STEGIFY_OK) {
    fprintf(stderr, "Failed to extract data: %s\n", stegify_error_string(status));
    free(buffer);
    stegify_image_free(&image);
    return 1;
  }

  if (options->output_file_path != NULL) {
    ok = write_buffer_to_file(options->output_file_path, buffer, data_size);
    if (!ok) {
      fprintf(stderr, "Failed to write output file: %s\n", options->output_file_path);
      free(buffer);
      stegify_image_free(&image);
      return 1;
    }

    fprintf(stderr,
      "Extract completed: %u bytes extracted from '%s' and saved to '%s' (size source: %s).\n",
      data_size, image_path, options->output_file_path,
      options->has_extract_size ? "flag -s" : "container header");
  } else {
    fprintf(stderr,
      "Extract completed: %u bytes extracted from '%s' (size source: %s).\n",
      data_size, image_path, options->has_extract_size ? "flag -s" : "container header");
  }

  if (options->print_data && data_size > 0) {
    fprintf(stderr, "Extracted payload (hex+ASCII):\n");
    print_hex_ascii_table(buffer, data_size);
  }

  free(buffer);
  stegify_image_free(&image);
  return 0;
}

static int
handle_size(const char *image_path)
{
  stegify_image_t image;
  stegify_status_t status;
  size_t max_capacity;
  double max_capacity_mb;

  memset(&image, 0, sizeof(image));

  status = stegify_image_load(image_path, &image);
  if (status != STEGIFY_OK) {
    fprintf(stderr, "Failed to load image: %s\n", stegify_error_string(status));
    return 1;
  }

  max_capacity = stegify_get_max_capacity(&image);
  max_capacity_mb = (double)max_capacity / (1024.0 * 1024.0);
  printf("%zu bytes (%.3f MB)\n", max_capacity, max_capacity_mb);

  stegify_image_free(&image);
  return 0;
}

int
main(int argc, char **argv)
{
  const char *command;
  const char *image_path;
  cli_options_t options;

  if (argc < 3) {
    print_usage();
    return 1;
  }

  command = argv[1];
  image_path = argv[2];
  memset(&options, 0, sizeof(options));

  if (strcmp(command, "embed") == 0) {
    if (argc < 7 || !parse_embed_options(argc, argv, &options)) {
      print_usage();
      return 1;
    }
    return handle_embed(image_path, &options);
  }

  if (strcmp(command, "extract") == 0) {
    if (!parse_extract_options(argc, argv, &options)) {
      print_usage();
      return 1;
    }
    return handle_extract(image_path, &options);
  }

  if (strcmp(command, "size") == 0) {
    if (argc != 3) {
      print_usage();
      return 1;
    }
    return handle_size(image_path);
  }

  print_usage();
  return 1;
}
