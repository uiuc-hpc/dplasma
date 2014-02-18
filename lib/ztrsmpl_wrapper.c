/*
 * Copyright (c) 2010-2012 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */

#include "dague_internal.h"
#include <core_blas.h>
#include "dplasma.h"
#include "dplasma/lib/dplasmaaux.h"
#include "dplasma/lib/dplasmatypes.h"

#include "ztrsmpl.h"
#include "ztrsmpl_sd.h"

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 * dplasma_ztrsmpl_New - Generates the object that solves U*x = b, when U has
 * been generated through LU factorization with incremental pivoting strategy
 * See dplasma_zgetrf_incpiv_New().
 *
 * WARNING: The computations are not done by this call.
 *
 *******************************************************************************
 *
 * @param[in] A
 *          Descriptor of the distributed matrix A to be factorized.
 *          On entry, The factorized matrix through dplasma_zgetrf_incpiv_New()
 *          routine.  Elements on and above the diagonal are the elements of
 *          U. Elements below the diagonal are NOT the classic L, but the L
 *          factors obtaines by succesive pivoting.
 *
 * @param[in] L
 *          Descriptor of the matrix L distributed exactly as the A matrix.
 *           - If IPIV != NULL, L.mb defines the IB parameter of the tile LU
 *          algorithm. This matrix must be of size A.mt * L.mb - by - A.nt *
 *          L.nb, with L.nb == A.nb.
 *          On entry, contains auxiliary information required to solve the
 *          system and generated by dplasma_zgetrf_inciv_New().
 *           - If IPIV == NULL, pivoting information are stored within
 *          L. (L.mb-1) defines the IB parameter of the tile LU algorithm. This
 *          matrix must be of size A.mt * L.mb - by - A.nt * L.nb, with L.nb =
 *          A.nb, and L.mb = ib+1.
 *          The first A.mb elements contains the IPIV information, the leftover
 *          contains auxiliary information required to solve the system.
 *
 * @param[in] IPIV
 *          Descriptor of the IPIV matrix. Should be distributed exactly as the
 *          A matrix. This matrix must be of size A.m - by - A.nt with IPIV.mb =
 *          A.mb and IPIV.nb = 1.
 *          On entry, contains the pivot indices of the successive row
 *          interchanged performed during the factorization.
 *          If IPIV == NULL, rows interchange information is stored within L.
 *
 * @param[in,out] B
 *          On entry, the N-by-NRHS right hand side matrix B.
 *          On exit, if return value = 0, B is overwritten by the solution matrix X.
 *
 *******************************************************************************
 *
 * @return
 *          \retval NULL if incorrect parameters are given.
 *          \retval The dague object describing the operation that can be
 *          enqueued in the runtime with dague_enqueue(). It, then, needs to be
 *          destroy with dplasma_ztrsmpl_Destruct();
 *
 *******************************************************************************
 *
 * @sa dplasma_ztrsmpl
 * @sa dplasma_ztrsmpl_Destruct
 * @sa dplasma_ctrsmpl_New
 * @sa dplasma_dtrsmpl_New
 * @sa dplasma_strsmpl_New
 *
 ******************************************************************************/
dague_handle_t *
dplasma_ztrsmpl_New(const tiled_matrix_desc_t *A,
                    const tiled_matrix_desc_t *L,
                    const tiled_matrix_desc_t *IPIV,
                    tiled_matrix_desc_t *B)
{
    dague_ztrsmpl_handle_t *dague_trsmpl = NULL; 

    if ( (A->mt != L->mt) || (A->nt != L->nt) ) {
        dplasma_error("dplasma_ztrsmpl_New", "L doesn't have the same number of tiles as A");
        return NULL;
    }
    if ( (IPIV != NULL) && ((A->mt != IPIV->mt) || (A->nt != IPIV->nt)) ) {
        dplasma_error("dplasma_ztrsmpl_New", "IPIV doesn't have the same number of tiles as A");
        return NULL;
    }

    if ( IPIV != NULL ) {
        dague_trsmpl = dague_ztrsmpl_new((dague_ddesc_t*)A,
                                         (dague_ddesc_t*)L,
                                         (dague_ddesc_t*)IPIV,
                                         (dague_ddesc_t*)B );
    }
    else {
        dague_trsmpl = (dague_ztrsmpl_handle_t*)
            dague_ztrsmpl_sd_new( (dague_ddesc_t*)A,
                                  (dague_ddesc_t*)L,
                                  NULL,
                                  (dague_ddesc_t*)B );
    }

    /* A */
    dplasma_add2arena_tile( dague_trsmpl->arenas[DAGUE_ztrsmpl_DEFAULT_ARENA],
                            A->mb*A->nb*sizeof(dague_complex64_t),
                            DAGUE_ARENA_ALIGNMENT_SSE,
                            MPI_DOUBLE_COMPLEX, A->mb );

    /* IPIV */
    dplasma_add2arena_rectangle( dague_trsmpl->arenas[DAGUE_ztrsmpl_PIVOT_ARENA],
                                 A->mb*sizeof(int),
                                 DAGUE_ARENA_ALIGNMENT_SSE,
                                 MPI_INT, A->mb, 1, -1 );

    /* L */
    dplasma_add2arena_rectangle( dague_trsmpl->arenas[DAGUE_ztrsmpl_SMALL_L_ARENA],
                                 L->mb*L->nb*sizeof(dague_complex64_t),
                                 DAGUE_ARENA_ALIGNMENT_SSE,
                                 MPI_DOUBLE_COMPLEX, L->mb, L->nb, -1);

    return (dague_handle_t*)dague_trsmpl;
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 *  dplasma_ztrsmpl_Destruct - Free the data structure associated to an object
 *  created with dplasma_ztrsmpl_New().
 *
 *******************************************************************************
 *
 * @param[in,out] o
 *          On entry, the object to destroy.
 *          On exit, the object cannot be used anymore.
 *
 *******************************************************************************
 *
 * @sa dplasma_ztrsmpl_New
 * @sa dplasma_ztrsmpl
 *
 ******************************************************************************/
void
dplasma_ztrsmpl_Destruct( dague_handle_t *o )
{
    dague_ztrsmpl_handle_t *dague_trsmpl = (dague_ztrsmpl_handle_t *)o;

    dplasma_datatype_undefine_type( &(dague_trsmpl->arenas[DAGUE_ztrsmpl_DEFAULT_ARENA]->opaque_dtt) );
    dplasma_datatype_undefine_type( &(dague_trsmpl->arenas[DAGUE_ztrsmpl_PIVOT_ARENA  ]->opaque_dtt) );
    dplasma_datatype_undefine_type( &(dague_trsmpl->arenas[DAGUE_ztrsmpl_SMALL_L_ARENA]->opaque_dtt) );

    DAGUE_INTERNAL_HANDLE_DESTRUCT(o);
}

/**
 *******************************************************************************
 *
 * @ingroup dplasma_complex64
 *
 * dplasma_ztrsmpl - Solves U*x = b, when U has been generated through LU
 * factorization with incremental pivoting strategy
 * See dplasma_zgetrf_incpiv().
 *
 *******************************************************************************
 *
 * @param[in,out] dague
 *          The dague context of the application that will run the operation.
 *
 * @param[in] A
 *          Descriptor of the distributed matrix A to be factorized.
 *          On entry, The factorized matrix through dplasma_zgetrf_incpiv_New()
 *          routine.  Elements on and above the diagonal are the elements of
 *          U. Elements below the diagonal are NOT the classic L, but the L
 *          factors obtaines by succesive pivoting.
 *
 * @param[in] L
 *          Descriptor of the matrix L distributed exactly as the A matrix.
 *           - If IPIV != NULL, L.mb defines the IB parameter of the tile LU
 *          algorithm. This matrix must be of size A.mt * L.mb - by - A.nt *
 *          L.nb, with L.nb == A.nb.
 *          On entry, contains auxiliary information required to solve the
 *          system and generated by dplasma_zgetrf_inciv_New().
 *           - If IPIV == NULL, pivoting information are stored within
 *          L. (L.mb-1) defines the IB parameter of the tile LU algorithm. This
 *          matrix must be of size A.mt * L.mb - by - A.nt * L.nb, with L.nb =
 *          A.nb, and L.mb = ib+1.
 *          The first A.mb elements contains the IPIV information, the leftover
 *          contains auxiliary information required to solve the system.
 *
 * @param[in] IPIV
 *          Descriptor of the IPIV matrix. Should be distributed exactly as the
 *          A matrix. This matrix must be of size A.m - by - A.nt with IPIV.mb =
 *          A.mb and IPIV.nb = 1.
 *          On entry, contains the pivot indices of the successive row
 *          interchanged performed during the factorization.
 *          If IPIV == NULL, rows interchange information is stored within L.
 *
 * @param[in,out] B
 *          On entry, the N-by-NRHS right hand side matrix B.
 *          On exit, if return value = 0, B is overwritten by the solution matrix X.
 *
 *******************************************************************************
 *
 * @return
 *          \retval -i if the ith parameters is incorrect.
 *          \retval 0 on success.
 *          \retval i if ith value is singular. Result is incoherent.
 *
 *******************************************************************************
 *
 * @sa dplasma_ztrsmpl
 * @sa dplasma_ztrsmpl_Destruct
 * @sa dplasma_ctrsmpl_New
 * @sa dplasma_dtrsmpl_New
 * @sa dplasma_strsmpl_New
 *
 ******************************************************************************/
int
dplasma_ztrsmpl( dague_context_t *dague,
                 const tiled_matrix_desc_t *A,
                 const tiled_matrix_desc_t *L,
                 const tiled_matrix_desc_t *IPIV,
                       tiled_matrix_desc_t *B )
{
    dague_handle_t *dague_ztrsmpl = NULL;

    if ( (A->mt != L->mt) || (A->nt != L->nt) ) {
        dplasma_error("dplasma_ztrsmpl", "L doesn't have the same number of tiles as A");
        return -3;
    }
    if ( (IPIV != NULL) && ((A->mt != IPIV->mt) || (A->nt != IPIV->nt)) ) {
        dplasma_error("dplasma_ztrsmpl", "IPIV doesn't have the same number of tiles as A");
        return -4;
    }

    dague_ztrsmpl = dplasma_ztrsmpl_New(A, L, IPIV, B);
    if ( dague_ztrsmpl != NULL )
    {
        dague_enqueue( dague, dague_ztrsmpl );
        dplasma_progress( dague );
        dplasma_ztrsmpl_Destruct( dague_ztrsmpl );
        return 0;
    }
    else
        return -101;
}
