# How to update submodules

On a branch without pending changes, execute the following commands.

For each library, verify if the branch/tag is correct.

```bash
git subtree pull --prefix Source/ZipLib/Source         https://github.com/madler/zlib               v1.3.2          --squash
git subtree pull --prefix Source/ZipLib/MiniZipSource  https://github.com/zlib-ng/minizip-ng        4.2.2           --squash
git subtree pull --prefix Source/OpenSSL/Source        https://github.com/openssl/openssl           openssl-3.6.3   --squash
git subtree pull --prefix Source/JsLib/DukTape/Source  https://github.com/svaarala/duktape          v2.7.0          --squash
git subtree pull --prefix Source/JsLib/BigInteger      https://github.com/peterolson/BigInteger.js  v1.6.52         --squash
git subtree pull --prefix Source/RapidJSON/Source      https://github.com/Tencent/rapidjson         master          --squash
```

## Merge conflicts

If there is a merge conflict during the update, usually we can discard the local changes and use the remote files directly.

Instead of the subtree pull, run the following commands (this is an example for zlib):

```bash
git fetch https://github.com/madler/zlib v1.3.2

git rm -r Source/ZipLib/Source
git read-tree --prefix=Source/ZipLib/Source/ -u FETCH_HEAD

git commit -m "Update zlib to v1.3.2"
```
