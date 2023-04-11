cmake_minimum_required(VERSION 2.8)

project(coro_scheduler)

include(CheckSymbolExists)

# FCTX_ARCH: arm arm64 i386 mips32 ppc32 ppc64 x86_64
# FCTX_PLATFORM: aapcs ms sysv o32
# FCTX_COFF: elf pe macho xcoff 
# FCTX_ASM_COMPILER: gas masm armasm
# FCTX_ASM_SOURCE_EXTENSION: .S .asm

if (${CMAKE_SYSTEM_PROCESSOR} MATCHES "AMD64" 
    OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86_64"
    OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "i386"
    OR ${CMAKE_SYSTEM_PROCESSOR} MATCHES "x86")
    if (${CMAKE_SIZEOF_VOID_P} EQUAL 8)
        set(FCTX_ARCH x86_64)
    else()
        set(FCTX_ARCH i386)
    endif()
elseif(${CMAKE_SYSTEM_PROCESSOR} MATCHES "arm")
    if (CMAKE_CL_64)
        set(FCTX_ARCH arm64)
    else()
        set(FCTX_ARCH arm)
    endif()
else()
    message(FATAL_ERROR "arch ${CMAKE_SYSTEM_PROCESSOR} not supported")
endif()

if (WIN32)
    set(FCTX_PLATFORM ms)
    set(FCTX_COFF pe)
    set(FCTX_ASM_SOURCE_EXTENSION .asm)
    if (MINGW)
        set(CMAKE_ASM_COMPILER as)
        set(FCTX_ASM_COMPILER gas)
        enable_language(ASM)
    else()
        set(FCTX_ASM_COMPILER masm)
        enable_language(ASM_MASM)
        set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} /SAFESEH:NO")
        endif()
    
        add_definitions(-DCOROUTINE_HAVE_WIN32API)
else()
    set(FCTX_PLATFORM sysv)
    set(FCTX_COFF elf)
    set(FCTX_ASM_SOURCE_EXTENSION .S)
    set(FCTX_ASM_COMPILER gas)
    enable_language(ASM)

    check_symbol_exists(MAP_ANONYMOUS "sys/mman.h" HAVE_MAP_ANONYMOUS)
    if(HAVE_MAP_ANONYMOUS)
        add_definitions(-DCOROUTINE_HAVE_MMAP -DCOROUTINE_MAP_ANONYMOUS=MAP_ANONYMOUS)
    else()
        check_symbol_exists(MAP_ANON "sys/mman.h" HAVE_MAP_ANON)
        if(HAVE_MAP_ANON)
            add_definitions(-DCOROUTINE_HAVE_MMAP -DCOROUTINE_MAP_ANONYMOUS=MAP_ANON)
        endif()
    endif()

    check_symbol_exists(_SC_PAGE_SIZE "unistd.h" HAVE__SC_PAGE_SIZE)
    if(HAVE__SC_PAGE_SIZE)
        add_definitions(-DCOROUTINE_HAVE_SYSCONF -DCOROUTINE_SC_PAGE_SIZE=_SC_PAGE_SIZE)
    else()
        check_symbol_exists(_SC_PAGESIZE "unistd.h" HAVE_SC_PAGESIZE)
        if(HAVE_SC_PAGESIZE)
            add_definitions(-DCOROUTINE_HAVE_SYSCONF -DCOROUTINE_SC_PAGE_SIZE=_SC_PAGESIZE)
        endif()
    endif()

    add_definitions(-DCOROUTINE_HAVE_GETPAGESIZE)
    add_definitions(-DCOROUTINE_HAVE_ALIGNED_ALLOC)
    add_definitions(-DCOROUTINE_HAVE_POSIX_MEMALIGN)
endif()

SET(CMAKE_INCLUDE_PATH ${CMAKE_INCLUDE_PATH} "${BOOST_ROOT}/boost")
SET(CMAKE_LIBRARY_PATH ${CMAKE_LIBRARY_PATH} "${BOOST_ROOT}/boost/lib")
set(Boost_USE_STATIC_LIBS        ON)  # only find static libs
set(Boost_USE_MULTITHREADED      ON)
set(Boost_USE_STATIC_RUNTIME    OFF)
find_package(Boost 1.75.0 COMPONENTS chrono)

set(FCTX_SOURCE_SUFFIX ${FCTX_ARCH}_${FCTX_PLATFORM}_${FCTX_COFF}_${FCTX_ASM_COMPILER}${FCTX_ASM_SOURCE_EXTENSION})
if(Boost_FOUND)
    set(ASM_FOLDER "${Boost_INCLUDE_DIR}/libs/context/src/asm")
    message("Using asm from Boost: ${ASM_FOLDER}")
else()
    set(ASM_FOLDER "third_party/boost.context/src/asm")
    message("Using own asm: ${ASM_FOLDER}")
endif()
set(ASM_FILES ${ASM_FOLDER}/make_${FCTX_SOURCE_SUFFIX} ${ASM_FOLDER}/jump_${FCTX_SOURCE_SUFFIX} ${ASM_FOLDER}/ontop_${FCTX_SOURCE_SUFFIX})

add_definitions(-DBOOST_CONTEXT_EXPORT)
add_library(fcontext fcontext.h fcontext.c ${ASM_FILES})

add_library(stack_alloc stack_alloc.h stack_alloc.c)

add_library(coroutine coroutine.h coroutine.c)
target_link_libraries(coroutine PRIVATE fcontext stack_alloc)

add_library(scheduler scheduler.h scheduler.c)
target_link_libraries(scheduler PRIVATE coroutine)

add_executable(coro_test coro_test.c)
target_link_libraries(coro_test PRIVATE coroutine)

add_executable(coro_manager_poc coro_manager_poc.c)
target_link_libraries(coro_manager_poc PRIVATE coroutine)

add_executable(scheduler_experiments scheduler_experiments.c)
target_link_libraries(scheduler_experiments PRIVATE scheduler)

if(Boost_FOUND)
  include_directories(${Boost_INCLUDE_DIRS})
  add_executable(scheduler_experiments_cpp scheduler_experiments.cpp)
  SET_TARGET_PROPERTIES(scheduler_experiments_cpp PROPERTIES COMPILE_FLAGS "/GL")
  SET_TARGET_PROPERTIES(scheduler_experiments_cpp PROPERTIES LINK_FLAGS "/LTCG")
  string(REPLACE "/EHsc" "/EHs" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
  target_link_libraries(scheduler_experiments_cpp PRIVATE ${Boost_LIBRARIES} scheduler)
endif()