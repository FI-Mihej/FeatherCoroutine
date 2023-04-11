// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#include "stack_alloc.h"

#include <stdlib.h>

#if defined COROUTINE_HAVE_WIN32API
	#define WIN32_LEAN_AND_MEAN
	#include <windows.h>
	#include <memoryapi.h>
	#include <sysinfoapi.h>
#else
	#if defined COROUTINE_HAVE_MMAP
		#include <sys/mman.h>
	#endif

	#if defined COROUTINE_HAVE_SYSCONF || defined COROUTINE_HAVE_GETPAGESIZE
		#include <unistd.h>
	#endif
#endif

size_t sa_real_stack_size(size_t size);

static inline size_t get_page_size(void) {
#if defined COROUTINE_STATIC_PAGE_SIZE
	return COROUTINE_STATIC_PAGE_SIZE;
#elif defined COROUTINE_HAVE_WIN32API
	SYSTEM_INFO si;
	GetSystemInfo(&si);
	return si.dwPageSize;
#elif defined COROUTINE_HAVE_SYSCONF
	return sysconf(COROUTINE_SC_PAGE_SIZE);
#elif defined COROUTINE_HAVE_GETPAGESIZE
	return getpagesize();
#else
	#error Can not detext page size
#endif
}

static inline void *alloc_stack_mem(size_t size) {
#if defined COROUTINE_HAVE_WIN32API
	return VirtualAlloc(NULL, size, MEM_COMMIT, PAGE_READWRITE);
#elif defined COROUTINE_HAVE_MMAP
	return mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | COROUTINE_MAP_ANONYMOUS, -1, 0);
#elif defined COROUTINE_HAVE_ALIGNED_ALLOC
	return aligned_alloc(sa_page_size(), size);
#elif defined COROUTINE_HAVE_POSIX_MEMALIGN
	void *p = NULL;
	posix_memalign(&p, sa_page_size(), size);
	return p;
#else
	#pragma GCC warning "stack can not be aligned to the page size"
	return calloc(1, size);
#endif
}

static inline void free_stack_mem(void *stack, size_t size) {
#if defined COROUTINE_HAVE_WIN32API
	(void)size;
	VirtualFree(stack, 0, MEM_RELEASE);
#elif defined COROUTINE_HAVE_MMAP
	munmap(stack, size);
#else
	(void)size;
	free(stack);
#endif
}

void *alloc_stack(size_t minsize, size_t *realsize) {
	size_t sz = sa_real_stack_size(minsize);
	*realsize = sz;
	return alloc_stack_mem(sz);
}

void free_stack(void *stack, size_t size) {
	free_stack_mem(stack, size);
}

size_t sa_page_size(void) {
	static size_t page_size = 0;

	if(!page_size) {
		page_size = get_page_size();
	}

	return page_size;
}

size_t sa_real_stack_size(size_t size) {
	size_t page_size = sa_page_size();

	if(size == 0) {
		size = 64 * 1024;
	}

	size_t num_pages = (size - 1) / page_size + 1;

	if(num_pages < 2) {
		num_pages = 2;
	}

	return num_pages * page_size;
}
