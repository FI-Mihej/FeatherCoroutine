// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "coroutine.h"
#include "scheduler.h"


static void serv_coro(schedule_t S, void *ud)
{
   struct CoroArgs *coro_args = (struct CoroArgs *)ud;
   coro_args->coroutine_body(coro_args->coro_payload, coro_args->server_data);
   server_free_coro_args(coro_args);
}

static int find_free_cell(struct CoroData *coro_list, int coro_list_len)
{
   for (int i = 0; i < coro_list_len; i++)
   {
      if (CellTypeUsedCell > coro_list[i].cell_type)
      {
         return i;
      }
   }
   return -1;
}

static void server_put_coro_to_list(struct CoroData *coro_list,
                                    int coro_index, enum CellType cell_type, coroutine_t coro, void *data)
{
   coro_list[coro_index].cell_type = cell_type;
   coro_list[coro_index].coro = coro;
   coro_list[coro_index].data = data;
}

static void *memcp_to_bigger(void *data, long long data_size, long long new_size, int default_value)
{
   if (new_size < data_size)
   {
      return NULL;
   }
   void *new_data = malloc(new_size);
   if (!new_data)
   {
      return NULL;
   }
   // memset(new_data, default_value, new_size);  // We don't need this since we will fill data by our code in put_or_realloc()
   memcpy(new_data, data, data_size);
   free(data);
   return new_data;
}

static int put_or_realloc(struct CoroData **coro_list_ptr, int *coro_list_len_ptr, enum CellType cell_type, coroutine_t coro)
{
   int add_coro_list_len = 1024;
   int free_cell = find_free_cell(*coro_list_ptr, (*coro_list_len_ptr));
   if (0 <= free_cell)
   {
      server_put_coro_to_list(*coro_list_ptr, free_cell, cell_type, coro, NULL);
   }
   else
   {
      int old_coro_list_len = (*coro_list_len_ptr);
      int old_coro_list_size = sizeof(struct CoroData) * old_coro_list_len;
      int new_coro_list_len = old_coro_list_len + add_coro_list_len;
      (*coro_list_len_ptr) = new_coro_list_len;
      int new_coro_list_size = sizeof(struct CoroData) * new_coro_list_len;
      (*coro_list_ptr) = memcp_to_bigger(
          (void *)(*coro_list_ptr),
          old_coro_list_size,
          new_coro_list_size,
          -1);
      if (!(*coro_list_ptr))
      {
         (*coro_list_len_ptr) = 0;
         return -1;
      }
      if (add_coro_list_len) {
         for(int i = old_coro_list_len; i < new_coro_list_len; i++) {
            mark_cell_as_unused((*coro_list_ptr), i);
         }
      }
      free_cell = find_free_cell((*coro_list_ptr), (*coro_list_len_ptr));
      if (0 <= free_cell)
      {
         server_put_coro_to_list((*coro_list_ptr), free_cell, cell_type, coro, NULL);
      }
      else
      {
         return -1;
      }
   }
   return free_cell;
}

static void server_free_request(struct ServerData *server_data)
{
   if (!server_data)
   {
      return;
   }
   if (server_data->request)
   {
      free(server_data->request);
      server_data->request = NULL;
   }
   server_data->coro_request_type = CoroRequestNone;
}

static void server_free_response(struct ServerData *server_data)
{
   if (!server_data)
   {
      return;
   }
   if (server_data->response)
   {
      free(server_data->response);
      server_data->response = NULL;
   }
}

void *server_request(struct ServerData *server_data,
                            enum CoroRequests coro_request_type,
                            void *request)
{
   if (!server_data)
   {
      return NULL;
   }
   server_free_request(server_data);
   server_free_response(server_data);
   server_data->coro_request_type = coro_request_type;
   server_data->request = request;
   coroutine_yield(server_data->shed);
   return server_data->response;
}

coroutine_t server_current_coro(struct ServerData *server_data)
{
   coroutine_t result = NULL;
   if(server_data && server_data->shed) {
      result = coroutine_running(server_data->shed);
   }

   return result;
}

int server_register_coro(struct ServerData *server_data, coroutine_callable coroutine_body, void* coro_payload)
{
   if (!server_data)
   {
      return -1;
   }
   int coro_index = put_or_realloc(&(server_data->coro_list), &(server_data->coro_list_len), CellTypeFreeCell, NULL);
   if (0 > coro_index)
   {
      return -1;
   }
   struct CoroArgs *coro_args = (struct CoroArgs *)malloc(sizeof(*coro_args));
   if (coro_args) {
       coro_args->server_data = server_data;
       coro_args->coroutine_body = coroutine_body;
       coro_args->coro_payload = coro_payload;
   } else {
       return -1;
   }
   coroutine_t coro = coroutine_new(server_data->shed, serv_coro, coro_args, NULL);
   coro_index = put_or_realloc(&(server_data->coro_list), &(server_data->coro_list_len), CellTypeUsedCell, coro);
   return coro_index;
}

static void server_free_coro_args(struct CoroArgs *coro_args)
{
   free(coro_args);
}

static void server_free_request_data(struct RequestData *request_data)
{
   if (!request_data)
   {
      return;
   }
   request_data->coro_request_type = CoroRequestNone;
   if (request_data->request)
   {
      free(request_data->request);
      request_data->request = NULL;
   }
   free(request_data);
}

static void server_free_response_data(void *response)
{
   if (!response)
   {
      return;
   }
   free(response);
}

static void mark_cell_as_unused(struct CoroData *coro_list, int coro_index)
{
   coro_list[coro_index].cell_type = CellTypeUnusedCell;
   coro_list[coro_index].coro = NULL;
   coro_list[coro_index].data = NULL;
}

static void mark_cell_as_free(struct CoroData *coro_list, int coro_index)
{
   coro_list[coro_index].cell_type = CellTypeFreeCell;
   coro_list[coro_index].coro = NULL;
   coro_list[coro_index].data = NULL;
}

static void mark_coro_free_or_unused(struct CoroData *coro_list, const int coro_list_len, const int coro_index)
{
   if (coro_index >= coro_list_len) {
      return;
   }

   if ((coro_list_len - 1) == coro_index) {
      mark_cell_as_unused(coro_list, coro_index);
   } else if (CellTypeUnusedCell == coro_list[coro_index + 1].cell_type) {
      mark_cell_as_unused(coro_list, coro_index);
   } else {
      mark_cell_as_free(coro_list, coro_index);
   }
}

static void server_remove_coro(struct ServerData *server_data, int coro_index)
{
   mark_coro_free_or_unused(server_data->coro_list, server_data->coro_list_len, coro_index);
}

static void server_remove_pending_coro(struct ServerData *server_data, int coro_index)
{
   mark_coro_free_or_unused(server_data->pending_coro_list, server_data->pending_coro_list_len, coro_index);
}

static int server_loop_coro(struct ServerData *server_data)
{
   if (!server_data)
   {
      return -1;
   }
   int coro_num = 0;
   for (int i = 0; i < server_data->coro_list_len; i++)
   {
      enum CellType cell_type = server_data->coro_list[i].cell_type;
      coroutine_t coro = server_data->coro_list[i].coro;
      void *coro_data = server_data->coro_list[i].data;
      if (CellTypeUsedCell > cell_type)
      {
         if (CellTypeFreeCell == cell_type) {
            continue;
         } else if (CellTypeUnusedCell == cell_type){
            break;
         } else {
            assert(0);
         }
      }
      bool need_to_remove_coro = false;
      if (coroutine_status(coro))
      {
         server_free_request(server_data);
         server_data->response = NULL;
         if (coro_data)
         {
            server_data->response = coro_data;
         }
         coroutine_resume(server_data->shed, coro);
         server_free_response(server_data);
         if (coroutine_status(coro))
         {
            server_move_request_to_services(server_data, i);
         }
         else
         {
            need_to_remove_coro = true;
         }
      }
      else
      {
         need_to_remove_coro = true;
      }
      if (need_to_remove_coro)
      {
         void* data = server_data->coro_list[i].data;
         if (data)
         {
            free(data);
            server_data->coro_list[i].data = NULL;
         }
         server_remove_coro(server_data, i);
      } else {
         coro_num++;
      }
   }
   server_free_request(server_data);
   server_free_response(server_data);
   return coro_num;
}

static void server_move_request_to_services(struct ServerData *server_data, int coro_index)
{
   enum CellType cell_type = server_data->coro_list[coro_index].cell_type;
   coroutine_t coro = server_data->coro_list[coro_index].coro;
   enum CoroRequests coro_request_type = server_data->coro_request_type;
   if (!coro_request_type) {
      return;
   }
   void *request = server_data->request;

   int pending_coro_index = put_or_realloc(&(server_data->pending_coro_list), &(server_data->pending_coro_list_len), cell_type, coro);
   if (0 <= pending_coro_index)
   {
      server_data->coro_request_type = CoroRequestNone;
      server_data->request = NULL;
      server_remove_coro(server_data, coro_index);
   } else {
      // server_data->pending_coro_list[coro_index].data = NULL;  // this is already done in put_or_realloc
      return;
   }

   struct RequestData *request_data = (struct RequestData *)malloc(sizeof(struct RequestData));
   request_data->coro_request_type = coro_request_type;
   request_data->request = request;
   server_data->pending_coro_list[pending_coro_index].data = (void *)request_data;
}

static void server_move_response_to_coro(struct ServerData *server_data, coroutine_t coro, void *response)
{
   int coro_index = put_or_realloc(&(server_data->coro_list), &(server_data->coro_list_len), coroutine_id(coro), coro);
   if (0 > coro_index)
   {
      server_free_response_data(response);
      return;
   }
   server_data->coro_list[coro_index].data = response;
}

static void server_put_request_to_service(struct ServerData *server_data, coroutine_t coro, struct RequestData *request_data)
{
   switch (request_data->coro_request_type)
   {
   case CoroRequestSocketRead:
   {
      break;
   }
   case CoroRequestSleep:
   {
      break;
   }
   case CoroRequestRevertSign:
   {
      int * num = (int*)request_data->request;
      int *result = (int*)malloc(sizeof(*result));
      *result = -(*num);
      server_move_response_to_coro(server_data, coro, result);
      break;
   }
   case CoroRequestYield:
   default:
   {
      server_move_response_to_coro(server_data, coro, NULL);
   }
   }
}

static void server_run_all_services(struct ServerData *server_data)
{
}

static void server_loop_services(struct ServerData *server_data)
{
   if (!server_data)
   {
      return;
   }
   for (int i = 0; i < server_data->pending_coro_list_len; i++)
   {
      enum CellType cell_type = server_data->pending_coro_list[i].cell_type;
      coroutine_t coro = server_data->pending_coro_list[i].coro;
      if (CellTypeUsedCell > cell_type)
      {
         if (CellTypeFreeCell == cell_type) {
            continue;
         } else if (CellTypeUnusedCell == cell_type){
            break;
         } else {
            assert(0);
         }
      }
      struct RequestData *request_data = (struct RequestData *)(server_data->pending_coro_list[i].data);
      server_data->pending_coro_list[i].data = NULL;
      server_remove_pending_coro(server_data, i);
      server_put_request_to_service(server_data, coro, request_data);
      server_free_request_data(request_data);
   }
   server_run_all_services(server_data);
}

bool server_loop_iteration(struct ServerData *server_data)
{
   bool need_to_proceed = false;
   if (!server_data)
   {
      return;
   }
   int live_coro_num = server_loop_coro(server_data);
   server_data->last_live_coroutines_num = live_coro_num;
   server_loop_services(server_data);
   if (live_coro_num) {
      need_to_proceed = true;
   }
   return need_to_proceed;
}

struct ServerData *server_create()
{
   struct ServerData *server_data = (struct ServerData *)malloc(sizeof(struct ServerData));
   server_data->shed = coro_server_open();

   server_data->coro_list_len = 1024;
   size_t coro_list_size = sizeof(struct CoroData) * server_data->coro_list_len;
   server_data->coro_list = (struct CoroData *)malloc(coro_list_size);
   memset(server_data->coro_list, 0, coro_list_size);
   for (int i = 0; i < server_data->coro_list_len; i++)
   {
      server_data->coro_list[i].cell_type = CellTypeUnusedCell;
      server_data->coro_list[i].coro = NULL;
      server_data->coro_list[i].data = NULL;
   }

   server_data->pending_coro_list_len = server_data->coro_list_len;
   size_t pending_coro_list_size = sizeof(struct CoroData) * server_data->pending_coro_list_len;
   server_data->pending_coro_list = (struct CoroData *)malloc(pending_coro_list_size);
   memset(server_data->pending_coro_list, 0, pending_coro_list_size);
   for (int i = 0; i < server_data->pending_coro_list_len; i++)
   {
      server_data->pending_coro_list[i].cell_type = CellTypeUnusedCell;
      server_data->pending_coro_list[i].coro = NULL;
      server_data->pending_coro_list[i].data = NULL;
   }

   server_data->coro_request_type = CoroRequestNone;
   server_data->request = NULL;
   server_data->response = NULL;
   return server_data;
}

void server_free(struct ServerData *server_data)
{
   if (!server_data)
   {
      return;
   }

   for (int i = 0; i < server_data->coro_list_len; i++)
   {
      enum CellType cell_type = server_data->coro_list[i].cell_type;
      if (CellTypeUsedCell <= cell_type) {
         server_data->coro_list[i].cell_type = CellTypeUnusedCell;
         coroutine_delete(server_data->coro_list[i].coro);
         server_data->coro_list[i].coro = NULL;
         void *data = server_data->coro_list[i].data;
         if (data)
         {
            free(data);
            server_data->coro_list[i].data = NULL;
         }
      } else if (CellTypeFreeCell == cell_type) {
         server_data->coro_list[i].cell_type = CellTypeUnusedCell;
         server_data->coro_list[i].coro = NULL;
         server_data->coro_list[i].data = NULL;
         continue;
      } else if (CellTypeUnusedCell == cell_type) {
         server_data->coro_list[i].data = NULL;
         break;
      } else {
         assert(0);
      }
   }
   server_data->coro_list_len = 0;
   free(server_data->coro_list);
   server_data->coro_list = NULL;

   for (int i = 0; i < server_data->pending_coro_list_len; i++)
   {
      enum CellType cell_type = server_data->pending_coro_list[i].cell_type;
      if (CellTypeUsedCell <= cell_type) {
         server_data->pending_coro_list[i].cell_type = CellTypeUnusedCell;
         coroutine_delete(server_data->coro_list[i].coro);
         void *data = server_data->pending_coro_list[i].data;
         if (data)
         {
            free(data);
            server_data->pending_coro_list[i].data = NULL;
         }
      } else if (CellTypeFreeCell == cell_type) {
         server_data->pending_coro_list[i].cell_type = CellTypeUnusedCell;
         server_data->pending_coro_list[i].coro = NULL;
         server_data->pending_coro_list[i].data = NULL;
         continue;
      } else if (CellTypeUnusedCell == cell_type) {
         server_data->pending_coro_list[i].data = NULL;
         break;
      } else {
         assert(0);
      }
   }
   server_data->pending_coro_list_len = 0;
   free(server_data->pending_coro_list);
   server_data->pending_coro_list = NULL;
   coro_server_close(server_data->shed);
   server_data->coro_request_type = CoroRequestNone;
   if (server_data->request)
   {
      free(server_data->request);
      server_data->request = NULL;
   }
   if (server_data->response)
   {
      free(server_data->response);
      server_data->response = NULL;
   }
   free(server_data);
   server_data = NULL;
}
