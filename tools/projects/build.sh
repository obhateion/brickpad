# usage: sh build.sh lorn  -> /tmp/gen/gen_lorn (O2, one file), run in /tmp/gen
Z=/userdata/claude-tools/zig/zig; export ZIG_GLOBAL_CACHE_DIR=/tmp/zig-cache ZIG_LOCAL_CACHE_DIR=/tmp/zig-cache
S=/userdata/claude/brickpad-git/src; T=/userdata/claude/brickpad-git/third_party; P=/userdata/claude/brickpad-projects
mkdir -p /tmp/gen
F="-target aarch64-linux-gnu.2.38 -mcpu=cortex_a53 -std=gnu11 -O2 -w -I$S -I$T/inc -I$P"
$Z cc $F -DPRJ="\"$1.h\"" -c -o /tmp/gen/g_$1.o $P/gen.c && $Z cc $F -c -o /tmp/gen/sample.o $S/sample.c && $Z cc -target aarch64-linux-gnu.2.38 -mcpu=cortex_a53 -o /tmp/gen/gen_$1 /tmp/gen/g_$1.o /tmp/gen/sample.o $T/lib/libSDL2-2.0.so.0 -lm -lpthread
