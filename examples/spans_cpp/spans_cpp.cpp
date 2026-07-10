// Copyright (c) 2026, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

// Span tracking with carryable ids in C++: the id is a plain value; a small
// RAII guard makes scoped spans convenient while the id stays available for
// passing into helpers, threads, or APIs.

#include "CommonLowLevelTracingKit/tracing/tracing.h"

#include <thread>

CLLTK_TRACEBUFFER(SPANS_CPP, 4096)

namespace
{
// convenience guard: begins a span on construction, ends it on scope exit
class Span
{
  public:
	Span(clltk_span_id_t parent, const clltk_span_id_t id) : m_id(id), m_parent(parent) {}
	~Span() { CLLTK_SPAN_END(SPANS_CPP, m_id); }
	clltk_span_id_t id() const { return m_id; }

	Span(const Span &) = delete;
	Span &operator=(const Span &) = delete;

  private:
	const clltk_span_id_t m_id;
	const clltk_span_id_t m_parent;
};
} // namespace

#define SPAN_SCOPE(_VAR_, _PARENT_, _NAME_) \
	Span _VAR_((_PARENT_), CLLTK_SPAN_BEGIN(SPANS_CPP, (_PARENT_), _NAME_))

static void worker(clltk_span_id_t parent)
{
	// the id crossed a thread boundary as a plain value
	SPAN_SCOPE(span, parent, "worker thread");
	CLLTK_TRACEPOINT(SPANS_CPP, "working on behalf of span %lu", (unsigned long)parent);
}

int main()
{
	SPAN_SCOPE(request, CLLTK_SPAN_NO_PARENT, "handle request");

	{
		SPAN_SCOPE(parsing, request.id(), "parsing");
		CLLTK_TRACEPOINT(SPANS_CPP, "parsed %d items", 3);
	}

	std::thread t(worker, request.id());
	t.join();

	return 0;
}
