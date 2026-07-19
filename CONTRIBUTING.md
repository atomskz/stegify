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
- Format code with clang-format before committing (`clang-format -i <files>`);
  the style is defined in `.clang-format` and enforced in CI. Keep declarations
  at the top of each block (C89 style), which the formatter does not impose.
- Add or update tests under the relevant component (`core/tests/`, `cli/tests/`)
  for any behavior change, and keep the suite green.
- Keep `README.md` and `CHANGELOG.md` in sync with user-facing changes.
