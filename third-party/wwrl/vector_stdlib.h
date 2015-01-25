/**
 * vector.h -- dynamic/growable arrays in C, mimicking C++ std::vector
 * version 1.0, May 15th, 2013
 *
 * Copyright (C) 2013 William Light <wrl@illest.net>
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the authors be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef _WWRL_VECTOR_H_
#define _WWRL_VECTOR_H_

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#define VECTOR(tag, type)						\
	struct tag {							\
		size_t size;						\
		size_t capacity;					\
		type *data;						\
	}

#define VECTOR_FRONT(vec) ((vec)->data)
#define VECTOR_BACK(vec) (&(vec)->data[(vec)->size - 1])

#define VECTOR_INIT(vec, initial_size) do {				\
	(vec)->size = 0;						\
	if (!initial_size)						\
		(vec)->capacity = 1;					\
	else								\
		(vec)->capacity = (initial_size);			\
	(vec)->data = calloc(sizeof(*(vec)->data), (vec)->capacity);	\
} while (0)

#define VECTOR_FREE(vec) do {						\
	assert((vec)->data);						\
	(vec)->size = 0;						\
	(vec)->capacity = 0;						\
	free((vec)->data);						\
	(vec)->data = NULL;						\
} while (0)

#define VECTOR_CLEAR(vec) do {						\
	assert((vec)->data);						\
	(vec)->size = 0;						\
} while (0)

#define VECTOR_SHRINK_TO_FIT(vec) do {					\
	assert((vec)->data);						\
	(vec)->capacity = ((vec)->size) ? (vec)->size : 1;		\
	(vec)->data = realloc((vec)->data,				\
			(vec)->capacity * sizeof(*(vec)->data));	\
} while (0)

/**
 * insert
 */

#define VECTOR_INSERT_DATA(vec, after, items, count) do {		\
	assert((vec)->data);						\
	if ((vec)->size + (count) >= (vec)->capacity) {			\
		(vec)->capacity += (count);				\
		(vec)->data = realloc((vec)->data,			\
			(vec)->capacity * sizeof(*(vec)->data));	\
	}								\
	memmove((vec)->data + (count) + (after), (vec)->data + (after),	\
		((vec)->size - (after)) * sizeof(*(vec)->data));	\
	memcpy((vec)->data + (after), (items),				\
			(count) * sizeof(*(vec)->data));		\
	(vec)->size += (count);						\
} while (0)

#define VECTOR_INSERT(vec, after, item)					\
	VECTOR_INSERT_DATA(vec, after, item, 1)

/**
 * erase
 */

#define VECTOR_ERASE_RANGE(vec, first, last) do {			\
	assert((vec)->data);						\
	memmove((vec)->data + (first), (vec)->data + (last),		\
		((vec)->size - (last)) * sizeof(*(vec)->data));		\
	(vec)->size -= ((last) - (first));				\
} while (0)

#define VECTOR_ERASE(vec, which)					\
	VECTOR_ERASE_RANGE(vec, which, which + 1)

/**
 * push back
 */

#define VECTOR_PUSH_BACK(vec, item) do {				\
	assert((vec)->data);						\
	if ((vec)->size >= (vec)->capacity) {				\
		(vec)->capacity *= 2;					\
		(vec)->data = realloc((vec)->data,			\
			(vec)->capacity * sizeof(*(vec)->data));	\
	}								\
	(vec)->data[(vec)->size] = (item);				\
	(vec)->size++;							\
} while(0)

#define VECTOR_PUSH_BACK_DATA(vec, items, count) do {			\
	assert((vec)->data);						\
	if ((vec)->size + (count) >= (vec)->capacity) {			\
		(vec)->capacity += (count);				\
		(vec)->data = realloc((vec)->data,			\
			(vec)->capacity * sizeof(*(vec)->data));	\
	}								\
	memcpy(&(vec)->data[(vec)->size],				\
		(items), (count) * sizeof(*(vec)->data));		\
	(vec)->size += (count);						\
} while (0)

/**
 * push front
 */

#define VECTOR_PUSH_FRONT(vec, item) do {				\
	assert((vec)->data);						\
	if ((vec)->size >= (vec)->capacity) {				\
		(vec)->capacity *= 2;					\
		(vec)->data = realloc((vec)->data,			\
			(vec)->capacity * sizeof(*(vec)->data));	\
	}								\
	memmove((vec)->data + 1, (vec)->data,				\
			(vec)->size * sizeof(*(vec)->data));		\
	(vec)->data[0] = *(item);					\
	(vec)->size++;							\
} while(0)

#define VECTOR_PUSH_FRONT_DATA(vec, items, count)			\
	VECTOR_INSERT_DATA(vec, 0, items, count)

/**
 * pop
 */

#define VECTOR_POP_BACK(vec) do {					\
	assert((vec)->data);						\
	if ((vec)->size)						\
		--(vec)->size;						\
} while(0)

#define VECTOR_POP_FRONT(vec) do {					\
	assert((vec)->data);						\
	if (!(vec)->size)						\
		break;							\
	(vec)->size--;							\
	memmove((vec)->data, (vec)->data + 1,				\
		(vec)->size * sizeof(*(vec)->data));			\
} while(0)

/* vim: set ts=8 sw=8 sts=8: */
#endif /* ndef _WWRL_VECTOR_H_ */
