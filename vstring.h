/*
 * vstring: https://github.com/dhobsd/vstring
 *
 * Copyright 2003-2013 Devon H. O'Dell <devon.odell@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 *  * Redistributions of source code must retain the above copyright notice, 
 *    this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright notice, 
 *    this list of conditions and the following disclaimer in the documentation 
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */
#ifndef _VSTRING_H_
#define _VSTRING_H_

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct vstring_malloc {
	void		*(*vs_malloc)(size_t);
	void		*(*vs_realloc)(void *, size_t);
	void		(*vs_free)(void *);
} vstring_malloc;

typedef struct vstring {
	char		*contents;
	uint32_t	type;
	uint32_t	flags;
	uint64_t	pointer;
	uint64_t	size;
	vstring_malloc	vm;
	char		vs___pad[8];
} vstring;

enum {
	VS_ALLOCSIZE = 256,
};

enum vstring_type {
	VS_TYPE_DYNAMIC		= 1,
	VS_TYPE_STATIC		= 1 << 1,
	VS_TYPE_GROWABLE	= 1 << 2,
};

enum vstring_flags {
	VS_NEEDSFREE	= 1, /* Set if the API needs to free the vs itself */
};

static inline vstring *
vs_init(vstring *vs, vstring_malloc *vm, enum vstring_type type, char *buf,
    size_t size)
{

	if ((type & VS_TYPE_DYNAMIC)) {
		if (vs == NULL) {
			if (vm != NULL) {
				vs = (vstring *)vm->vs_malloc(sizeof (*vs));
			} else {
				vs = (vstring *)calloc(1, sizeof (*vs));
			}
			vs->flags |= VS_NEEDSFREE;
		} else {
			memset(vs, 0, sizeof (*vs));
		}

		if (vs == NULL) {
			return NULL;
		}

		if (buf != NULL && size > 0) {
			vs->contents = buf;
			vs->size = size;
		}
	} else if ((type & VS_TYPE_STATIC) || (type & VS_TYPE_GROWABLE)) {
		if (buf == NULL || size == 0) {
			return NULL;
		}

		if (vs == NULL) {
			if (vm != NULL) {
				vs = (vstring *)vm->vs_malloc(sizeof (*vs));
			} else {
				vs = (vstring *)calloc(1, sizeof (*vs));
			}
			vs->flags |= VS_NEEDSFREE;
		} else {
			memset(vs, 0, sizeof (*vs));
		}

		vs->contents = buf;
		vs->size = size;
	}

	vs->type = type;
	if (vm != NULL) {
		vs->vm.vs_malloc = vm->vs_malloc;
		vs->vm.vs_realloc = vm->vs_realloc;
		vs->vm.vs_free = vm->vs_free;
	} else {
		memset(&vs->vm, 0, sizeof (vs->vm));
	}

	return vs;
}

static inline void
vs_deinit(vstring *vs)
{

	if ((vs->type & VS_TYPE_DYNAMIC) && vs->contents != NULL) {
		if (vs->vm.vs_free) {
			vs->vm.vs_free(vs->contents);
		} else {
			free(vs->contents);
		}
	}

	if (vs->flags & VS_NEEDSFREE) {
		memset(vs, 0, sizeof (*vs));
		if (vs->vm.vs_free) {
			vs->vm.vs_free(vs);
		} else {
			free(vs);
		}
	} else {
		memset(vs, 0, sizeof (*vs));
	}
}

static inline void
vs_rewind(vstring *vs)
{

	vs->pointer = 0;
}

static inline void *
vs_resize(vstring *vs, size_t hint)
{
	char *tmp;

	if (vs->size == 0) {
		if (VS_ALLOCSIZE < hint) {
			vs->size = hint;
		} else {
			vs->size = VS_ALLOCSIZE;
		}

		if (vs->vm.vs_malloc) {
			vs->contents = (char *)vs->vm.vs_malloc(vs->size);
		} else {
			vs->contents = (char *)calloc(1, vs->size);
		}
	} else {
		size_t size = vs->size * 2;
		if (size < hint) {
			size = hint * 2;
		}

		if ((vs->type & VS_TYPE_GROWABLE)) {
			/* Upgrade to a dynamic string. */
			vs->type = VS_TYPE_DYNAMIC;

			if (vs->vm.vs_malloc) {
				tmp = (char *)vs->vm.vs_malloc(size);
			} else {
				tmp = (char *)calloc(1, size);
			}

			memcpy(tmp, vs->contents, vs->size);
			vs->contents = tmp;
			vs->size = size;
		} else if ((vs->type & VS_TYPE_DYNAMIC)) {
			if (vs->vm.vs_realloc) {
				tmp = (char *)vs->vm.vs_realloc(vs->contents, size * 2);
			} else {
				tmp = (char *)realloc(vs->contents, size * 2);
			}
			if (tmp != NULL) {
				vs->contents = tmp;
				vs->size = size;
			}
		} else if ((vs->type & VS_TYPE_STATIC)) {
			/*
			 * VS_TYPE_STATIC strings that do not also have
			 * VS_TYPE_GROWABLE set cannot be resized.
			 */
			return NULL;
		}
	}

	/* This API is meant to return NULL on failure */
	return vs->contents;
}

static inline bool
vs_push(vstring *vs, char c)
{

	if (vs->pointer >= vs->size) {
		if (vs_resize(vs, vs->size + 1) == NULL) {
			return false;
		}
	}

	vs->contents[vs->pointer++] = c;
	return true;
}

static inline bool
vs_pushstr(vstring *vs, const char *s, uint64_t len)
{

	if (s == NULL || len == 0) {
		return false;
	}

	if (vs->pointer + len > vs->size) {
		if (vs_resize(vs, vs->size + len) == NULL) {
			return false;
		}
	}

	memmove(&vs->contents[vs->pointer], s, len);
	vs->pointer += len;
	return true;
}

static inline bool
vs_pushuint(vstring *vs, uint64_t n)
{
	char buf[20];
	int s, e, l;

	if (n != 0) {
		s = e = 0;

		/* Parse the number into the buffer in reverse order. */
		while (n != 0) {
			buf[e++] = '0' + (n % 10);
			n /= 10;
		}

		/* Reverse the string in-place. */
		for (l = --e; e > s; --e, ++s) {
			buf[s] ^= buf[e];
			buf[e] ^= buf[s];
			buf[s] ^= buf[e];
		}

		return vs_pushstr(vs, buf, l + 1);
	} else {
		return vs_push(vs, '0');
	}
}

static inline bool
vs_pushint(vstring *vs, int64_t n)
{
	char buf[21];
	int s, e, l;
	int64_t t;

	if (n != 0) {
		s = e = 0;
		t = n;

		if (t < 0) {
			n = -n;
		}

		while (n != 0) {
			buf[e++] = '0' + (n % 10);
			n /= 10;
		}

		if (t < 0) {
			buf[e++] = '-';
		}

		/* Reverse the string in-place. */
		for (l = --e; e > s; --e, ++s) {
			buf[s] ^= buf[e];
			buf[e] ^= buf[s];
			buf[s] ^= buf[e];
		}

		return vs_pushstr(vs, buf, l + 1);
	} else {
		return vs_push(vs, '0');
	}
}

static inline bool
vs_padint(vstring *vs, uint64_t n, int places)
{
	char buf[20];
	int s, e, l;

	if (places > 19) {
		return false;
	}

	s = e = 0;
	while (n != 0) {
		buf[e++] = '0' + (n % 10);
		n /= 10;

		if (e == places) {
			break;
		}
	}

	n = places - e;
	while (n--) {
		buf[e++] = '0';
	}

	for (l = --e; e > s; --e, ++s) {
		buf[s] ^= buf[e];
		buf[e] ^= buf[s];
		buf[s] ^= buf[e];
	}

	return vs_pushstr(vs, buf, l + 1);
}

static inline bool
vs_pushdouble(vstring *vs, double n)
{
	double modf_int, modf_frac;
	int fpclass;

	fpclass = fpclassify(n);
	switch (fpclass) {
	case FP_NAN:
		return vs_pushstr(vs, "NaN", 3);

	case FP_INFINITE:
		if (n == INFINITY) {
			return vs_pushstr(vs, "inf", 3);
		}

		return vs_pushstr(vs, "-inf", 4);

	case FP_NORMAL:
	case FP_ZERO:
		if (signbit(n)) {
			if (!vs_push(vs, '-')) {
				return false;
			}
		}
		modf_frac = modf(n, &modf_int);
		return (vs_pushuint(vs, (uint64_t)modf_int) &&
		    vs_push(vs, '.') &&
		    vs_padint(vs, (uint64_t)(1e9*modf_frac), 9));

	case FP_SUBNORMAL:
	default:
		return false;
	}
}

static inline bool
vs_finalize(vstring *vs)
{

	return vs_push(vs, '\0');
}

static inline char *
vs_contents(vstring *vs)
{

	return vs->contents;
}

static inline uint64_t
vs_len(vstring *vs)
{

	return vs->pointer;
}

#endif
