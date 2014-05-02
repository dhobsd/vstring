# vstring

Vstring is a simple string building API for the C programming language. The
API does not make any thread-safety guarantees; sharing vstrings between
threads requires some form of synchronization. (At some point, I may make a
concurrent string API, but that's neither here nor there.) Vstring supports
static and dynamic buffers (static buffers are upgraded to dynamic buffers
as needed), and can efficiently parse signed and unsigned integers.

The API is intended to promote safety in string manipulation as well as to
provide a means to avoid hugely expensive `printf(3)`-family calls for
common operations.

## API Usage

A `vstring` type is defined by the API; this type contains all necessary
information / metadata for modifying the underlying buffer. The type is not
opaque (because opaque types suck), but it is not recommended to play with
the type outside of the API. (If you find the need to do this, fix / extend
the API and send a pull request!)

## Compiling

Simply `#include <vstring.h>`. If you use `vs_pushdouble`, you may need to 
link in the system's math library if that is not part of libc.

### vstring

The `vstring` type is defined as follows:

```c
typedef struct vstring {
	char		*contents;
	uint32_t	type;
	uint32_t	flags;
	uint64_t	pointer;
	uint64_t	size;
} vstring;
```

The `contents` pointer represents the underlying buffer.

The `type` member is a bitmap containing information about the type of the
`vstring` instance.

The `flags` member is a bitmap containing metadata about the `vstring`
instance. These flags are intended for API internal use and should not be
relied upon by consumers of the API.

The `pointer` member is used as an offset into the `contents` buffer. It
points to the end of the string + 1 byte.

The `size` member contains the total capacity of the `contents` buffer.

### Types

Three sorts of `vstring`s exist:

 * Static strings (`VS_TYPE_STATIC`): these strings are backed by a static
 buffer and cannot grow.

 * Growable static strings (`VS_TYPE_GROWABLE`): these strings are
 backed by a static buffer, but may be upgraded to dynamic strings if an
 append operation would cause an overflow.

 * Dynamic strings (`VS_TYPE_DYNAMIC`): these strings are backed by a
 dynamically allocated buffer and may grow if an append option would cause
 an overflow.

### Initialization

```c
static inline vstring *
vs_init(vstring *vs, enum vstring_type type, char *buf, size_t size)
```

A `vstring` is initialized by calling `vs_init` with the proper arguments.
If the first argument is `NULL`, the `vstring` itself will be dynamically
allocated. It is legal to pass a pointer to a statically allocated
`vstring` of type `VS_TYPE_DYNAMIC`.

The `buf` and `size` arguments are useful when `vs_init` is called to
initialize a `VS_TYPE_STATIC` or `VS_TYPE_GROWABLE` `vstring`, but also
allow passing in an externally allocated buffer with a known size into a
`vstring` of type `VS_TYPE_DYNAMIC`. Note that such a buffer must have
been allocated with the same `malloc(3)` available to `vstring`.

### Destruction

```c
static inline void
vs_deinit(vstring *vs)
```

The `vs_deinit` function destroys the `vstring` passed into it. If the
`vstring` is of type `VS_TYPE_DYNAMIC`, its contents will be freed. If the
`vstring` passed to `vs_deinit` was dynamically allocated, it will be freed
as well. In all cases, `sizeof (*vs)` bytes will be zeroed at the address
pointed to by the argument to `vs_deinit`.

### Reusing vstrings

```c
static inline void
vs_rewind(vstring *vs)
```

To avoid constant allocation of `vstring`s and their contents, it may be
useful to reuse `vstring` objects. Calling `vs_rewind` resets the pointer
of the string. Future calls to operations modifying the underlying buffer
will then start at the beginning of the buffer.

### Resizing vstrings

```c
static inline void *
vs_resize(vstring *vs, size_t hint)
```

Don't worry about it. This is done for you.

### Appending characters

```c
static inline bool
vs_push(vstring *vs, char c)
```

The `vs_push` function appends an individual character `c` into the buffer
managed by `*vs`. Returns `true` if successful, `false` otherwise.

This function isn't really intended to be used outside the API. If you are
using this function in a loop, you are almost certainly doing it wrong.

This function may fail if:

 * The buffer is `VS_TYPE_STATIC` and the append would overflow the buffer.
 * The buffer is not large enough to hold `len` bytes and resizing failed.

### Appending strings

```c
static inline bool
vs_pushstr(vstring *vs, const char *s, uint64_t len)
```

The `vs_pushstr` function appends `len` characters pointed to by `s` into
the buffer managed by `*vs`. Returns `true` if successful, `false`
otherwise.

This function may fail if:

 * The value of `len` is 0.
 * The value of `s` is NULL.
 * The buffer is `VS_TYPE_STATIC` and the append would overflow the buffer.
 * The buffer is not large enough to hold `len` bytes and resizing failed.

### Appending Integers

```c
static inline bool
vs_pushuint(vstring *vs, uint64_t n)
static inline bool
vs_pushint(vstring *vs, int64_t n)
```

The `vs_pushuint` and `vs_pushint` functions append a string representation
of the integer `n` to the buffer pointed to by `*vs`. Returns `true` if
successful, `false` otherwise. 

This can function fail for the same reasons as `vs_push` and `vs_pushstr`.

### Appending FP Numbers

```c
static inline bool
vs_pushdouble(vstring *vs, double n)
```

Stringifies the representation of `n` and appends that to the buffer pointed
to by `*vs`. It works by using `modf(3)` to retrieve the whole integer part
and the fractional part. The fractional part is padded to 9 spaces for no good
reason. `NAN`, positive and negative infinity, and negative zero is handled.

Returns `true` if successful, `false` otherwise.

This function fails when any concatenation fails, or if the floating point
number passed in is a subnormal FP number.

### Creating a C String

```c
static inline bool
vs_finalize(vstring *vs)
```

To make the buffer managed by `*vs` a valid C string, `vs_finalize` must be
called. This function is a convenience wrapper around `vs_push(vs, '\0')`
and can fail for the same reasons.

To get the length of the resulting C string without calling `strlen(3)`, you
may use `vs_len(vs) - 1`.

### Getting the contents of a vstring

```c
static inline char *
vs_contents(vstring *vs)
```

The buffer returned by `vs_contents` is not safe to use as a C string until
`vs_finalize` was successfully called.

### Getting the vstring length

```c
static inline uint64_t
vs_len(vstring *vs)
```

The length of the `vstring`-managed buffer can always be determined by a
call to `vs_len`.

## Future Improvements

It would be nice to allow custom `malloc(3)` implementations to be used
with the `vstring` API.
