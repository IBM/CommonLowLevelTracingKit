#!/bin/bash
# era-adaptive golden fixture generator: adapts the writer include to the
# header layout of the checked-out source tree in /src
set -e
dnf install -y -q gcc gcc-c++ cmake make git gettext-envsubst rsync > /dev/null 2>&1
git config --global --add safe.directory "*"
cp /fix/writer.c /tmp/writer.c
if [ ! -f /src/tracing_library/include/CommonLowLevelTracingKit/tracing/tracing.h ]; then
	# pre-1.2.48 layout: header lives directly under CommonLowLevelTracingKit/
	sed -i 's|CommonLowLevelTracingKit/tracing/tracing.h|CommonLowLevelTracingKit/tracing.h|' /tmp/writer.c
fi
cmake -S /src -B /tmp/b \
  -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=OFF \
  -DCLLTK_TRACING=ON -DCLLTK_COMMAND_LINE_TOOL=OFF -DCLLTK_CPP_DECODER=OFF \
  -DCLLTK_SNAPSHOT=OFF -DCLLTK_PYTHON_DECODER=OFF -DCLLTK_DECODER=OFF \
  -DCLLTK_KERNEL_TRACING=OFF -DCLLTK_EXAMPLES=OFF -DCLLTK_TESTS=OFF > /tmp/cfg.log 2>&1 || { tail -10 /tmp/cfg.log; exit 1; }
cmake --build /tmp/b --target clltk_tracing_static -- -j4 > /tmp/build.log 2>&1 || { tail -30 /tmp/build.log; exit 1; }
LIB=$(find /tmp/b -name 'libclltk_tracing*.a' | head -1)
gcc -std=c11 -O1 -I/src/tracing_library/include /tmp/writer.c "$LIB" -pthread -o /tmp/writer
rm -rf /tmp/traces && mkdir /tmp/traces
CLLTK_TRACING_PATH=/tmp/traces /tmp/writer
cp /tmp/traces/GOLDEN.clltk_trace /out/
# fmt-style tracepoints (C++20, since 1.6.0): build the C++ writer when the
# checked-out headers and compiler support it, skip gracefully otherwise
if [ -f /src/tests/golden/generator/writer_fmt.cpp ]; then
	if g++ -std=c++20 -O1 -I/src/tracing_library/include \
		/src/tests/golden/generator/writer_fmt.cpp "$LIB" -pthread -o /tmp/writer_fmt \
		2>/tmp/writer_fmt.log; then
		CLLTK_TRACING_PATH=/tmp/traces /tmp/writer_fmt
		cp /tmp/traces/GOLDEN_FMT.clltk_trace /out/
	else
		echo "fmt writer skipped (headers or compiler without C++20 fmt support)"
	fi
fi
echo "fixture written: $(ls -l /out/GOLDEN.clltk_trace)"
