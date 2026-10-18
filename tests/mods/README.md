# Test of the JavaScript mods

The host of the mods (common/scripting/modhost.cpp) does not depend on the game, so it is tested alone.
In the MSYS2 MinGW64 shell, from the root of the repository:

```bash
Q=lib/quickjs; OUT=/tmp/modtest; mkdir -p $OUT
for f in dtoa libregexp libunicode quickjs; do gcc -c -O1 -D_GNU_SOURCE -DQUICKJS_NG_BUILD -I$Q $Q/$f.c -o $OUT/$f.o; done
gcc -c -O1 -Ilib/miniz lib/miniz/miniz.c -o $OUT/miniz.o
g++ -std=c++20 -Wall -Icommon -I$Q -Ilib/miniz -Ilib/rapidjson/include \
    tests/mods/modhost_test.cpp common/scripting/mod*.cpp common/scripting/api/*.cpp $OUT/*.o -o $OUT/modhost_test && $OUT/modhost_test
```
