
// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#ifndef KOISHI_STACK_ALLOC_H
#define KOISHI_STACK_ALLOC_H

#include <stddef.h>

void *alloc_stack(size_t minsize, size_t *realsize);
void free_stack(void *stack, size_t size);

#endif
