/*
 * Copyright (c) 2011-2012 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */


#include <core_blas.h>
#include "dplasma.h"
#include "dplasma/lib/dplasmatypes.h"
#include "dplasma/lib/dplasmaaux.h"
#include "dplasma/lib/memory_pool.h"

#include "zherbt_L.h"

dague_handle_t *
dplasma_zherbt_New( PLASMA_enum uplo, int IB,
                    tiled_matrix_desc_t *A,
                    tiled_matrix_desc_t *T)
{
    dague_zherbt_L_handle_t *dague_zherbt = NULL;
    dague_memory_pool_t *pool[4];

    if( PlasmaLower != uplo ) {
        dplasma_error("dplasma_zherbt_New", "illegal value of uplo");
        return NULL;
    }

    pool[0] = (dague_memory_pool_t*)malloc(sizeof(dague_memory_pool_t));  /* tau */
    dague_private_memory_init( pool[0], (sizeof(dague_complex64_t)*T->nb) );

    pool[1] = (dague_memory_pool_t*)malloc(sizeof(dague_memory_pool_t));  /* work */
    dague_private_memory_init( pool[1], (sizeof(dague_complex64_t)*T->nb*IB) );

    pool[2] = (dague_memory_pool_t*)malloc(sizeof(dague_memory_pool_t));  /* work for HERFB1 */
    dague_private_memory_init( pool[2], (sizeof(dague_complex64_t)*T->nb*2 *T->nb) );

    pool[3] = (dague_memory_pool_t*)malloc(sizeof(dague_memory_pool_t));  /* work for the TSMQRLR */
    dague_private_memory_init( pool[3], (sizeof(dague_complex64_t)*T->nb*4 *T->nb) );

    if( PlasmaLower == uplo ) {
        dague_zherbt = dague_zherbt_L_new(uplo, IB,
                                          *A, (dague_ddesc_t*)A,
                                          *T, (dague_ddesc_t*)T,
                                          pool[3], pool[2], pool[1], pool[0]);
        dplasma_add2arena_rectangle( dague_zherbt->arenas[DAGUE_zherbt_L_DEFAULT_ARENA],
                                     A->mb*A->nb*sizeof(dague_complex64_t),
                                     DAGUE_ARENA_ALIGNMENT_SSE,
                                     MPI_DOUBLE_COMPLEX, A->mb, A->nb, -1);
        dplasma_add2arena_rectangle( dague_zherbt->arenas[DAGUE_zherbt_L_LITTLE_T_ARENA],
                                     T->mb*T->nb*sizeof(dague_complex64_t),
                                     DAGUE_ARENA_ALIGNMENT_SSE,
                                     MPI_DOUBLE_COMPLEX, T->mb, T->nb, -1);
    }

    return (dague_handle_t*)dague_zherbt;
}

void dplasma_zherbt_Destruct( dague_handle_t *o )
{
    dague_zherbt_L_handle_t *dague_zherbt = (dague_zherbt_L_handle_t *)o;

    if( PlasmaLower == dague_zherbt->uplo ) {

        dague_matrix_del2arena( dague_zherbt->arenas[DAGUE_zherbt_L_DEFAULT_ARENA   ] );
        dague_matrix_del2arena( dague_zherbt->arenas[DAGUE_zherbt_L_LITTLE_T_ARENA  ] );

        dague_private_memory_fini( dague_zherbt->pool_0 );
        free( dague_zherbt->pool_0 );
        dague_private_memory_fini( dague_zherbt->pool_1 );
        free( dague_zherbt->pool_1 );
        dague_private_memory_fini( dague_zherbt->pool_2 );
        free( dague_zherbt->pool_2 );
        dague_private_memory_fini( dague_zherbt->pool_3 );
        free( dague_zherbt->pool_3 );

        DAGUE_INTERNAL_HANDLE_DESTRUCT(dague_zherbt);
    }
}
