---
name: mxcommonlibraries-maintenance
description: Safely maintain the MXCommonLibraries Visual Studio C/C++ solution. Use when changing its first-party libraries, public headers, native tests, solution/project files, or repository guidance, especially when dependency ownership and existing working-tree changes must be preserved.
---

# MXCommonLibraries maintenance

## Establish scope

1. Read `AGENTS.md`, `.editorconfig`, `SUBMODULES.md`, and `git status --short` before editing.
2. Treat public headers in `Include/`, implementations in `Source/`, and native tests in `Test/` as first-party only after checking the path.
3. Do not edit imported or generated code: OpenSSL, RapidJSON, Duktape source, BigInteger, zlib/minizip, SQLite, MariaDB headers, or OpenSSL generated outputs.
4. Preserve files that were dirty before the task unless the user explicitly includes them.

## Make and validate changes

1. Preserve the existing C/C++ API surface and Visual Studio configuration unless the task requires a change.
2. Follow `.editorconfig` and the repository’s Allman-brace and comment conventions.
3. Preserve the established Hungarian variable prefixes, including `cStr` for CString values.
3. Review the diff before finishing; confirm that no imported/generated path or unrelated user change was modified.
4. Build the relevant Debug or Release Win32/x64 project and run focused native tests when practical. Separate environment or pre-existing failures from new failures.
