/*
 * Copyright (c) 2009-2020 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */

#include "common.h"
#include "flops.h"
#include "parsec/data_dist/matrix/sym_two_dim_rectangle_cyclic.h"
#include "parsec/data_dist/matrix/two_dim_rectangle_cyclic.h"

int main(int argc, char ** argv)
{
    parsec_context_t* parsec;
    int iparam[IPARAM_SIZEOF];
    dplasma_enum_t uplo = dplasmaUpper;
    int info = 0;
    int ret = 0;

    /* Set defaults for non argv iparams */
    iparam_default_facto(iparam);
    iparam_default_ibnbmb(iparam, 0, 180, 180);

    /* Initialize PaRSEC */
    parsec = setup_parsec(argc, argv, iparam);
    PASTE_CODE_IPARAM_LOCALS(iparam);
    PASTE_CODE_FLOPS(FLOPS_ZPOTRF, ((DagDouble_t)N));

    /* initializing matrix structure */
    LDA = dplasma_imax( LDA, N );
    LDB = dplasma_imax( LDB, N );
    KP = 1;
    KQ = 1;

    PASTE_CODE_ALLOCATE_MATRIX(dcA, 1,
        sym_two_dim_block_cyclic, (&dcA, matrix_ComplexDouble,
                                   nodes, rank, MB, NB, LDA, N, 0, 0,
                                   N, N, P, uplo));
    /* matrix generation */
    if(loud > 3) printf("+++ Generate matrices ... ");
    dplasma_zplghe( parsec, (double)(N), uplo,
                    (parsec_tiled_matrix_dc_t *)&dcA, random_seed);
    if(loud > 3) printf("Done\n");


    PASTE_CODE_ALLOCATE_MATRIX(dcA2, 1,
       sym_two_dim_block_cyclic, (&dcA2, matrix_ComplexDouble,
                                  nodes, rank, MB, NB, LDA, N, 0, 0,
                                  N, N, P, uplo));
    int t;
    for(t = 0; t < nruns; t++) {
        dplasma_zlacpy( parsec, uplo,
                       (parsec_tiled_matrix_dc_t *)&dcA, (parsec_tiled_matrix_dc_t *)&dcA2 );
        parsec_devices_release_memory();

        if((iparam[IPARAM_HNB] != iparam[IPARAM_NB]) || (iparam[IPARAM_HMB] != iparam[IPARAM_MB]))
        {

            SYNC_TIME_START();
            parsec_taskpool_t* PARSEC_zpotrf = dplasma_zpotrf_New( uplo, (parsec_tiled_matrix_dc_t*)&dcA2, &info );
            /* Set the recursive size */
            dplasma_zpotrf_setrecursive( PARSEC_zpotrf, iparam[IPARAM_HMB] );
            parsec_context_add_taskpool(parsec, PARSEC_zpotrf);
            if( loud > 2 ) SYNC_TIME_PRINT(rank, ( "zpotrf\tDAG created\n"));

            PASTE_CODE_PROGRESS_KERNEL(parsec, zpotrf);
            dplasma_zpotrf_Destruct( PARSEC_zpotrf );

            parsec_taskpool_sync_ids(); /* recursive DAGs are not synchronous on ids */

        }
        else
        {
            PASTE_CODE_ENQUEUE_PROGRESS_DESTRUCT_KERNEL(parsec, zpotrf, 
                                      ( uplo, (parsec_tiled_matrix_dc_t*)&dcA2, &info),
                                      dplasma_zpotrf_Destruct( PARSEC_zpotrf ));
        }
        parsec_devices_reset_load(parsec);

    }
    dplasma_zlacpy( parsec, uplo,
                       (parsec_tiled_matrix_dc_t *)&dcA2, (parsec_tiled_matrix_dc_t *)&dcA );

    if( 0 == rank && info != 0 ) {
        printf("-- Factorization is suspicious (info = %d) ! \n", info);
        ret |= 1;
    }
    if( !info && check ) {
        /* Check the factorization */
        PASTE_CODE_ALLOCATE_MATRIX(dcA0, check,
            sym_two_dim_block_cyclic, (&dcA0, matrix_ComplexDouble,
                                       nodes, rank, MB, NB, LDA, N, 0, 0,
                                       N, N, P, uplo));
        dplasma_zplghe( parsec, (double)(N), uplo,
                        (parsec_tiled_matrix_dc_t *)&dcA0, random_seed);

        ret |= check_zpotrf( parsec, (rank == 0) ? loud : 0, uplo,
                             (parsec_tiled_matrix_dc_t *)&dcA,
                             (parsec_tiled_matrix_dc_t *)&dcA0);

        /* Check the solution */
        PASTE_CODE_ALLOCATE_MATRIX(dcB, check,
            two_dim_block_cyclic, (&dcB, matrix_ComplexDouble, matrix_Tile,
                                   nodes, rank, MB, NB, LDB, NRHS, 0, 0,
                                   N, NRHS, KP, KQ, P));
        dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcB, random_seed+1);

        PASTE_CODE_ALLOCATE_MATRIX(dcX, check,
            two_dim_block_cyclic, (&dcX, matrix_ComplexDouble, matrix_Tile,
                                   nodes, rank, MB, NB, LDB, NRHS, 0, 0,
                                   N, NRHS, KP, KQ, P));
        dplasma_zlacpy( parsec, dplasmaUpperLower,
                        (parsec_tiled_matrix_dc_t *)&dcB, (parsec_tiled_matrix_dc_t *)&dcX );

        dplasma_zpotrs(parsec, uplo,
                       (parsec_tiled_matrix_dc_t *)&dcA,
                       (parsec_tiled_matrix_dc_t *)&dcX );

        ret |= check_zaxmb( parsec, (rank == 0) ? loud : 0, uplo,
                            (parsec_tiled_matrix_dc_t *)&dcA0,
                            (parsec_tiled_matrix_dc_t *)&dcB,
                            (parsec_tiled_matrix_dc_t *)&dcX);

        /* Cleanup */
        parsec_data_free(dcA0.mat); dcA0.mat = NULL;
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcA0 );
        parsec_data_free(dcB.mat); dcB.mat = NULL;
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcB );
        parsec_data_free(dcX.mat); dcX.mat = NULL;
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcX );
    }

    parsec_data_free(dcA.mat); dcA.mat = NULL;
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcA);
    parsec_data_free(dcA2.mat); dcA2.mat = NULL;
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcA2);

    cleanup_parsec(parsec, iparam);
    return ret;
}
