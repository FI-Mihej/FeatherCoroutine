// Copyright © 2018-2023 ButenkoMS. All rights reserved.
// Licensed under the Apache License, Version 2.0.


#ifndef _FCONTEXT_H_INCLUDED
#define _FCONTEXT_H_INCLUDED

#include <stddef.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void *fcontext_t;

    typedef struct
    {
        fcontext_t fctx;
        void *data;
    } transfer_t;

    fcontext_t make_fcontext(void *sp, size_t size, void (*func)(transfer_t));
    transfer_t jump_fcontext(fcontext_t const to, void *vp);
    transfer_t ontop_fcontext(fcontext_t const to, void *vp, transfer_t (*func)(transfer_t));

#ifdef __cplusplus
}
#endif

#endif //!_FCONTEXT_H_INCLUDED