/*
 * Copyright (c) 2010-2017 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */

#include <math.h>
#include <stdlib.h>
#include <core_blas.h>
#include <cblas.h>
#include "parsec/parsec_config.h"
#include "dplasma.h"
#include "dplasma/lib/dplasmatypes.h"
#include "dplasma/lib/dplasmaaux.h"
#include "parsec/private_mempool.h"

#include "dplasma/lib/zhetrf.h"
#include "dplasma/lib/ztrmdm.h"
/*#include <lapacke.h>*/

/*
 * dplasma_zhetrf_New()
 */
parsec_handle_t*
dplasma_zhetrf_New( tiled_matrix_desc_t *A, int *INFO)
{
    int ldwork, lwork, ib;
    parsec_handle_t *parsec_zhetrf = NULL;
    parsec_memory_pool_t *pool_0, *pool_1;

    ib = A->mb;

    /* ldwork and lwork are necessary for the macros zhetrf_pool_0_SIZE and zhetrf_pool_1_SIZE */
    ldwork = (A->nb+1)*ib;
    lwork = (A->mb+1)*A->nb + ib*ib;

    pool_0 = (parsec_memory_pool_t*)malloc(sizeof(parsec_memory_pool_t));
    parsec_private_memory_init( pool_0, zhetrf_pool_0_SIZE );

    pool_1 = (parsec_memory_pool_t*)malloc(sizeof(parsec_memory_pool_t));
    parsec_private_memory_init( pool_1, zhetrf_pool_1_SIZE );

    parsec_zhetrf = (parsec_handle_t *)parsec_zhetrf_new(PlasmaLower, A, (parsec_ddesc_t *)A, ib, pool_1, pool_0, INFO);

    dplasma_add2arena_tile(((parsec_zhetrf_handle_t*)parsec_zhetrf)->arenas[PARSEC_zhetrf_DEFAULT_ARENA],
                           A->mb*A->nb*sizeof(parsec_complex64_t),
                           PARSEC_ARENA_ALIGNMENT_SSE,
                           parsec_datatype_double_complex_t, A->mb);

    return parsec_zhetrf;
}

void
dplasma_zhetrf_Destruct( parsec_handle_t *handle )
{
    parsec_zhetrf_handle_t *obut = (parsec_zhetrf_handle_t *)handle;

    parsec_matrix_del2arena( obut->arenas[PARSEC_zhetrf_DEFAULT_ARENA] );

    parsec_handle_free(handle);
}


/*
 * dplasma_ztrmdm_New()
 */
parsec_handle_t*
dplasma_ztrmdm_New( tiled_matrix_desc_t *A)
{
    parsec_handle_t *parsec_ztrmdm = NULL;


    parsec_ztrmdm = (parsec_handle_t *)parsec_ztrmdm_new(A);

    dplasma_add2arena_tile(((parsec_ztrmdm_handle_t*)parsec_ztrmdm)->arenas[PARSEC_ztrmdm_DEFAULT_ARENA],
                           A->mb*A->nb*sizeof(parsec_complex64_t),
                           PARSEC_ARENA_ALIGNMENT_SSE,
                           parsec_datatype_double_complex_t, A->mb);

    return parsec_ztrmdm;
}

void
dplasma_ztrmdm_Destruct( parsec_handle_t *handle )
{
    parsec_ztrmdm_handle_t *obut = (parsec_ztrmdm_handle_t *)handle;

    parsec_matrix_del2arena( obut->arenas[PARSEC_ztrmdm_DEFAULT_ARENA] );

    //parsec_ztrmdm_destroy(obut);
    parsec_handle_free(handle);
}

/*
 * Blocking Interface
 */

int dplasma_zhetrf(parsec_context_t *parsec, tiled_matrix_desc_t *A)
{
    parsec_handle_t *parsec_zhetrf/*, *parsec_ztrmdm*/;
    int info = 0, ginfo = 0;

    parsec_zhetrf = dplasma_zhetrf_New(A, &info);
    parsec_enqueue(parsec, (parsec_handle_t *)parsec_zhetrf);
    dplasma_progress(parsec);
    dplasma_zhetrf_Destruct(parsec_zhetrf);

    /*
    parsec_ztrmdm = dplasma_ztrmdm_New(A);
    parsec_enqueue(parsec, (parsec_handle_t *)parsec_ztrmdm);
    dplasma_progress(parsec);
    dplasma_ztrmdm_Destruct(parsec_ztrmdm);
    */

#if defined(PARSEC_HAVE_MPI)
    MPI_Allreduce( &info, &ginfo, 1, MPI_INT, MPI_MAX, MPI_COMM_WORLD);
#else
    ginfo = info;
#endif
    return ginfo;
}
