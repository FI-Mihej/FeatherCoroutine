// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#ifndef C_COROUTINE_H
#define C_COROUTINE_H

#if 1
#define DEBUG_PRINTF(a) printf a
#else
#define DEBUG_PRINTF(a) (void)0
#endif

#define COROUTINE_DEAD 0
#define COROUTINE_READY 1
#define COROUTINE_RUNNING 2
#define COROUTINE_SUSPEND 3

struct schedule;
typedef struct schedule *schedule_t;
struct coroutine;
typedef struct coroutine *coroutine_t;
typedef unsigned long long coro_id;

typedef void (*coroutine_func)(schedule_t *S, void *payload);

#ifdef __cplusplus
extern "C"{
#endif 
schedule_t coro_server_open(void);
void coro_server_close(schedule_t);

coroutine_t coroutine_new(schedule_t S, coroutine_func func, void *payload, coro_id id);
void coroutine_resume(schedule_t , coroutine_t);
coro_id coroutine_id(coroutine_t co);
int coroutine_status(coroutine_t);
coroutine_t coroutine_running(schedule_t );
void coroutine_yield(schedule_t );
void coroutine_delete(struct coroutine *);
#ifdef __cplusplus
}
#endif

#endif
