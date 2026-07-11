// Golden fixture writer: exercises every tracepoint argument type once with
// deterministic values. The produced .clltk_trace file is committed as a
// golden test fixture; pid/tid/timestamps are frozen inside the file.
#include "CommonLowLevelTracingKit/tracing/tracing.h"
#include <stdint.h>

CLLTK_TRACEBUFFER(GOLDEN, 4096)

volatile int never = 0;

int main(void)
{
	CLLTK_TRACEPOINT(GOLDEN, "plain int %d", 42);
	CLLTK_TRACEPOINT(GOLDEN, "u8 %u u16 %u u32 %u", (uint8_t)8, (uint16_t)1616, (uint32_t)323232);
	CLLTK_TRACEPOINT(GOLDEN, "i64 %ld u64 %lu", (int64_t)-64646464,
					 (uint64_t)18446744073709551615ull);
	CLLTK_TRACEPOINT(GOLDEN, "float %f double %f", 3.5f, -2.25);
	CLLTK_TRACEPOINT(GOLDEN, "string %s", "golden string");
	CLLTK_TRACEPOINT(GOLDEN, "pointer %p", (void *)0x123456789abcull);
	const uint8_t dump_data[8] = {0xde, 0xad, 0xbe, 0xef, 0x01, 0x02, 0x03, 0x04};
	CLLTK_TRACEPOINT_DUMP(GOLDEN, "golden dump", dump_data, sizeof(dump_data));
#ifdef CLLTK_SPAN_BEGIN
	/* spans exist since 1.5.0; guarded so the generator also compiles against
	 * older library versions when regenerating historic fixtures */
	{
		clltk_span_id_t outer = CLLTK_SPAN_BEGIN(GOLDEN, CLLTK_SPAN_NO_PARENT, "golden outer span");
		clltk_span_id_t inner = CLLTK_SPAN_BEGIN(GOLDEN, outer, "golden inner span");
		CLLTK_SPAN_END(GOLDEN, inner);
		CLLTK_SPAN_END(GOLDEN, outer);
		(void)CLLTK_SPAN_BEGIN(GOLDEN, CLLTK_SPAN_NO_PARENT, "golden open span");
	}
#endif
	if (never) {
		CLLTK_TRACEPOINT(GOLDEN, "never fired %d", 1);
	}
	return 0;
}
