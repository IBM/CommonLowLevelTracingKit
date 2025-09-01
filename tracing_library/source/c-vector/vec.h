//
//  vec.h
//
//  Created by Mashpoe on 2/26/19.
//

#ifndef vec_h
#define vec_h

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__KERNEL__)
#include <linux/types.h>
#else
#include <stdbool.h>
#include <stdlib.h>
#endif

typedef void
	*vector; // you can't use this to store vectors, it's just used internally as a generic type
typedef size_t vec_size_t;		  // stores the number of elements
typedef unsigned char vec_type_t; // stores the number of bytes for a type

#ifndef _MSC_VER

// shortcut defines

// vec_addr is a vector* (aka type**)
#define vector_free(vec_addr) (_vector_free((vector *)vec_addr))
#define vector_add_asg(vec_addr) \
	((__typeof__(*vec_addr))(_vector_add((vector *)vec_addr, sizeof(__typeof__(**vec_addr)))))

#define vector_add(vec_addr, value) (*vector_add_asg(vec_addr) = value)

// vec is a vector (aka type*)
#define vector_erase(vec, pos, len) (_vector_erase((vector)vec, sizeof(__typeof__(*vec)), pos, len))
#define vector_remove(vec, pos) (_vector_remove((vector)vec, sizeof(__typeof__(*vec)), pos))
#define vector_find(vec, match_func, context)                                                 \
	(_vector_find((vector)vec, sizeof(__typeof__(*vec)), (vector_match_function_t)match_func, \
				  (void *)context))

#else

// shortcut defines

// vec is a vector* (aka type**)
#define vector_free(vec_addr) (_vector_free((vector *)vec_addr))
#define vector_add_asg(vec_addr, type) ((type *)_vector_add((vector *)vec_addr, sizeof(type)))

#define vector_add(vec_addr, type, value) (*vector_add_asg(vec_addr, type) = value)

// vec is a vector (aka type*)
#define vector_erase(vec, type, pos, len) (_vector_erase((vector)vec, sizeof(type), pos, len))
#define vector_remove(vec, type, pos) (_vector_remove((vector)vec, sizeof(type), pos))
#define vector_find(vec, match_func, context)                                                  \
	_vector_find((vector *)vec, sizeof(__typeof__(*vec)), (vector_match_function_t)match_func, \
				 (void *)context)

#endif

vector vector_create(void);

void _vector_free(vector *vec);

void *_vector_add(vector *vec_addr, vec_type_t type_size);

void _vector_erase(vector vec_addr, vec_type_t type_size, vec_size_t pos, vec_size_t len);

void _vector_remove(vector vec_addr, vec_type_t type_size, vec_size_t pos);

vec_size_t vector_size(vector vec);

vec_size_t vector_get_alloc(vector vec);

typedef bool (*vector_match_function_t)(void *const element, void *context);
typedef struct {
	bool found;
	vec_size_t position;
	void *entry;
} vector_entry_match_t;
vector_entry_match_t _vector_find(vector vec, vec_type_t type_size,
								  vector_match_function_t match_func, void *context);

#ifdef __cplusplus
}
#endif

#endif /* vec_h */
