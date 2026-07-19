#include <ctype.h>
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "stegify/core.h"
#include "stegify/ops.h"

#define STEGIFY_VERSION "0.1.0"

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

static void
print_help(void)
{
  printf(
    "stegify - hide and recover data in images using LSB steganography.\n"
    "\n"
    "Usage:\n"
    "  stegify embed <image_path> (-m <data_as_string> | -f <data_file_path>) -o <output_image_path> [-p] [-n]\n"
    "  stegify extract <image_path> [-o <output_file_path>] [-p] [-s <size>]\n"
    "  stegify size <image_path>\n"
    "  stegify --help | --version\n"
    "\n"
    "Commands:\n"
    "  embed     Embed a string (-m) or a file (-f) into <image_path> and write\n"
    "            the result to -o.\n"
    "  extract   Recover an embedded payload to -o, or print it with -p.\n"
    "  size      Print the maximum payload capacity of <image_path>.\n"
    "\n"
    "Options:\n"
    "  -m <text>   Embed the given string.\n"
    "  -f <file>   Embed the contents of the given file.\n"
    "  -o <file>   Output path (image for embed, data for extract).\n"
    "  -n          Embed without a size header; extract then requires -s.\n"
    "  -s <size>   Extract exactly <size> bytes instead of reading the header.\n"
    "  -p          Print the payload as a hex+ASCII table.\n"
    "  -h, --help  Show this help and exit.\n"
    "  --version   Show the version and exit.\n"
    "\n"
    "The image path must come first, before any options.\n"
    "\n"
    "Supported image formats: PNG, BMP.\n");
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
  const char *p;

  if (value == NULL || result == NULL)
    return 0;

  /* strtoul silently maps a leading '-' to a large unsigned value, so reject
   * negative input explicitly before parsing. */
  p = value;
  while (isspace((unsigned char)*p))
    p++;
  if (*p == '-')
    return 0;

  errno = 0;
  parsed = strtoul(value, &end, 10);
  if (errno != 0 || end == value || *end != '\0' || parsed > UINT32_MAX)
    return 0;

  *result = (uint32_t)parsed;
  return 1;
}

static int
parse_embed_options(int argc, char **argv, cli_options_t *options)
{
  int i;

  for (i = 3; i < argc; i++) {
    if (strcmp(argv[i], "-m") == 0 || strcmp(argv[i], "-f") == 0) {
      if (options->message != NULL || options->data_file_path != NULL) {
        fprintf(stderr, "stegify: -m and -f are mutually exclusive and may be given once\n");
        return 0;
      }
      if (i + 1 >= argc) {
        fprintf(stderr, "stegify: option '%s' requires an argument\n", argv[i]);
        return 0;
      }
      if (strcmp(argv[i], "-m") == 0)
        options->message = argv[++i];
      else
        options->data_file_path = argv[++i];
      continue;
    }

    if (strcmp(argv[i], "-o") == 0) {
      if (options->output_file_path != NULL) {
        fprintf(stderr, "stegify: option '-o' may be given once\n");
        return 0;
      }
      if (i + 1 >= argc) {
        fprintf(stderr, "stegify: option '-o' requires an argument\n");
        return 0;
      }
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

    fprintf(stderr, "stegify: unknown option '%s'\n", argv[i]);
    return 0;
  }

  if (options->message == NULL && options->data_file_path == NULL) {
    fprintf(stderr, "stegify: embed requires -m <string> or -f <file>\n");
    return 0;
  }

  if (options->output_file_path == NULL) {
    fprintf(stderr, "stegify: embed requires -o <output_image_path>\n");
    return 0;
  }

  return 1;
}

static int
parse_extract_options(int argc, char **argv, cli_options_t *options)
{
  int i;

  for (i = 3; i < argc; i++) {
    if (strcmp(argv[i], "-o") == 0) {
      if (options->output_file_path != NULL) {
        fprintf(stderr, "stegify: option '-o' may be given once\n");
        return 0;
      }
      if (i + 1 >= argc) {
        fprintf(stderr, "stegify: option '-o' requires an argument\n");
        return 0;
      }
      options->output_file_path = argv[++i];
      continue;
    }

    if (strcmp(argv[i], "-p") == 0) {
      options->print_data = 1;
      continue;
    }

    if (strcmp(argv[i], "-s") == 0) {
      if (options->has_extract_size) {
        fprintf(stderr, "stegify: option '-s' may be given once\n");
        return 0;
      }
      if (i + 1 >= argc) {
        fprintf(stderr, "stegify: option '-s' requires an argument\n");
        return 0;
      }
      if (!parse_u32(argv[++i], &options->extract_size) || options->extract_size == 0) {
        fprintf(stderr, "stegify: invalid size '%s' for -s\n", argv[i]);
        return 0;
      }
      options->has_extract_size = 1;
      continue;
    }

    fprintf(stderr, "stegify: unknown option '%s'\n", argv[i]);
    return 0;
  }

  return 1;
}

static int
handle_embed(const char *image_path, const cli_options_t *options)
{
  const uint8_t *payload;
  uint8_t *file_payload;
  size_t payload_size;
  size_t capacity_remaining;
  stegify_status_t status;

  file_payload = NULL;
  capacity_remaining = 0;

  if (options->message != NULL) {
    payload = (const uint8_t *)options->message;
    payload_size = strlen(options->message);
  } else {
    status = stegify_read_file(options->data_file_path, &file_payload, &payload_size);
    if (status != STEGIFY_OK) {
      fprintf(stderr, "Failed to read data file '%s': %s\n",
        options->data_file_path, stegify_error_string(status));
      return 1;
    }
    payload = file_payload;
  }

  status = stegify_ops_embed(image_path, payload, payload_size,
    options->output_file_path, options->no_size_header ? 0 : 1, &capacity_remaining);
  if (status != STEGIFY_OK) {
    fprintf(stderr, "Failed to embed data: %s\n", stegify_error_string(status));
    free(file_payload);
    return 1;
  }

  fprintf(stderr,
    "Embed completed: %zu bytes embedded into '%s' and saved to '%s' (capacity remaining: %zu bytes, size header: %s).\n",
    payload_size, image_path, options->output_file_path, capacity_remaining,
    options->no_size_header ? "disabled" : "enabled");

  if (options->print_data) {
    fprintf(stderr, "Embedded payload (hex+ASCII):\n");
    print_hex_ascii_table(payload, payload_size);
  }

  free(file_payload);
  return 0;
}

static int
handle_extract(const char *image_path, const cli_options_t *options)
{
  uint8_t *buffer;
  uint32_t data_size;
  uint32_t explicit_size;
  stegify_status_t status;

  buffer = NULL;
  data_size = 0;
  explicit_size = options->has_extract_size ? options->extract_size : 0;

  status = stegify_ops_extract(image_path, explicit_size, &buffer, &data_size);
  if (status != STEGIFY_OK) {
    if (status == STEGIFY_ERR_CORRUPTED_DATA)
      fprintf(stderr,
        "No stegify payload detected. If it was embedded with -n, re-run extract with -s <size>.\n");
    else
      fprintf(stderr, "Failed to extract data: %s\n", stegify_error_string(status));
    return 1;
  }

  if (options->output_file_path != NULL) {
    status = stegify_write_file(options->output_file_path, buffer, data_size);
    if (status != STEGIFY_OK) {
      fprintf(stderr, "Failed to write output file '%s': %s\n",
        options->output_file_path, stegify_error_string(status));
      free(buffer);
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
  return 0;
}

static int
handle_size(const char *image_path)
{
  size_t cap_header;
  size_t cap_raw;
  stegify_status_t status;

  cap_header = 0;
  cap_raw = 0;

  status = stegify_ops_capacity(image_path, &cap_header, &cap_raw);
  if (status != STEGIFY_OK) {
    fprintf(stderr, "Failed to load image: %s\n", stegify_error_string(status));
    return 1;
  }

  printf("capacity: %zu bytes with size header, %zu bytes with -n (%.3f MiB)\n",
    cap_header, cap_raw, (double)cap_header / (1024.0 * 1024.0));
  return 0;
}

int
main(int argc, char **argv)
{
  const char *command;
  const char *image_path;
  cli_options_t options;

  if (argc >= 2) {
    if (strcmp(argv[1], "-h") == 0 || strcmp(argv[1], "--help") == 0) {
      print_help();
      return 0;
    }
    if (strcmp(argv[1], "--version") == 0) {
      printf("stegify %s\n", STEGIFY_VERSION);
      return 0;
    }
  }

  if (argc < 3) {
    print_usage();
    return 1;
  }

  command = argv[1];
  image_path = argv[2];
  memset(&options, 0, sizeof(options));

  if (strcmp(command, "embed") == 0) {
    if (!parse_embed_options(argc, argv, &options)) {
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

  fprintf(stderr, "stegify: unknown command '%s'\n", command);
  print_usage();
  return 1;
}
