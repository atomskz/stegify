# stegify

Hide and recover data in images — from the command line or from C.

`stegify` is a small CLI tool and C library that hides a payload inside a PNG or
BMP using **LSB (least-significant-bit) steganography**: each payload byte is
spread across eight image bytes, one bit in each byte's lowest bit, so the
picture looks unchanged. A fixed header (magic `STGF` + a version byte + a
`uint32` length) precedes the payload, so an image that carries nothing is
reported as empty instead of returning garbage. Version 0.1.0.

> **`stegify` hides data; it does not protect it.** The payload is stored in
> plaintext and is trivially recoverable — see
> [Security & limitations](#security--limitations).

## In two commands

```bash
stegify embed cover.png -m "Hello, stegify!" -o stego.png   # hide it
stegify extract stego.png -p                                # read it back
```

`-p` prints the recovered payload as a hex+ASCII table:

```text
00000000  48 65 6c 6c 6f 2c 20 73  74 65 67 69 66 79 21     |Hello, stegify!|
```

## Build

Requires **CMake ≥ 3.14**, a **C99 compiler**, and network access on the *first*
configure (stb is fetched via CMake `FetchContent`).

```bash
# Linux / macOS
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build           # binary: ./build/stegify
```

```bat
:: Windows (MSVC, multi-config)
cmake -S . -B build
cmake --build build --config Release   :: binary: build\Release\stegify.exe
```

Run the tests with `ctest --test-dir build`. The examples below call `stegify`;
use the path above, or add the binary to your `PATH`.

## Commands

The image path is positional and must come **first**, before any options.

| Command | What it does | Example |
| --- | --- | --- |
| `embed` | Hide a string (`-m`) or file (`-f`) in an image, saved to `-o` | `stegify embed in.png -m "hi" -o out.png` |
| `extract` | Recover the payload to `-o` and/or print it with `-p` | `stegify extract out.png -o data.bin` |
| `size` | Print an image's maximum payload capacity | `stegify size in.png` |
| `--help` / `--version` | Show usage or the version and exit | `stegify --help` |

| Option | Meaning |
| --- | --- |
| `-m <string>` | Embed the given string (`embed` only; exclusive with `-f`). |
| `-f <file>` | Embed a file's contents (`embed` only; exclusive with `-m`). |
| `-o <path>` | Output path — the image for `embed` (required), the data file for `extract` (optional). |
| `-p` | Print the payload as a hex+ASCII table. Works on `extract` and on `embed -m` (not `-f`). |

### Round-trip a file

```bash
stegify size cover.png                              # capacity: 2039 bytes (0.002 MiB)
stegify embed cover.png -f secret.bin -o stego.png
stegify extract stego.png -o restored.bin
cmp -s secret.bin restored.bin && echo OK
```

## Image formats

Only **PNG** and **BMP** are supported.

- **Input** is detected from the file's *signature* (magic bytes), not its name
  — a JPEG renamed to `.png` is rejected.
- **Output** is chosen from the output path's *extension* — `-o out.bmp` writes a
  BMP whatever the input was.

JPEG is intentionally unsupported: it is lossy, so re-encoding would destroy the
LSB payload.

## Capacity

```text
floor(width * height * channels / 8) - header_size
```

Run `stegify size <image>` for the exact value.

## Library API

Two layers, both under `<stegify/…>`; a compilable example lives in
[`core/examples/usage.c`](core/examples/usage.c).

**Path-based facade — `<stegify/ops.h>`** (owns all file I/O; start here):

| Function | Purpose |
| --- | --- |
| `stegify_ops_embed` | Embed an in-memory payload into an image and save it. |
| `stegify_ops_embed_file` | Embed a payload file into an image and save it. |
| `stegify_ops_extract` | Extract a payload to a file and/or a caller buffer. |
| `stegify_ops_capacity` | Report an image's maximum payload capacity. |

**In-memory codec — `<stegify/core.h>`** (no file I/O):

| Function | Purpose |
| --- | --- |
| `stegify_image_load` | Decode an image from a buffer. |
| `stegify_image_export` | Encode an image to a write callback. |
| `stegify_image_free` | Free the decoded pixel buffer. |
| `stegify_get_max_capacity` | Maximum payload capacity of a loaded image. |
| `stegify_embed` / `stegify_extract` | Embed / extract data in the pixel LSBs. |
| `stegify_error_string` | Human-readable text for a status code. |

Every function returns a `stegify_status_t`: `STEGIFY_OK`,
`STEGIFY_ERR_INVALID_INPUT`, `STEGIFY_ERR_INVALID_IMAGE`,
`STEGIFY_ERR_UNSUPPORTED_FORMAT`, `STEGIFY_ERR_INSUFFICIENT_CAPACITY`,
`STEGIFY_ERR_MEMORY_ALLOC`, `STEGIFY_ERR_FILE_IO`, `STEGIFY_ERR_CORRUPTED_DATA`.

## Security & limitations

- **No confidentiality** — the payload is stored in plaintext and is trivially
  recoverable by anyone who looks.
- **No integrity or authentication** beyond the header's magic check; a modified
  container is not detected.
- **Easily detected** by standard steganalysis.
- **Fragile** — any lossy re-encode, resize, or format conversion destroys the
  payload; it survives only lossless, unmodified PNG/BMP.
- The output path is **overwritten without confirmation**.

If you need secrecy or tamper-resistance, encrypt and authenticate the payload
**before** embedding it.

## License

MIT — see the [`LICENSE`](LICENSE) file. Uses
[`stb`](https://github.com/nothings/stb) (`stb_image`, `stb_image_write`),
fetched at build time via CMake `FetchContent`.
