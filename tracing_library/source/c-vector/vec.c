//
//  vec.c
//
//  Created by Mashpoe on 2/26/19.
//
#include "vec.h"
#include "abstraction/memory.h"

#if defined(__KERNEL__)
#include <linux/string.h>
#include <linux/types.h>
#else
#include <stdint.h>
#include <string.h>
#endif

// do not check this file with "-fanalyzer"
#pragma GCC diagnostic push
#if __GNUC__ >= 10
#pragma GCC diagnostic ignored "-Wanalyzer-null-dereference"
#pragma GCC diagnostic ignored "-Wanalyzer-malloc-leak"
#pragma GCC diagnostic ignored "-Wanalyzer-possible-null-dereference"
#endif

struct vector_data {
	vec_size_t alloc; // stores the number of bytes allocated
	vec_size_t length;
	char buff[0]; // use char to store bytes of an unknown type
};
typedef struct vector_data vector_data;

static vector_data *vector_get_data(vector vec)
{

	return ((vector_data *)vec - 1);
}

vector vector_create(void)
{
	vector_data *v = memory_heap_allocation(sizeof(vector_data));
	v->alloc = 0;
	v->length = 0;

	return &v->buff;
}

void _vector_free(vector *vec)
{
	if (vec) {
		memory_heap_free(vector_get_data(*vec));
		*vec = NULL;
	}
}

vec_size_t vector_size(vector vec)
{
	if (vec == NULL)
		return 0;
	return vector_get_data(vec)->length;
}

vec_size_t vector_get_alloc(vector vec)
{
	if (vec == NULL)
		return 0;
	return vector_get_data(vec)->alloc;
}

static vector_data *vector_realloc(vector_data *v_data, vec_type_t type_size)
{
	vec_size_t new_alloc = (v_data->alloc == 0) ? 1 : v_data->alloc * 2;
	vector_data *new_v_data =
		memory_heap_realloc(v_data, sizeof(vector_data) + new_alloc * type_size);
	new_v_data->alloc = new_alloc;
	return new_v_data;
}

static bool vector_has_space(const vector_data *const v_data)
{
	return v_data->alloc - v_data->length > 0;
}

void *_vector_add(vector *vec_addr, vec_type_t type_size)
{
	if (vec_addr == NULL || *vec_addr == NULL)
		return NULL;
	vector_data *v_data = vector_get_data(*vec_addr);
	if (!vector_has_space(v_data)) {
		v_data = vector_realloc(v_data, type_size);
		*vec_addr = v_data->buff;
	}
	return (void *)&v_data->buff[type_size * v_data->length++];
}

void *_vector_insert(vector *vec_addr, vec_type_t type_size, vec_size_t pos)
{
	vector_data *v_data = vector_get_data(*vec_addr);
	vec_size_t new_length = v_data->length + 1;
	// make sure there is enough room for the new element
	if (!vector_has_space(v_data)) {
		v_data = vector_realloc(v_data, type_size);
	}
	memmove(&v_data->buff[(pos + 1) * type_size], &v_data->buff[pos * type_size],
			(v_data->length - pos) * type_size); // move trailing elements

	v_data->length = new_length;
	return &v_data->buff[pos * type_size];
}

void _vector_erase(vector vec_addr, vec_type_t type_size, vec_size_t pos, vec_size_t len)
{
	vector_data *v_data = vector_get_data(vec_addr);
	// anyone who puts in a bad index can face the consequences on their own
	memmove(&v_data->buff[pos * type_size], &v_data->buff[(pos + len) * type_size],
			(v_data->length - pos - len) * type_size);
	v_data->length -= len;
}

void _vector_remove(vector vec_addr, vec_type_t type_size, vec_size_t pos)
{
	_vector_erase(vec_addr, type_size, pos, 1);
}

vector_entry_match_t _vector_find(vector vec, vec_type_t type_size,
								  vector_match_function_t match_func, void *context)
{
	vector_entry_match_t match = {0};
	if (vec == NULL)
		return match;
	if (!match_func)
		return match;
	const vector_data *const v_data = vector_get_data(vec);
	for (size_t index = 0; index < v_data->length; index++) {
		void *element = (void *)((uint64_t)vec + (index * type_size));
		if (match_func(element, context)) {
			match.found = true;
			match.position = index;
			match.entry = element;
			return match;
		}
	}
	return match;
}
#pragma GCC diagnostic pop