/* stddef.h - Standard defines for freestanding environment */

#ifndef STDDEF_H
#define STDDEF_H

#define NULL ((void*)0)

typedef __SIZE_TYPE__ size_t;
typedef __PTRDIFF_TYPE__ ptrdiff_t;

#ifndef offsetof
#define offsetof(type, member) __builtin_offsetof(type, member)
#endif

#endif /* STDDEF_H */
