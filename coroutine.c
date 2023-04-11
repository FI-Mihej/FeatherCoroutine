// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#include "coroutine.h"
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdint.h>

#include "fcontext.h"
#include "stack_alloc.h"

#define STACK_SIZE (1024 * 1024)
#define DEFAULT_COROUTINE 16

typedef unsigned long long coro_ptr_diff_t;

struct coroutine;

struct schedule
{
	void *stack;
	void *stack_top;
	size_t stack_size;
	void *sp;
	coro_id running;
	struct coroutine *current_coro;
};

struct coroutine
{
	fcontext_t fctx;
	fcontext_t wayback_fctx;

	coro_id id;
	coroutine_func func;
	void *payload;
	schedule_t sch;
	// ptrdiff_t max_coro_number;
	int status;
	void *context_holder;
	coro_ptr_diff_t context_holder_size;
	void *current_stack_ptr;
};

struct StackTrackingStruct
{
	coro_ptr_diff_t a;
	coro_ptr_diff_t b;
	coro_ptr_diff_t c;
	coro_ptr_diff_t d;
	coro_ptr_diff_t e;
};

void *print_stack_pointer()
{
	struct StackTrackingStruct sts = {0, 0, 0, 0, 0};
	DEBUG_PRINTF(("\tc >> STACK PTR: %p\n", (void *)&sts));
	void *stp = (void *)&sts;
	return stp;
}

coro_ptr_diff_t stack_size_counter(void *bottom_ptr, void *top_ptr)
{
	coro_ptr_diff_t bottom = (coro_ptr_diff_t)bottom_ptr;
	coro_ptr_diff_t top = (coro_ptr_diff_t)top_ptr;
	DEBUG_PRINTF(("\tc >> STACK SIZE COUNTER: BOTTOM: %p; TOP: %p\n", bottom, top));
	if (bottom <= top)
	{
		return (top - bottom);
	}
	else
	{
		return (bottom - top);
	}
}

void *get_stack_pointer()
{
	DEBUG_PRINTF(("\tc >> get_stack_pointer start\n"));
	struct StackTrackingStruct sts = {0, 0, 0, 0, 0};
	void *stp = (void *)&sts;
	DEBUG_PRINTF(("\tc >> get_stack_pointer end\n"));
	return stp;
}

unsigned char get_stack_direction()
{
	DEBUG_PRINTF(("\tc >> get_stack_direction start\n"));
	struct StackTrackingStruct sts = {0, 0, 0, 0, 0};
	void *stp = (void *)&sts;
	void *deep_stp = get_stack_pointer();
	if ((coro_ptr_diff_t)deep_stp < (coro_ptr_diff_t)stp)
	{
		DEBUG_PRINTF(("\tc >> get_stack_direction end\n"));
		return 1;
	}
	else
	{
		DEBUG_PRINTF(("\tc >> get_stack_direction end\n"));
		return 0;
	}
}

coro_ptr_diff_t calc_stack_size(void *bottom_ptr, void *top_ptr)
{
	coro_ptr_diff_t bottom = (coro_ptr_diff_t)bottom_ptr;
	coro_ptr_diff_t top = (coro_ptr_diff_t)top_ptr;
	if (bottom <= top)
	{
		return (top - bottom + 1);
	}
	else
	{
		return (bottom - top + 1);
	}
}

struct coroutine *
_co_new(schedule_t S, coroutine_func func, void *payload, coro_id id)
{
	DEBUG_PRINTF(("\tc >> _co_new start = id:%llu\n", S->running));
	struct coroutine *co = malloc(sizeof(*co));
	DEBUG_PRINTF(("\tc >> _co_new. New coro with ptr: %p; = id:%llu\n", co, S->running));
	co->fctx = NULL;
	co->wayback_fctx = NULL;
	if (id) {
		co->id = id;
	} else {
		co->id = (coro_id)co;
	}
	co->func = func;
	co->payload = payload;
	co->sch = S;
	// co->max_coro_number = 0;
	co->status = COROUTINE_READY;
	co->context_holder = NULL;
	co->context_holder_size = 0;
	co->current_stack_ptr = NULL;
	DEBUG_PRINTF(("\tc >> _co_new end = id:%llu\n", S->running));
	return co;
}

void _co_delete(struct coroutine *co)
{
	DEBUG_PRINTF(("\tc >> _co_delete start = ptp: %p\n", co));
	DEBUG_PRINTF(("\tc >> _co_delete 0\n"));
	if (co->context_holder)
	{
		DEBUG_PRINTF(("\tc >> _co_delete 1\n"));
		free(co->context_holder);
	}
	DEBUG_PRINTF(("\tc >> _co_delete 2\n"));
	co->context_holder = NULL;
	DEBUG_PRINTF(("\tc >> _co_delete 3\n"));
	co->context_holder_size = 0;
	DEBUG_PRINTF(("\tc >> _co_delete 4\n"));
	free(co);
	DEBUG_PRINTF(("\tc >> _co_delete end = ptp: %p\n", co));
}

schedule_t 
coro_server_open(void)
{
	schedule_t S = malloc(sizeof(*S));
	S->stack = alloc_stack(STACK_SIZE, &S->stack_size);
	S->stack_top = (void*)((coro_ptr_diff_t)(S->stack) + (coro_ptr_diff_t)(S->stack_size) - 1);
	DEBUG_PRINTF(("\tc >> coro_server_open start = id:%llu\n", S->running));
	DEBUG_PRINTF(("\tc >> S: %p; S size: %d; schedule size: %d\n", S, sizeof(*S), sizeof(struct schedule)));

	S->running = 0;
	S->current_coro = NULL;

	DEBUG_PRINTF(("\tc >> coro_server_open end = id:%llu\n", S->running));
	return S;
}

void coro_server_close(schedule_t S)
{
	DEBUG_PRINTF(("\tc >> coro_server_close start\n"));
	int i;
	S->current_coro = NULL;
	free_stack(S->stack, S->stack_size);
	S->stack = NULL;
	S->stack_top = NULL;
	S->stack_size = 0;
	free(S);
	S = NULL;
	DEBUG_PRINTF(("\tc >> coro_server_close end\n"));
}

coroutine_t coroutine_new(schedule_t S, coroutine_func func, void *payload, coro_id id)
{
	struct coroutine *co = _co_new(S, func, payload, id);
	return co;
}

static void
fcontext_entry(transfer_t t)
{
	schedule_t S = (schedule_t )(t.data);
	S->current_coro->wayback_fctx = t.fctx;
	int id = S->running;
	DEBUG_PRINTF(("\tc >> fcontext_entry start = id:%llu\n", id));
	struct coroutine *C = S->current_coro;

	S->sp = get_stack_pointer();
	C->status = COROUTINE_SUSPEND;

	DEBUG_PRINTF(("\tc >> fcontext_entry before jump_fcontext 0 = id:%llu\n", id));
	S->current_coro->wayback_fctx = jump_fcontext(S->current_coro->wayback_fctx, NULL).fctx;

	C->func(S, C->payload);

	S->sp = get_stack_pointer();
	C->status = COROUTINE_DEAD;

	DEBUG_PRINTF(("\tc >> fcontext_entry before jump_fcontext 1 = id:%llu\n", id));
	jump_fcontext(S->current_coro->wayback_fctx, NULL);
	DEBUG_PRINTF(("\tc >> fcontext_entry end = id:%llu\n", id));
}

void coroutine_resume(schedule_t S, coroutine_t co)
{
	assert(S->running == 0);
	struct coroutine *C = co;
	if (C == NULL)
	{
		DEBUG_PRINTF(("\tc >> coroutine_resume C == NULL. EXITING\n"));
		S->running = 0;
		S->current_coro = NULL;
		return;
	}

	coro_id id = co->id;
	DEBUG_PRINTF(("\tc >> coroutine_resume start = id:%llu\n", id));
	S->running = id;
	S->current_coro = C;

	int status = C->status;
	switch (status)
	{
	case COROUTINE_READY:
		DEBUG_PRINTF(("\tc >> coroutine_resume COROUTINE_READY start = id:%llu\n", id));
		C->status = COROUTINE_RUNNING;
		DEBUG_PRINTF(("\tc >> coroutine_resume before make_fcontext = id:%llu\n", id));
		// C->fctx = make_fcontext((coro_ptr_diff_t)(S->stack) + (coro_ptr_diff_t)(S->stack_size), S->stack_size, &fcontext_entry);
		C->fctx = make_fcontext(S->stack_top, S->stack_size, &fcontext_entry);

		DEBUG_PRINTF(("\tc >> coroutine_resume before jump_fcontext = id:%llu\n", id));
		C->fctx = jump_fcontext(C->fctx, (void *)S).fctx;

		DEBUG_PRINTF(("\tc >> coroutine_resume after jump_fcontext = id:%llu\n", id));
		C->context_holder_size = calc_stack_size((void *)(C->fctx), S->stack_top);
		C->context_holder = malloc(C->context_holder_size);
		memcpy(C->context_holder, (void *)(C->fctx), C->context_holder_size);
		DEBUG_PRINTF(("\tc >> coroutine_resume saving stack. context_holder: %p; C->fctx: %p; context_holder_size: %d; = id:%d\n", C->context_holder, (void *)(C->fctx), C->context_holder_size, id));
		DEBUG_PRINTF(("\tc >> coroutine_resume COROUTINE_READY end = id:%llu\n", id));
		break;
	case COROUTINE_SUSPEND:
		DEBUG_PRINTF(("\tc >> coroutine_resume COROUTINE_SUSPEND start = id:%llu\n", id));
		// void *sp = (void*)((coro_ptr_diff_t)(S->stack_top) - (coro_ptr_diff_t)(C->context_holder_size) + 1);
		// memcpy(sp, C->context_holder, C->context_holder_size);
		memcpy((void *)(C->fctx), C->context_holder, C->context_holder_size);
		free(C->context_holder);
		C->context_holder = NULL;
		C->context_holder_size = 0;

		C->status = COROUTINE_RUNNING;

		DEBUG_PRINTF(("\tc >> coroutine_resume before jump_fcontext = id:%llu\n", id));
		C->fctx = jump_fcontext(C->fctx, (void *)S).fctx;
		DEBUG_PRINTF(("\tc >> coroutine_resume after jump_fcontext = id:%llu\n", id));

		if (COROUTINE_DEAD == C->status)
		{
			DEBUG_PRINTF(("\tc >> coroutine_resume COROUTINE_DEAD = id:%llu\n", id));
			coro_ptr_diff_t real_stack_size = calc_stack_size((void *)(C->fctx), S->stack_top);
			DEBUG_PRINTF(("\tc >> coroutine_resume real stack size. context_holder_size: %d; = id:%d\n", real_stack_size, id));
		}
		else
		{
			DEBUG_PRINTF(("\tc >> coroutine_resume !COROUTINE_DEAD = id:%llu\n", id));
			C->context_holder_size = calc_stack_size((void *)(C->fctx), S->stack_top);
			C->context_holder = malloc(C->context_holder_size);
			memcpy(C->context_holder, (void *)(C->fctx), C->context_holder_size);
			DEBUG_PRINTF(("\tc >> coroutine_resume saving stack. context_holder: %p; C->fctx: %p; context_holder_size: %d; = id:%llu\n", C->context_holder, (void *)(C->fctx), C->context_holder_size, id));
		}
		DEBUG_PRINTF(("\tc >> coroutine_resume COROUTINE_SUSPEND end = id:%llu\n", id));
		break;
	default:
		assert(0);
	}

	S->running = 0;
	S->current_coro = NULL;
	DEBUG_PRINTF(("\tc >> coroutine_resume end = id:%llu\n", id));
}

coro_id coroutine_id(coroutine_t co)
{
	return co->id;
}

int coroutine_status(coroutine_t co)
{
	return co->status;
}

coroutine_t coroutine_running(schedule_t S)
{
	DEBUG_PRINTF(("\tc >> coroutine_running start = id:%llu\n", S->running));
	DEBUG_PRINTF(("\tc >> coroutine_running end = id:%llu\n", S->running));
	return S->current_coro;
}

void coroutine_yield(schedule_t S)
{
	DEBUG_PRINTF(("\tc >> coroutine_yield start = id:%llu\n", S->running));
	coro_id id = S->running;
	assert(id > 0);
	struct coroutine *C = S->current_coro;
	C->status = COROUTINE_SUSPEND;
	S->sp = get_stack_pointer();
	DEBUG_PRINTF(("\tc >> coroutine_yield before jump_fcontext = id:%llu\n", S->running));
	S->current_coro->wayback_fctx = jump_fcontext(S->current_coro->wayback_fctx, NULL).fctx;
	DEBUG_PRINTF(("\tc >> coroutine_yield end = id:%llu\n", S->running));
}

void coroutine_delete(struct coroutine *co) 
{
	_co_delete(co);
}
