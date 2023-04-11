// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>

#include "coroutine.h"
#include "scheduler.h"

struct my_payload {
   long long iter_index;
};

void serve_coroutine(void* coro_payload, struct ServerData *server_data)
{
   struct my_payload* payload = (struct my_payload*) coro_payload;
   // int current_id = server_current_coro(server_data);
   int current_id = -1;
   printf("SERVE COROUTINE %d: INDEX %d\n", current_id, payload->iter_index);
   printf("SERVE COROUTINE %d: BEFORE YIELD\n", current_id);
   if (server_data)
   {
      server_request(server_data, CoroRequestYield, NULL);
   } else {
      printf("SERVE COROUTINE %d: server_data is NULL!\n\n", current_id);
   }
   printf("SERVE COROUTINE %d: AFTER YIELD\n\n", current_id);
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
void listen_loop_coro()
{
   printf("SERVER PAUSED BEFORE DATA CREATION.\n");
   getchar();
   struct ServerData *server_data = server_create();
   printf("SERVER DATA CREATED. PAUSED.\n");
   getchar();

   long long iter_index = 0;

   struct my_payload payload;
   payload.iter_index = 0; 

   for (;;)
   {
      printf("ITERATION %d START\n", payload.iter_index);
      int child_id;
      int max_coro_qnt = 10000;
      if (max_coro_qnt > payload.iter_index)
      {
         if ((max_coro_qnt>>1) == payload.iter_index)
         {
            printf("LOOP PAUSED ON ITERATION %d\n", payload.iter_index);
            int c = getchar();
         }
         printf("BEFORE REGISTER NEW CORO\n");
         child_id = server_register_coro(server_data, serve_coroutine, &payload);
         printf("NEW CORO %d REGISTERED\n\n", child_id);
      }
      else if ((max_coro_qnt + 20) == payload.iter_index)
      {
         printf("LOOP PAUSED ON ITERATION %d\n", payload.iter_index);
         int c = getchar();
      }
      else if ((max_coro_qnt + 60) == payload.iter_index)
      {
         printf("LOOP FINISHED ON ITERATION %d\n", payload.iter_index);
         break;
      }

      server_loop_iteration(server_data);
      printf("ITERATION %d END\n", payload.iter_index);
      payload.iter_index++;
   }
   printf("SERVER PAUSED BEFORE DATA FREE.\n");
   getchar();
   server_free(server_data);
   printf("SERVER DATA FREE. PAUSED.\n");
   getchar();
}

int main()
{
   printf("SERVER START\n");
   listen_loop_coro();
   printf("SERVER END\n");
   return 0;
}
