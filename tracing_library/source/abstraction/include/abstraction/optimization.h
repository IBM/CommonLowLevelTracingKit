// Copyright (c) 2024, International Business Machines
// SPDX-License-Identifier: BSD-2-Clause-Patent

#ifndef _CLLTK_ABSTRACTION_OPTIMIZATION_H
#define _CLLTK_ABSTRACTION_OPTIMIZATION_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Branch prediction hints
#if defined(__GNUC__) || defined(__clang__)
#define likely(x) __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x) (x)
#define unlikely(x) (x)
#endif

// Thread-local storage
#if defined(__GNUC__) || defined(__clang__)
#define THREAD_LOCAL __thread
#elif defined(_MSC_VER)
#define THREAD_LOCAL __declspec(thread)
#else
#define THREAD_LOCAL
#endif

// Force inline
#if defined(__GNUC__) || defined(__clang__)
#define FORCE_INLINE __attribute__((always_inline)) inline
#elif defined(_MSC_VER)
#define FORCE_INLINE __forceinline
#else
#define FORCE_INLINE inline
#endif

#ifdef __cplusplus
}
#endif

#endif // _CLLTK_ABSTRACTION_OPTIMIZATION_H