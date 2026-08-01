---
name: source-style-cleanup
description: Reformat and edit comments in first-party MXCommonLibraries source without changing behavior. Use for requested readability, brace-style, whitespace, line-ending, encoding, or concise comment cleanup across C/C++ code, tests, scripts, configuration, and project files.
---

# Source style cleanup

## Select files safely

1. Read `AGENTS.md`, `.editorconfig`, `SUBMODULES.md`, and `git status --short`.
2. Include only clean, first-party text files. Exclude imported, generated, binary, certificate, and license files.
3. Do not change files already modified, deleted, or untracked before the cleanup.

## Apply the cleanup

1. Enforce `.editorconfig` indentation, encoding, line endings, trailing-whitespace, and final-newline rules.
2. Use four-space, Allman-style C/C++ formatting. Add braces to every control-flow body, including single-statement `if`, `else`, loops, and `do` bodies.
3. Keep behavior, macro structure, includes, identifiers, and public declarations unchanged.
4. Preserve Hungarian variable names and semantic prefixes such as `cStr` for CString values; do not rename identifiers during style-only work.
4. Edit prose comments for clarity and sentence case. Preserve technical terms and keep normal comments to one or two lines when practical.

## Verify

1. Inspect the diff for accidental changes to imported/generated code and user changes.
2. Check C/C++ files for remaining unbraced control-flow bodies and malformed line endings or encodings.
3. Build and run focused tests when the local configuration permits it.
