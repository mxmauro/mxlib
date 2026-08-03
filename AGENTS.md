# MXCommonLibraries Contributor Guide

## Repository layout

- `Include/` contains public C/C++ headers; `Source/` contains implementations; `Test/` contains the native test executable and its data.
- `*.vcxproj` files and `MXLib.sln` define Debug/Release Win32 and x64 Visual Studio builds.
- The following folders contain third-party code and must not be touched: `Source/ZipLib/Source`, `Source/ZipLib/MiniZipSource`, `Source/OpenSSL/Source`, `Source/JsLib/DukTape/Source`, `Source/JsLib/BigInteger`, and `Source/RapidJSON/Source`.
- Imported/generated code also includes SQLite, MariaDB headers, and generated OpenSSL outputs. Confirm ownership before editing a similar path.

## Working rules

- Check `git status --short` before editing and preserve already-dirty files.
- Make the smallest behavior-preserving change. Do not mix bug fixes, dependency updates, or identifier renames into formatting work.
- Follow `.editorconfig`: C/C++ uses four spaces, CRLF, and Latin-1; Visual Studio project files use tabs, CRLF, and UTF-8 BOM.
- Keep lines to at most 140 characters unless splitting them is impractical. When wrapping parameter lists, align continued parameters below the opening parenthesis.
- Use Allman braces for functions, classes, namespaces, and every control-flow body, including a single statement. Retain the established same-line form for C-style `struct` and `enum` declarations.
- Keep comments concise and purposeful. Use sentence case for prose while preserving identifiers, acronyms, URLs, and protocol names.
- Use the established Hungarian naming convention for variables. Preserve semantic prefixes, such as `cStr` for `CStringW`/`CStringA` values; do not rename existing identifiers for style alone.
- Preserve include order and API qualifiers such as `extern "C"`, `noexcept`, `final`, packed layouts, bitfields, and deleted operations unless required by the task.

## Validation

- Review diffs for imported/generated paths, unrelated user changes, and unnecessary whitespace churn.
- Build the relevant solution/project configuration and run focused native tests when the local toolchain and dependencies permit it.
- Report pre-existing build failures or unavailable local dependencies separately.
