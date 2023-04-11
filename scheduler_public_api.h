// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#ifndef C_SCHEDULER_PUBLIC_API_H
#define C_SCHEDULER_PUBLIC_API_H

#include <stddef.h>
#include <stdbool.h>

#include "coroutine.h"
#include "scheduler.h"


enum CoroRequests
{
   CoroRequestNone = 0,
   CoroRequestYield = 1,
   CoroRequestSleep,
   CoroRequestSocketRead,
   CoroRequestSocketWrite
};

struct ServerData;

typedef void (*coroutine_callable)(void* coro_payload, struct ServerData *server_data);

struct ServerData *server_create();
int server_register_coro(struct ServerData *server_data, coroutine_callable coroutine_body, void* coro_payload);
void *server_request(struct ServerData *server_data,
                            enum CoroRequests coro_request_type,
                            void *request);
bool server_loop_iteration(struct ServerData *server_data);
void server_free(struct ServerData *server_data);

#endif
