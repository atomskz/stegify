# stegify

`stegify` is a command-line tool and C library for hiding and recovering data in
images using the LSB (Least Significant Bit) method.

The project consists of:
- a CLI tool (`src/main.c`) for everyday use;
- a library (`src/stegify.c`, `src/stegify.h`) with an API for loading/saving
  images and performing the steganography operations;
- the `stb_image`/`stb_image_write` translation unit (`src/stbi_impl.c`) for
  reading and writing PNG and BMP.

## How it works

The core idea is to store the bits of the payload in the least significant bit of
each byte of the pixel data.

- One payload byte needs eight container bytes (one bit per container byte).
- In the default (size-header) mode the library writes a small fixed header
  before the payload: a `STGF` magic marker, a one-byte format version, and the
  payload length as a `uint32_t`. On extraction the magic is validated first, so
  an image that carries no payload is reported as such instead of returning random
  bytes.
- With `-n` (embed) / `-s` (extract) the header is omitted and you supply the
  exact payload length yourself.

## Features

- Embed a string into an image.
- Embed the contents of a file into an image.
- Extract data from an image:
  - to a file;
  - to the console as a hex+ASCII table (with `-p`).
- Query the maximum payload capacity of an image (`size`).

## Project layout

- `CMakeLists.txt` — CMake build.
- `src/main.c` — CLI and argument parsing.
- `src/stegify.h` — public library API.
- `src/stegify.c` — steganography and image I/O implementation.
- `src/stbi_impl.c` — compiles the `stb_image` / `stb_image_write` implementations.
- `tests/` — CTest-driven library and CLI tests.

## Requirements

- CMake >= 3.14
- A C99 compiler
- Network access on the first CMake configure (to fetch `stb` via `FetchContent`)

## Building

### Linux / macOS

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

The binary is then at `./build/stegify`.

### Windows (MSVC)

```bat
cmake -S . -B build
cmake --build build --config Release
```

With the multi-config Visual Studio generator the binary is at
`build\Release\stegify.exe`.

### Running the tests

```bash
ctest --test-dir build --output-on-failure
```

## CLI usage

```text
stegify embed <image_path> (-m <data_as_string> | -f <data_file_path>) -o <output_image_path> [-p] [-n]
stegify extract <image_path> [-o <output_file_path>] [-p] [-s <size>]
stegify size <image_path>
stegify --help | --version
```

### 1) Embed a string

```bash
./build/stegify embed input.png -m "secret message" -o output.png
```

The command loads `input.png`, embeds the string, writes the result to
`output.png`, and prints a status line.

### 2) Embed a file

```bash
./build/stegify embed input.png -f /path/to/data.bin -o output.png
```

Notes:
- `-m` and `-f` are mutually exclusive;
- `-o` is required for `embed`;
- `-n` embeds without the size header — see the note below;
- `-p` prints the embedded payload as a hex+ASCII table.

### 3) Extract to a file

```bash
./build/stegify extract output.png -o extracted.bin
```

### 4) Extract to the console (hex+ASCII)

```bash
./build/stegify extract output.png -p
```

Without `-p`, `extract` prints only a status line. With `-p` the payload is
printed as a hex table with a parallel ASCII column, for example:

```text
00000000  48 65 6c 6c 6f 2c 20 73  74 65 67 69 66 79 21     |Hello, stegify!|
```

### 5) Query capacity

```bash
./build/stegify size input.png
```

Example output:

```text
capacity: 2039 bytes with size header, 2048 bytes with -n (0.002 MiB)
```

## `-n` and `-s`

`-n` (embed without a size header) and `-s <size>` (extract an explicit number of
bytes) go together:

- If you embed with `-n`, the payload length is **not** stored, so you **must**
  extract with `-s <exact_size>`.
- Do **not** use `-s` on an image embedded in the default mode: the header bytes
  would be treated as payload.

## Image formats

Supported:
- PNG
- BMP

The format is determined by the file extension. The output format is chosen from
the **output** path, so `-o out.bmp` writes a BMP regardless of the input format.

JPEG is intentionally not supported: it is a lossy format, so re-encoding would
destroy the LSB payload.

## Container capacity

Each payload byte occupies eight container bytes. In the default mode a small
fixed header is also stored, so the usable capacity is:

```text
floor(width * height * channels / 8) - <header size>   (default mode)
floor(width * height * channels / 8)                    (with -n)
```

Run `stegify size <image>` to see the exact capacity for both modes.

## Public library API

Declared in `src/stegify.h`.

Key functions:
- `stegify_image_load(...)` — load an image.
- `stegify_image_save(...)` — save an image (encoder chosen from the output path).
- `stegify_image_free(...)` — free the image buffer.
- `stegify_get_max_capacity(image, attributes)` — maximum payload capacity for the
  given mode (`STEGIFY_ATTR_WITH_SIZE` or `0`).
- `stegify_embed(...)` — embed data.
- `stegify_extract(...)` — extract data. `*data_size` is in/out: the caller sets it
  to the output buffer capacity and the function overwrites it with the number of
  bytes actually extracted.
- `stegify_error_string(...)` — human-readable status text.

The same `attributes` value must be used for `embed` and `extract`.

### Example

A compilable version of this example lives in `examples/usage.c`.

```c
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stegify.h"

int embed_example(void)
{
  stegify_image_t image;
  const char *payload = "hidden message";
  uint32_t payload_size = (uint32_t)strlen(payload);
  stegify_status_t status;

  if (stegify_image_load("cover.png", &image) != STEGIFY_OK)
    return 1;

  if (payload_size > stegify_get_max_capacity(&image, STEGIFY_ATTR_WITH_SIZE)) {
    stegify_image_free(&image);
    return 1;
  }

  status = stegify_embed(&image, (const uint8_t *)payload, payload_size,
                         STEGIFY_ATTR_WITH_SIZE);
  if (status == STEGIFY_OK)
    status = stegify_image_save("stego.png", &image);

  stegify_image_free(&image);
  return status == STEGIFY_OK ? 0 : 1;
}

int extract_example(void)
{
  stegify_image_t image;
  uint8_t buffer[4096];
  uint32_t size = sizeof(buffer); /* in: buffer capacity, out: bytes extracted */
  stegify_status_t status;

  if (stegify_image_load("stego.png", &image) != STEGIFY_OK)
    return 1;

  status = stegify_extract(&image, buffer, &size, STEGIFY_ATTR_WITH_SIZE);
  stegify_image_free(&image);

  if (status != STEGIFY_OK)
    return 1;

  fwrite(buffer, 1, size, stdout);
  return 0;
}
```

Status codes in `stegify_status_t`:
- `STEGIFY_OK`
- `STEGIFY_ERR_INVALID_INPUT`
- `STEGIFY_ERR_INVALID_IMAGE`
- `STEGIFY_ERR_UNSUPPORTED_FORMAT`
- `STEGIFY_ERR_INSUFFICIENT_CAPACITY`
- `STEGIFY_ERR_FILE_IO`
- `STEGIFY_ERR_CORRUPTED_DATA`

## Limitations and notes

- The LSB method is sensitive to image transformations.
- Any lossy re-encoding, resize, or format conversion destroys the hidden data;
  use PNG or BMP and do not modify the container.
- The library provides no encryption or authentication — it only hides data.
- Extraction works only if the container has not been modified after embedding.

## End-to-end example

```bash
# 1) Check the capacity
./build/stegify size cover.png

# 2) Embed a file
./build/stegify embed cover.png -f secret.bin -o stego.png

# 3) Extract it back
./build/stegify extract stego.png -o restored.bin

# 4) Compare
cmp -s secret.bin restored.bin && echo "OK"
```

## Troubleshooting

If a command fails:
- check the input path;
- check the extension (`png`, `bmp`);
- check that the payload fits in the container (`size`);
- check write permissions for the output path;
- if `extract` reports "No stegify payload detected", the image has no payload in
  the default mode — if it was embedded with `-n`, pass `-s <size>`.

## License

This project is distributed under the MIT License. See the `LICENSE` file for the
full text.

## Third-party licenses

The project uses `stb` (`stb_image`, `stb_image_write`), fetched at build time via
CMake `FetchContent` from the official `nothings/stb` repository.
