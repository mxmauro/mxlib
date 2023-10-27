# How to update submodules

On a branch without pending changes, execute the following commands.

For each library, verify if the branch/tag is correct.

```
git subtree pull --prefix Source/ZipLib/Source         https://github.com/madler/zlib               v1.3            --squash
git subtree pull --prefix Source/ZipLib/MiniZipSource  https://github.com/nmoinvaz/minizip          4.0.1           --squash
git subtree pull --prefix Source/OpenSSL/Source        https://github.com/openssl/openssl           openssl-3.1.3   --squash
git subtree pull --prefix Source/JsLib/DukTape/Source  https://github.com/svaarala/duktape          v2.7.0          --squash
git subtree pull --prefix Source/JsLib/BigInteger      https://github.com/peterolson/BigInteger.js  master          --squash
git subtree pull --prefix Source/RapidJSON/Source      https://github.com/Tencent/rapidjson         master          --squash
```
