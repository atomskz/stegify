# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [0.1.0] - 2026-07-19

Initial tagged release.

### Added
- LSB steganography core library (`stegify_core`) and the `stegify` CLI
  (`embed`, `extract`, `size`).
- A payload header (magic marker + version + length) so extraction can detect an
  image that carries no payload instead of returning random bytes.
- `--help` and `--version` output, and specific argument-error diagnostics.
- A CTest suite (library round-trips, boundary cases, C++ linkage, CLI
  round-trip) and a GitHub Actions CI workflow.
- Optional sanitizer (`STEGIFY_SANITIZE`) and warnings-as-errors
  (`STEGIFY_WERROR`) builds, and install rules for the library, CLI, and header.

### Changed
- The output image format is chosen from the output path rather than the input.
- Capacity is computed by a single function that never underflows.
- The README was rewritten in English.

### Removed
- JPEG support: lossy re-encoding cannot preserve an LSB payload.

### Fixed
- Out-of-bounds read of the size header on images smaller than the header.
- `embed_bit` fell off the end without returning; negative `-s` values were
  accepted; oversized (decompression-bomb) images are now rejected.
- The `libm` link that broke the MSVC build.
