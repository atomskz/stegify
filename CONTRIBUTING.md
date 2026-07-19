# Contributing

## Building and testing

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
```

For a stricter local check (GCC/Clang), build with warnings as errors and the
sanitizers enabled:

```bash
cmake -S . -B build -DSTEGIFY_WERROR=ON -DSTEGIFY_SANITIZE=ON
cmake --build build
ctest --test-dir build --output-on-failure
```

## Guidelines

- Target C99 and keep the build warning-clean under `-Wall -Wextra` (GCC/Clang)
  and `/W4` (MSVC).
- Match the existing code style: declarations at the top of a block, two-space
  indentation, and the return type on its own line before a definition.
- Add or update tests under `tests/` for any behavior change, and keep the suite
  green.
- Keep `README.md` and `CHANGELOG.md` in sync with user-facing changes.
