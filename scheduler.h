// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#ifndef C_SCHEDULER_H
#define C_SCHEDULER_H

#include <stddef.h>
#include <stdbool.h>

#include "coroutine.h"


enum CellType
{
   CellTypeUnusedCell = -2,
   CellTypeFreeCell,
   CellTypeUsedCell,
};
typedef enum CellType corotype_t;

enum CoroRequests
{
   CoroRequestNone,
   CoroRequestYield,
   CoroRequestRevertSign,
   CoroRequestSleep,
   CoroRequestSocketRead,
   CoroRequestSocketWrite
};
typedef enum CoroRequests cororequest_t;

struct CoroData
{
   enum CellType cell_type;
   coroutine_t coro;
   void *data;
};

struct ServerData
{
   schedule_t shed;
   int coro_list_len;
   struct CoroData *coro_list; // data is respone
   int pending_coro_list_len;
   struct CoroData *pending_coro_list; // data is request
   int last_live_coroutines_num;

   enum CoroRequests coro_request_type;
   void *request;
   void *response;
};

struct RequestData
{
   enum CoroRequests coro_request_type;
   void *request;
};

typedef void (*coroutine_callable)(void* coro_payload, struct ServerData *server_data);

struct CoroArgs
{
   struct ServerData *server_data;
   coroutine_callable coroutine_body;
   void* coro_payload;
};

#ifdef __cplusplus
extern "C"{
#endif 
struct ServerData *server_create();
int server_register_coro(struct ServerData *server_data, coroutine_callable coroutine_body, void* coro_payload);
void *server_request(struct ServerData *server_data,
                            enum CoroRequests coro_request_type,
                            void *request);
coroutine_t server_current_coro(struct ServerData *server_data);
bool server_loop_iteration(struct ServerData *server_data);
void server_free(struct ServerData *server_data);
#ifdef __cplusplus
}
#endif

static void serv_coro(schedule_t S, void *ud);
static int find_free_cell(struct CoroData *coro_list, int coro_list_len);
static void server_put_coro_to_list(struct CoroData *coro_list,
                                    int coro_index, enum CellType cell_type, coroutine_t coro, void *data);
static void *memcp_to_bigger(void *data, long long data_size, long long new_size, int default_value);
static int put_or_realloc(struct CoroData **coro_list_ptr, int *coro_list_len_ptr, enum CellType cell_type, coroutine_t coro);
static void server_free_request(struct ServerData *server_data);
static void server_free_response(struct ServerData *server_data);
static void server_free_coro_args(struct CoroArgs *coro_args);
static void server_free_request_data(struct RequestData *request_data);
static void server_free_response_data(void *response);
static void mark_cell_as_unused(struct CoroData *coro_list, int coro_index);
static void mark_cell_as_free(struct CoroData *coro_list, int coro_index);
static void mark_coro_free_or_unused(struct CoroData *coro_list, const int coro_list_len, const int coro_index);
static void server_remove_coro(struct ServerData *server_data, int coro_index);
static void server_remove_pending_coro(struct ServerData *server_data, int coro_index);
static int server_loop_coro(struct ServerData *server_data);
static void server_move_request_to_services(struct ServerData *server_data, int coro_index);
static void server_move_response_to_coro(struct ServerData *server_data, coroutine_t coro, void *response);
static void server_put_request_to_service(struct ServerData *server_data, coroutine_t coro, struct RequestData *request_data);
static void server_run_all_services(struct ServerData *server_data);
static void server_loop_services(struct ServerData *server_data);

#endif
