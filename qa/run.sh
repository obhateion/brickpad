#!/bin/sh
# usage: sh run.sh BINARY OUT.txt [STEPS] : runs the harness in a fresh /tmp folder with NEON and its 4 samples
B=$(cd "$(dirname "$1")" && pwd)/$(basename "$1"); O=$2; N=${3:-2000}; P=/userdata/roms/ports/brickpad
D=/tmp/golden/run.$$; rm -rf $D; mkdir -p $D/projects/DEMO; cd $D || exit 1
cp $P/projects/DEMO/NEON.bpp projects/DEMO/
python3 -I -c "
import re,os,shutil,sys
d=open('projects/DEMO/NEON.bpp','rb').read()
for m in set(re.findall(rb'samples/[ -~]{3,250}?\.(?:wav|ogg)',d)):
    s=m.decode(); os.makedirs(os.path.dirname(s),exist_ok=True); shutil.copy('$P/'+s,s)
"
mkdir -p samples/808 && cp $P/samples/808/* samples/808/ 2>/dev/null
"$B" $N $4 > "$O" 2>${ERR:-/dev/null}; rc=$?
md5sum exports/*.wav >> "$O" 2>/dev/null
for f in shot_*.bmp; do [ -f "$f" ] && python3 -I -c "import sys; from PIL import Image; Image.open(sys.argv[1]).save(sys.argv[2])" "$f" "/tmp/golden/${f%.bmp}.png"; done
[ -n "$KEEP" ] && { rm -rf "$KEEP"; mkdir -p "$KEEP"; cp -r projects exports "$KEEP"/ 2>/dev/null; }
cd / && rm -rf $D; exit $rc
