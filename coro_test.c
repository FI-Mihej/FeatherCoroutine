// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#include <stdio.h>
#include <stdlib.h>

#include "coroutine.h"

struct args
{
	int n;
};

static void
foo(struct schedule *S, void *ud)
{
	struct args *arg = ud;
	int start = arg->n;
	int i;
	for (i = 0; i < 5; i++)
	{
		DEBUG_PRINTF(("coroutine FOO %d : %d\n", coroutine_running(S), start + i));
		coroutine_yield(S);
	}
	free(ud);
}

static void
bar(struct schedule *S, void *ud)
{
	struct args *arg = ud;
	int start = arg->n;
	int i;
	for (i = 0; i < 5; i++)
	{
		DEBUG_PRINTF(("coroutine BAR %d : %d\n", coroutine_running(S), start + i));
		coroutine_yield(S);
	}
	free(ud);
}

static void
temp_coro(struct schedule *S, void *ud)
{
	DEBUG_PRINTF(("coroutine TEMP START %d\n", coroutine_running(S)));
	coroutine_yield(S);
	DEBUG_PRINTF(("coroutine TEMP END %d\n", coroutine_running(S)));
	free(ud);
}

static void
test(struct schedule *S)
{
	unsigned int coro_qnt = 10000;
	int iter_qnt = 1;
	int temp_coro_qnt = 10;
	int *coro_list = (int *)malloc(sizeof(int) * coro_qnt);
	for (int i = 0; i < coro_qnt; i++)
	{
		// struct args another_arg = { i * iter_qnt };
		struct args *another_arg = (struct args *)malloc(sizeof(struct args));
		another_arg->n = i * iter_qnt;
		if (i % 2)
		{
			coro_list[i] = coroutine_new(S, foo, another_arg, NULL);
		}
		else
		{
			coro_list[i] = coroutine_new(S, bar, another_arg, NULL);
		}
	}
	printf("WAIT AFTER ALL CORO CREATION\n");
	int c = getchar();

	unsigned int effective_coro_runs = coro_qnt;
	printf("main start\n");
	unsigned int index = 0;
	while (effective_coro_runs)
	{
		unsigned int new_effective_coro_runs = 0;
		for (int i = 0; i < coro_qnt; i++)
		{
			if (coroutine_status(S, coro_list[i]))
			{
				new_effective_coro_runs += 1;
				coroutine_resume(S, coro_list[i]);

				for (int temp_coro_iter = 0; temp_coro_iter < temp_coro_qnt; temp_coro_iter++)
				{
					struct args *another_arg = (struct args *)malloc(sizeof(struct args));
					another_arg->n = temp_coro_iter;
					int temp_coro_id = coroutine_new(S, temp_coro, another_arg, NULL);
					while (coroutine_status(S, temp_coro_id))
					{
						coroutine_resume(S, temp_coro_id);
					}
				}
			}
		}
		effective_coro_runs = new_effective_coro_runs;
		index += 1;
		if (1 == index)
		{
			printf("WAIT AFTER THE FIRST ITERATION\n");
			c = getchar();
		}
	}
	printf("WAIT AFTER ALL CORO WORK DONE\n");
	c = getchar();

	free(coro_list);

	printf("main end\n");
}

int main_func()
{
	printf("WAIT AFTER START\n");
	int c = getchar();
	// int* some_buffer = (int*)malloc(1*1024*1024);
	// printf("WAIT AFTER SOME BUFFER MALLOC\n");
	// c = getchar( );
	struct schedule *S = coro_server_open();
	printf("WAIT AFTER STACK CREATION\n");
	c = getchar();
	test(S);
	coro_server_close(S);
	printf("WAIT AFTER STACK REMOVING\n");
	c = getchar();
	// free(some_buffer);
	// printf("WAIT AFTER SOME BUFFER FREE\n");
	// c = getchar( );

	return 0;
}

int main()
{
	int iter_qnt = 1;
	for (int i = 0; i < iter_qnt; i++)
	{
		main_func();
	}

	return 0;
}
