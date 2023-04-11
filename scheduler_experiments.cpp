// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#include <stdio.h>
#include <stdlib.h>
#include <stddef.h>
#include <stdbool.h>
#include <string.h>
#include <iostream>

#include <boost/chrono.hpp>
#include "coroutine.h"
#include "scheduler.h"

using namespace boost::chrono;

class my_payload {
public:
   my_payload(long long iter_index) {
      this->iter_index = iter_index;
   }
   long long iter_index;
};

void serve_coroutine(void* coro_payload, struct ServerData *server_data)
{
   printf("\n<<<<\n");
   printf("SC >> SERVE COROUTINE\n");
   class my_payload* payload = (class my_payload*) coro_payload;
   coro_id current_id = coroutine_id(server_current_coro(server_data));
   printf("SC %llu >> SERVE COROUTINE: iter_index == %d\n", current_id, payload->iter_index);
   printf("SC %llu >> SERVE COROUTINE: BEFORE yield\n", current_id);
   if (server_data)
   {
      int* request = (int*)malloc(sizeof(*request));
      *request = -current_id;
      int* response = (int*) server_request(server_data, CoroRequestRevertSign, (void*)request);
      printf("SC %llu >> SERVE COROUTINE: server_data response is %d!\n\n", current_id, *response);

      server_request(server_data, CoroRequestYield, NULL);
   } else {
      printf("SC %llu >> SERVE COROUTINE: server_data is NULL!\n\n", current_id);
   }
   printf("SC %llu >> SERVE COROUTINE: AFTER yield\n\n", current_id);
   delete payload;
   printf("SC %llu >> SERVE COROUTINE: AFTER delete\n\n", current_id);
   printf(">>>>\n\n");
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
   printf("\n=============\nS >> PAUSED. SERVER PAUSED BEFORE DATA CREATION.");
   getchar();
   std::cout << high_resolution_clock::now() << "\n";
   struct ServerData *server_data = server_create();
   std::cout << high_resolution_clock::now() << "\n";
   printf("S >> SERVER DATA CREATION DONE.\n=============\n");
   printf("\n=============\nS >> PAUSED. BEFORE LOOP.");
   getchar();
   std::cout << high_resolution_clock::now() << "\n";

   long long iter_index = 0;

   for (;;)
   {
      int child_id = -1;
      int max_coro_qnt = 10;
      if (max_coro_qnt > iter_index)
      {
         my_payload* payload = new my_payload(iter_index);
         child_id = server_register_coro(server_data, serve_coroutine, payload);
         if (0 > child_id) {
            printf("S >> FAILED TO CREATE CORO FOR A iter_indes == %d\n", iter_index);
         }
      }
      else if (max_coro_qnt <= iter_index)
      {
         std::cout << high_resolution_clock::now() << "\n";
         printf("S >> LOOP FINISHED ON ITERATION %d\n=============\n", iter_index);
         break;
      }

      iter_index++;
   }

   printf("\n=============\nS >> PAUSED. BEFORE SERVER PROCESSING STARTED. COROUTINES CREATED: %d.", iter_index);
   int c = getchar();
   std::cout << high_resolution_clock::now() << "\n";
   while (server_loop_iteration(server_data))
   {
      printf("S >> NUMBER OF LIVE COROUTINES: %d\n\n", server_data->last_live_coroutines_num);
   }
   std::cout << high_resolution_clock::now() << "\n";
   printf("S >> SERVER PROCESSING FINISHED.\n=============\n");

   printf("\n=============\nS >> PAUSED. SERVER PAUSED BEFORE DATA FREE.");
   getchar();
   std::cout << high_resolution_clock::now() << "\n";
   server_free(server_data);
   std::cout << high_resolution_clock::now() << "\n";
   printf("S >> SERVER DATA FREE DONE.\n=============\n");

   printf("\n=============\nS >> PAUSED. BEFORE EXIT.");
   getchar();
}

int main()
{
   printf("S >> SERVER START\n");
   listen_loop_coro();
   printf("S >> SERVER END\n");
   return 0;
}
