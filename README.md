# About FeatherCoroutine

Introducing a lightweight C library designed to facilitate asynchronous programming with memory-efficient coroutines and a microkernel-inspired main loop. This library is tailor-made for applications that demand minimal memory usage, including background operations on iOS devices with strict memory constraints.

## ! Note

Current state: Proof-of-Concept (POC)

Updates made after May 2021 were not committed to the repository and have been lost due to data loss experienced as a consequence of the Russia's war against the Ukrane. As a result, I plan to resume work on the project from the most recent available point.

## Key Features

* Memory-efficient coroutines: This library minimizes memory consumption by saving only essential parts of the stack for each coroutine, unlike other implementations that typically allocate at least 1 MB of stack per coroutine.
* Microkernel-inspired main loop: The main loop manages coroutines and services using a minimal core, with additional functionality provided by services. This design enables extensibility and flexibility while keeping the core small and efficient.
* Coroutine-service interaction: Coroutines send requests to services through the main loop's API and yield control back to the main loop, which then dispatches requests to services and returns responses to coroutines. This approach allows for seamless concurrency and load balancing among coroutines and services.
* iOS compatibility: The library's low memory footprint makes it suitable for use on iOS devices, which enforce strict memory limits for background operations.

## Workflow Overview

Basically the same as [Cengal.coroutines](https://github.com/FI-Mihej/Cengal) has.

1. Coroutine sends a request to a chosen service using the main loop's API and yields control back to the main loop.
1. Main loop stores the request for later processing and resumes execution of the next available coroutine.
1. Subsequent coroutines send their requests to services and yield control back to the main loop.
1. When all coroutines have either completed or sent requests, the main loop processes the services one by one, providing them with the stored requests.
1. After receiving responses from the services, the main loop delivers the responses to the corresponding coroutines one at a time, either waiting for the coroutine to complete or for the next request to be issued.
1. The cycle repeats, enabling efficient and concurrent execution of coroutines and services.

## Summary

This library is perfect for developers seeking a memory-efficient solution for asynchronous programming in C, particularly in environments with stringent memory constraints such as iOS background tasks.

## Example

```c
static void
foo(struct ServerData *server_data, int n)
// coroutine must have server_data parameter; other parameters are up to developer
{
   int i;
   for (i = 0; i < n; i++) {
        // yields a CoroRequestSleep request to the Sleep service and returns execution to the main loop until response will be issued by the Sleep service 
        server_request(server_data, CoroRequestSleep, 0.01);
   }
}
```

Development POC:

* [coro_manager_poc.c](coro_manager_poc.c)
* [scheduler_experiments.c](scheduler_experiments.c)
* [scheduler_experiments.cpp](scheduler_experiments.cpp)

## Build

Depends on [boost.context](https://github.com/boostorg/context)'s ASM files. You may provide `Boost_INCLUDE_DIR` env var in order to use ASM files from the specific Boost version.

## License

Copyright © 2018-2023 ButenkoMS. All rights reserved.

Licensed under the Apache License, Version 2.0.

## Third-party licenses

Boost Software License - Version 1.0
