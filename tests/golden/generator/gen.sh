#!/bin/bash
# builds the tracing library from /src and produces a golden trace file in /out
set -e
dnf install -y -q gcc gcc-c++ cmake make git gettext-envsubst rsync > /dev/null 2>&1
git config --global --add safe.directory "*"; cmake -S /src -B /tmp/b \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
  -DCLLTK_TRACING=ON -DCLLTK_COMMAND_LINE_TOOL=OFF -DCLLTK_CPP_DECODER=OFF \
  -DCLLTK_SNAPSHOT=OFF -DCLLTK_PYTHON_DECODER=OFF -DCLLTK_EXAMPLES=OFF \
  -DCLLTK_TESTS=OFF > /tmp/cfg.log 2>&1 || { tail -5 /tmp/cfg.log; exit 1; }
cmake --build /tmp/b --target clltk_tracing_static -- -j4 > /tmp/build.log 2>&1 || { tail -30 /tmp/build.log; exit 1; }
LIB=$(find /tmp/b -name 'libclltk_tracing*.a' | head -1)
gcc -std=c11 -O1 -I/src/tracing_library/include /src/.fixtures/writer.c "$LIB" -pthread -o /tmp/writer
rm -rf /tmp/traces && mkdir /tmp/traces
CLLTK_TRACING_PATH=/tmp/traces /tmp/writer
cp /tmp/traces/GOLDEN.clltk_trace /out/
uname -m > /out/arch.txt
echo "fixture written: $(ls -l /out/GOLDEN.clltk_trace)"
