// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

#include "coroutine.h"

enum CoroRequests
{
   CoroRequestNone = 0,
   CoroRequestYield = 1,
   CoroRequestSleep,
   CoroRequestSocketRead,
   CoroRequestSocketWrite
};

struct CoroData
{
   int coro_id;
   void *data;
};

struct ServerData
{
   struct schedule *shed;
   int coro_list_len;
   struct CoroData *coro_list; // data is respone
   int pending_coro_list_len;
   struct CoroData *pending_coro_list; // data is request

   enum CoroRequests coro_request_type;
   void *request;
   void *response;
};

struct RequestData
{
   enum CoroRequests coro_request_type;
   void *request;
};

struct CoroArgs
{
   struct ServerData *server_data;
   struct client_state *csp;
   int n;
};

static void serv_coro(struct schedule *S, void *ud);
static void listen_loop_coro();
static int find_free_cell(struct CoroData *coro_list, int coro_list_len);
static void server_put_coro_to_list(struct CoroData *coro_list,
                                    int coro_index, int coro_id, void *data);
static void *memcp_to_bigger(void *data, long long data_size, long long new_size, int default_value);
static int put_or_realloc(struct CoroData **coro_list_ptr, int *coro_list_len_ptr, int coro_id);
static void server_free_request(struct ServerData *server_data);
static void server_free_response(struct ServerData *server_data);
static void *server_request(struct ServerData *server_data,
                            enum CoroRequests coro_request_type,
                            void *request);
static int server_register_coro(struct ServerData *server_data, int n);
static void server_free_coro_args(struct CoroArgs *coro_args);
static void server_free_request_data(struct RequestData *request_data);
static void server_free_response_data(void *response);
static void remove_coro_from_list_imp(struct CoroData *coro_list, int coro_index);
static void server_remove_coro(struct ServerData *server_data, int coro_index);
static void server_remove_pending_coro(struct ServerData *server_data, int coro_index);
static void server_loop_coro(struct ServerData *server_data);
static void server_move_request_to_services(struct ServerData *server_data, int coro_index);
static void server_move_response_to_coro(struct ServerData *server_data, int coro_id, void *response);
static void server_put_request_to_service(struct ServerData *server_data, int coro_id, struct RequestData *request_data);
static void server_run_all_services(struct ServerData *server_data);
static void server_loop_services(struct ServerData *server_data);
static void server_loop(struct ServerData *server_data);
static struct ServerData *server_create();
static void server_free(struct ServerData *server_data);

static void
foo(struct ServerData *server_data, int n)
{
    printf("COROUTINE FOO (%d) - START\n", n);
   int start = n;
   int i;
   for (i = 0; i < 500; i++)
   {
       printf("COROUTINE FOO (%d) %d : %d\n", n, coroutine_running(server_data->shed), start + i);
      if (server_data)
      {
         server_request(server_data, CoroRequestYield, NULL);
      }
   }
   printf("COROUTINE FOO (%d) - END\n", n);
}

static void
bar(struct ServerData *server_data, int n)
{
    printf("COROUTINE BAR (%d) - START\n", n);
   int start = n;
   int i;
   for (i = 0; i < 500; i++)
   {
       printf("COROUTINE BAR (%d) %d : %d\n", n, coroutine_running(server_data->shed), start + i);
      if (server_data)
      {
         server_request(server_data, CoroRequestYield, NULL);
      }
   }
   printf("COROUTINE BAR (%d) - END\n", n);
}

static void serv_coro(struct schedule *S, void *ud)
{
   struct CoroArgs *coro_args = (struct CoroArgs *)ud;
   if (coro_args->n % 2)
   {
      foo(coro_args->server_data, coro_args->n);
   }
   else
   {
      bar(coro_args->server_data, coro_args->n);
   }
   server_free_coro_args(coro_args);
}

/*********************************************************************
 *
 * Function    :  listen_loop_coro
 *
 * Description :  bind the listen port and enter a "FOREVER" listening loop.
 *
 * Parameters  :  N/A
 *
 * Returns     :  Never.
 *
 *********************************************************************/
static void listen_loop_coro()
{
   printf("SERVER PAUSED BEFORE DATA CREATION.\n");
   getchar();
   struct ServerData *server_data = server_create();
   printf("SERVER DATA CREATED. PAUSED.\n");
   getchar();

   long long iter_index = 0;

   for (;;)
   {
      DEBUG_PRINTF(("ITERATION %d START\n", iter_index));
      int child_id;
      int max_coro_qnt = 10000;
      if (max_coro_qnt > iter_index)
      {
         if ((max_coro_qnt>>1) == iter_index)
         {
            printf("LOOP PAUSED ON ITERATION %d\n", iter_index);
            int c = getchar();
         }
         DEBUG_PRINTF(("BEFORE REGISTER NEW CORO\n"));
         child_id = server_register_coro(server_data, iter_index);
         DEBUG_PRINTF(("NEW CORO %d REGISTERED\n", child_id));
      }
      else if ((max_coro_qnt + 20) == iter_index)
      {
         printf("LOOP PAUSED ON ITERATION %d\n", iter_index);
         int c = getchar();
      }
      else if ((max_coro_qnt + 60) == iter_index)
      {
         printf("LOOP FINISHED ON ITERATION %d\n", iter_index);
         break;
      }

      server_loop(server_data);
      DEBUG_PRINTF(("ITERATION %d END\n", iter_index));
      iter_index++;
   }
   printf("SERVER PAUSED BEFORE DATA FREE.\n");
   getchar();
   server_free(server_data);
   printf("SERVER DATA FREE. PAUSED.\n");
   getchar();
}

static int find_free_cell(struct CoroData *coro_list, int coro_list_len)
{
   for (int i = 0; i < coro_list_len; i++)
   {
      if (0 > coro_list[i].coro_id)
      {
         return i;
      }
   }
   return -1;
}

static void server_put_coro_to_list(struct CoroData *coro_list,
                                    int coro_index, int coro_id, void *data)
{
   coro_list[coro_index].coro_id = coro_id;
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
   memset(new_data, default_value, new_size);
   memcpy(new_data, data, data_size);
   free(data);
   return new_data;
}

static int put_or_realloc(struct CoroData **coro_list_ptr, int *coro_list_len_ptr, int coro_id)
{
   int add_coro_list_len = 1024;
   int free_cell = find_free_cell(*coro_list_ptr, (*coro_list_len_ptr));
   if (0 <= free_cell)
   {
      server_put_coro_to_list(*coro_list_ptr, free_cell, coro_id, NULL);
   }
   else
   {
      int coro_list_size = sizeof(struct CoroData) * (*coro_list_len_ptr);
      int new_coro_list_len = (*coro_list_len_ptr) + add_coro_list_len;
      (*coro_list_len_ptr) = new_coro_list_len;
      int new_coro_list_size = sizeof(struct CoroData) * new_coro_list_len;
      (*coro_list_ptr) = memcp_to_bigger(
          (void *)(*coro_list_ptr),
          coro_list_size,
          new_coro_list_size,
          -1);
      if (!(*coro_list_ptr))
      {
         (*coro_list_len_ptr) = 0;
         return -1;
      }
      free_cell = find_free_cell((*coro_list_ptr), (*coro_list_len_ptr));
      if (0 <= free_cell)
      {
         server_put_coro_to_list((*coro_list_ptr), free_cell, coro_id, NULL);
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

static void *server_request(struct ServerData *server_data,
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

static int server_register_coro(struct ServerData *server_data, int n)
{
   if (!server_data)
   {
      return -1;
   }
   int coro_index = put_or_realloc(&(server_data->coro_list), &(server_data->coro_list_len), -1);
   if (0 > coro_index)
   {
      return -1;
   }
   struct CoroArgs *coro_args = (struct CoroArgs *)malloc(sizeof(struct CoroArgs));
   coro_args->server_data = server_data;
   coro_args->n = n;
   int coro_id = coroutine_new(server_data->shed, serv_coro, coro_args, NULL);
   coro_index = put_or_realloc(&(server_data->coro_list), &(server_data->coro_list_len), coro_id);
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

static void remove_coro_from_list_imp(struct CoroData *coro_list, int coro_index)
{
   coro_list[coro_index].coro_id = -1;
   coro_list[coro_index].data = NULL;
}

static void server_remove_coro(struct ServerData *server_data, int coro_index)
{
   remove_coro_from_list_imp(server_data->coro_list, coro_index);
}

static void server_remove_pending_coro(struct ServerData *server_data, int coro_index)
{
   remove_coro_from_list_imp(server_data->pending_coro_list, coro_index);
}

static void server_loop_coro(struct ServerData *server_data)
{
   DEBUG_PRINTF(("SERVER LOOP CORO - START\n"));
   if (!server_data)
   {
      DEBUG_PRINTF(("SERVER LOOP CORO - ERROR. EXITING\n"));
      return;
   }
   for (int i = 0; i < server_data->coro_list_len; i++)
   {
      int coro_id = server_data->coro_list[i].coro_id;
      void *coro_data = server_data->coro_list[i].data;
      if (0 > coro_id)
      {
         continue;
      }
      bool need_to_remove_coro = false;
      if (coroutine_status(server_data->shed, coro_id))
      {
         server_free_request(server_data);
         if (coro_data)
         {
            server_data->response = coro_data;
         }
         coroutine_resume(server_data->shed, coro_id);
         server_free_response(server_data);
         if (coroutine_status(server_data->shed, coro_id))
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
         if (server_data->coro_list[i].data)
         {
            free(server_data->coro_list[i].data);
         }
         server_remove_coro(server_data, i);
      }
   }
   server_free_request(server_data);
   server_free_response(server_data);
   DEBUG_PRINTF(("SERVER LOOP CORO - END\n"));
}

static void server_move_request_to_services(struct ServerData *server_data, int coro_index)
{
   int coro_id = server_data->coro_list[coro_index].coro_id;
   server_remove_coro(server_data, coro_index);
   enum CoroRequests coro_request_type = server_data->coro_request_type;
   server_data->coro_request_type = CoroRequestNone;
   void *request = server_data->request;
   server_data->request = NULL;

   struct RequestData *request_data = (struct RequestData *)malloc(sizeof(struct RequestData));
   request_data->coro_request_type = coro_request_type;
   request_data->request = request;

   coro_index = put_or_realloc(&(server_data->pending_coro_list), &(server_data->pending_coro_list_len), coro_id);
   if (0 > coro_index)
   {
      return;
   }
   server_data->pending_coro_list[coro_index].data = (void *)request_data;
}

static void server_move_response_to_coro(struct ServerData *server_data, int coro_id, void *response)
{
   int coro_index = put_or_realloc(&(server_data->coro_list), &(server_data->coro_list_len), coro_id);
   if (0 > coro_index)
   {
      return;
   }
   server_data->coro_list[coro_index].data = response;
}

static void server_put_request_to_service(struct ServerData *server_data, int coro_id, struct RequestData *request_data)
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
   case CoroRequestYield:
   default:
   {
      server_move_response_to_coro(server_data, coro_id, NULL);
   }
   }
}

static void server_run_all_services(struct ServerData *server_data)
{
   DEBUG_PRINTF(("SERVER RUN ALL SERVICES - START\n"));
   DEBUG_PRINTF(("SERVER RUN ALL SERVICES - END\n"));
}

static void server_loop_services(struct ServerData *server_data)
{
   DEBUG_PRINTF(("SERVER LOOP SERVICES - START\n"));
   if (!server_data)
   {
      DEBUG_PRINTF(("SERVER LOOP SERVICES - ERROR. EXITING\n"));
      return;
   }
   for (int i = 0; i < server_data->pending_coro_list_len; i++)
   {
      int coro_id = server_data->pending_coro_list[i].coro_id;
      if (0 > coro_id)
      {
         continue;
      }
      struct RequestData *request_data = (struct RequestData *)(server_data->pending_coro_list[i].data);
      server_data->pending_coro_list[i].data = NULL;
      server_remove_pending_coro(server_data, i);
      server_put_request_to_service(server_data, coro_id, request_data);
      server_free_request_data(request_data);
   }
   server_run_all_services(server_data);
   DEBUG_PRINTF(("SERVER LOOP SERVICES - END\n"));
}

static void server_loop(struct ServerData *server_data)
{
   DEBUG_PRINTF(("SERVER LOOP - START\n"));
   if (!server_data)
   {
      DEBUG_PRINTF(("SERVER LOOP - ERROR. EXITING\n"));
      return;
   }
   server_loop_coro(server_data);
   server_loop_services(server_data);
   DEBUG_PRINTF(("SERVER LOOP - FINISH\n"));
}

static struct ServerData *server_create()
{
   struct ServerData *server_data = (struct ServerData *)malloc(sizeof(struct ServerData));
   server_data->shed = coro_server_open();

   server_data->coro_list_len = 1024;
   size_t coro_list_size = sizeof(struct CoroData) * server_data->coro_list_len;
   server_data->coro_list = (struct CoroData *)malloc(coro_list_size);
   //    memset(server_data->coro_list, -1, coro_list_size);
   for (int i = 0; i < server_data->coro_list_len; i++)
   {
      server_data->coro_list[i].coro_id = -1;
      server_data->coro_list[i].data = NULL;
   }

   server_data->pending_coro_list_len = server_data->coro_list_len;
   size_t pending_coro_list_size = sizeof(struct CoroData) * server_data->pending_coro_list_len;
   server_data->pending_coro_list = (struct CoroData *)malloc(pending_coro_list_size);
   //    memset(server_data->pending_coro_list, -1, pending_coro_list_size);
   for (int i = 0; i < server_data->pending_coro_list_len; i++)
   {
      server_data->pending_coro_list[i].coro_id = -1;
      server_data->pending_coro_list[i].data = NULL;
   }

   server_data->coro_request_type = CoroRequestNone;
   server_data->request = NULL;
   server_data->response = NULL;
   return server_data;
}

static void server_free(struct ServerData *server_data)
{
   DEBUG_PRINTF(("SERVER FREE - START\n"));
   if (!server_data)
   {
      DEBUG_PRINTF(("SERVER FREE - ERROR. EXITING\n"));
      return;
   }

   for (int i = 0; i < server_data->coro_list_len; i++)
   {
      int coro_id = server_data->coro_list[i].coro_id;
      if (0 <= coro_id)
      {
         server_data->coro_list[i].coro_id = -1;
      }
      void *data = server_data->coro_list[i].data;
      if (data)
      {
         free(data);
         server_data->coro_list[i].data = NULL;
      }
      // DEBUG_PRINTF(("SERVER FREE - CORO LIST DATA ITEM %d - FLUSHED AND FREE\n", i));
   }
   DEBUG_PRINTF(("SERVER FREE - CORO LIST DATA - FREE\n"));
   server_data->coro_list_len = 0;
   free(server_data->coro_list);
   server_data->coro_list = NULL;
   DEBUG_PRINTF(("SERVER FREE - CORO LIST - FREE\n"));

   for (int i = 0; i < server_data->pending_coro_list_len; i++)
   {
      int coro_id = server_data->pending_coro_list[i].coro_id;
      if (0 <= coro_id)
      {
         server_data->pending_coro_list[i].coro_id = -1;
      }
      void *data = server_data->pending_coro_list[i].data;
      if (data)
      {
         free(data);
         server_data->pending_coro_list[i].data = NULL;
      }
      // DEBUG_PRINTF(("SERVER FREE - PENDING CORO LIST DATA ITEM %d - FLUSHED AND FREE\n", i));
   }
   DEBUG_PRINTF(("SERVER FREE - PENDING CORO LIST DATA - FREE\n"));
   server_data->pending_coro_list_len = 0;
   free(server_data->pending_coro_list);
   server_data->pending_coro_list = NULL;
   DEBUG_PRINTF(("SERVER FREE - PENDING CORO LIST - FREE\n"));

   coro_server_close(server_data->shed);
   DEBUG_PRINTF(("SERVER FREE - COROUTINE - CLOSED\n"));

   server_data->coro_request_type = CoroRequestNone;
   DEBUG_PRINTF(("SERVER FREE - CORO REQUEST TYPE - FLUSHED\n"));
   if (server_data->request)
   {
      free(server_data->request);
      server_data->request = NULL;
   }
   DEBUG_PRINTF(("SERVER FREE - REQUEST - FLUSHED AND FREE\n"));
   if (server_data->response)
   {
      free(server_data->response);
      server_data->response = NULL;
   }
   DEBUG_PRINTF(("SERVER FREE - RESPONSE - FLUSHED AND FREE\n"));
   free(server_data);
   DEBUG_PRINTF(("SERVER FREE - SERVER DATA - FREE\n"));
   server_data = NULL;
   DEBUG_PRINTF(("SERVER FREE - END\n"));
}

int main()
{
   printf("SERVER START\n");
   listen_loop_coro();
   printf("SERVER END\n");
   return 0;
}
