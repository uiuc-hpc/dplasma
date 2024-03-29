/*
 * Copyright (c) 2011-2020 The University of Tennessee and The University
 *                         of Tennessee Research Foundation.  All rights
 *                         reserved.
 *
 * @precisions normal z -> s d c
 *
 */

#include "common.h"
#include "parsec/data_dist/matrix/two_dim_rectangle_cyclic.h"

static int check_orthogonality(parsec_context_t *parsec, int loud,
                               parsec_tiled_matrix_dc_t *Q);
static int check_factorization(parsec_context_t *parsec, int loud,
                               parsec_tiled_matrix_dc_t *Aorig,
                               parsec_tiled_matrix_dc_t *A,
                               parsec_tiled_matrix_dc_t *Q);
static int check_solution( parsec_context_t *parsec, int loud,
                           parsec_tiled_matrix_dc_t *dcA,
                           parsec_tiled_matrix_dc_t *dcB,
                           parsec_tiled_matrix_dc_t *dcX );

int main(int argc, char ** argv)
{
    parsec_context_t* parsec;
    int iparam[IPARAM_SIZEOF];
    int ret = 0;
    PLASMA_enum uplo = PlasmaUpperLower;

    /* Set defaults for non argv iparams */
    iparam_default_facto(iparam);
    iparam_default_ibnbmb(iparam, 32, 200, 200);
    iparam[IPARAM_KP] = 4;
    iparam[IPARAM_KQ] = 1;
    iparam[IPARAM_LDA] = -'m';
    iparam[IPARAM_LDB] = -'m';

    /* Initialize PaRSEC */
    parsec = setup_parsec(argc, argv, iparam);
    PASTE_CODE_IPARAM_LOCALS(iparam);
    PASTE_CODE_FLOPS(FLOPS_ZGEQRF, ((DagDouble_t)M, (DagDouble_t)N));

    LDA = max(M, LDA);
    LDB = max(M, LDB);

    /* initializing matrix structure */
    PASTE_CODE_ALLOCATE_MATRIX(dcA, 1,
        two_dim_block_cyclic, (&dcA, matrix_ComplexDouble, matrix_Tile,
                               nodes, rank, MB, NB, LDA, N, 0, 0,
                               M, N, KP, KQ, P));
    PASTE_CODE_ALLOCATE_MATRIX(dcT, 1,
        two_dim_block_cyclic, (&dcT, matrix_ComplexDouble, matrix_Tile,
                               nodes, rank, IB, NB, MT*IB, N, 0, 0,
                               MT*IB, N, KP, KQ, P));
    PASTE_CODE_ALLOCATE_MATRIX(dcA0, check,
        two_dim_block_cyclic, (&dcA0, matrix_ComplexDouble, matrix_Tile,
                               nodes, rank, MB, NB, LDA, N, 0, 0,
                               M, N, KP, KQ, P));
    PASTE_CODE_ALLOCATE_MATRIX(dcQ, check,
        two_dim_block_cyclic, (&dcQ, matrix_ComplexDouble, matrix_Tile,
                               nodes, rank, MB, NB, LDA, N, 0, 0,
                               M, N, KP, KQ, P));

    /* Check the solution */
    PASTE_CODE_ALLOCATE_MATRIX(dcB, check,
        two_dim_block_cyclic, (&dcB, matrix_ComplexDouble, matrix_Tile,
                               nodes, rank, MB, NB, LDB, NRHS, 0, 0,
                               M, NRHS, KP, KQ, P));

    PASTE_CODE_ALLOCATE_MATRIX(dcX, check,
        two_dim_block_cyclic, (&dcX, matrix_ComplexDouble, matrix_Tile,
                               nodes, rank, MB, NB, LDB, NRHS, 0, 0,
                               M, NRHS, KP, KQ, P));

    /* matrix generation */
    if(loud > 3) printf("+++ Generate matrices ... ");
    dplasma_zpltmg( parsec, matrix_init, (parsec_tiled_matrix_dc_t *)&dcA, random_seed );
    if( check )
        dplasma_zlacpy( parsec, dplasmaUpperLower,
                        (parsec_tiled_matrix_dc_t *)&dcA, (parsec_tiled_matrix_dc_t *)&dcA0 );
    dplasma_zlaset( parsec, dplasmaUpperLower, 0., 0., (parsec_tiled_matrix_dc_t *)&dcT);
    if(loud > 3) printf("Done\n");


   PASTE_CODE_ALLOCATE_MATRIX(dcA2, 1,
        two_dim_block_cyclic, (&dcA2, matrix_ComplexDouble, matrix_Tile,
                               nodes, rank, MB, NB, LDA, N, 0, 0,
                               M, N, KP, KQ, P));
    PASTE_CODE_ALLOCATE_MATRIX(dcT2, 1,
        two_dim_block_cyclic, (&dcT2, matrix_ComplexDouble, matrix_Tile,
                               nodes, rank, IB, NB, MT*IB, N, 0, 0,
                               MT*IB, N, KP, KQ, P));


    int t;
    for(t = 0; t < nruns; t++) {
        dplasma_zlacpy( parsec, uplo,
                        (parsec_tiled_matrix_dc_t *)&dcA, (parsec_tiled_matrix_dc_t *)&dcA2 );
        dplasma_zlacpy( parsec, uplo,
                        (parsec_tiled_matrix_dc_t *)&dcT, (parsec_tiled_matrix_dc_t *)&dcT2 );

        parsec_devices_release_memory();

        if(iparam[IPARAM_HNB] != iparam[IPARAM_NB])
        {
            SYNC_TIME_START();
            parsec_taskpool_t* PARSEC_zgeqrf = dplasma_zgeqrf_New( (parsec_tiled_matrix_dc_t*)&dcA2,
                                                                   (parsec_tiled_matrix_dc_t*)&dcT2 );
            /* Set the recursive size */
            dplasma_zgeqrf_setrecursive( PARSEC_zgeqrf, iparam[IPARAM_HNB] );
            parsec_context_add_taskpool(parsec, PARSEC_zgeqrf);
            if( loud > 2 ) SYNC_TIME_PRINT(rank, ( "zgeqrf\tDAG created\n"));

            PASTE_CODE_PROGRESS_KERNEL(parsec, zgeqrf);
            dplasma_zgeqrf_Destruct( PARSEC_zgeqrf );

            parsec_taskpool_sync_ids(); /* recursive DAGs are not synchronous on ids */
        }
        else
        {
            PASTE_CODE_ENQUEUE_PROGRESS_DESTRUCT_KERNEL(parsec, zgeqrf,
                                      ((parsec_tiled_matrix_dc_t*)&dcA2,
                                      (parsec_tiled_matrix_dc_t*)&dcT2),
                                      dplasma_zgeqrf_Destruct( PARSEC_zgeqrf ));
        }
        parsec_devices_reset_load(parsec);

    }
    dplasma_zlacpy( parsec, uplo,
                       (parsec_tiled_matrix_dc_t *)&dcA2, (parsec_tiled_matrix_dc_t *)&dcA );
    dplasma_zlacpy( parsec, uplo,
                       (parsec_tiled_matrix_dc_t *)&dcT2, (parsec_tiled_matrix_dc_t *)&dcT );
    
#if defined(PARSEC_SIM)
    {
        int largest_simulation_date = parsec_getsimulationdate( parsec );
        if ( rank == 0 ) {
            printf("zgeqrf simulation NP= %d NC= %d P= %d KP= %d MT= %d NT= %d : %d \n",
               iparam[IPARAM_NNODES],
                   iparam[IPARAM_NCORES],
                   iparam[IPARAM_P],
                   iparam[IPARAM_KP],
                   MT, NT,
                   largest_simulation_date);
        }
    }
#endif
    if( check ) {
        if (M >= N) {
            if(loud > 2) printf("+++ Generate the Q ...");
            dplasma_zungqr( parsec,
                            (parsec_tiled_matrix_dc_t *)&dcA,
                            (parsec_tiled_matrix_dc_t *)&dcT,
                            (parsec_tiled_matrix_dc_t *)&dcQ);
            if(loud > 2) printf("Done\n");

            if(loud > 2) printf("+++ Solve the system ...");
            dplasma_zplrnt( parsec, 0, (parsec_tiled_matrix_dc_t *)&dcX, 2354);
            dplasma_zlacpy( parsec, dplasmaUpperLower,
                            (parsec_tiled_matrix_dc_t *)&dcX,
                            (parsec_tiled_matrix_dc_t *)&dcB );
            dplasma_zgeqrs( parsec,
                            (parsec_tiled_matrix_dc_t *)&dcA,
                            (parsec_tiled_matrix_dc_t *)&dcT,
                            (parsec_tiled_matrix_dc_t *)&dcX );
            if(loud > 2) printf("Done\n");

            /* Check the orthogonality, factorization and the solution */
            ret |= check_orthogonality( parsec, (rank == 0) ? loud : 0,
                                        (parsec_tiled_matrix_dc_t *)&dcQ);
            ret |= check_factorization( parsec, (rank == 0) ? loud : 0,
                                        (parsec_tiled_matrix_dc_t *)&dcA0,
                                        (parsec_tiled_matrix_dc_t *)&dcA,
                                        (parsec_tiled_matrix_dc_t *)&dcQ );
            ret |= check_solution( parsec, (rank == 0) ? loud : 0,
                                   (parsec_tiled_matrix_dc_t *)&dcA0,
                                   (parsec_tiled_matrix_dc_t *)&dcB,
                                   (parsec_tiled_matrix_dc_t *)&dcX );

        } else {
            printf("Check cannot be performed when N > M\n");
        }

        parsec_data_free(dcA0.mat);
        parsec_data_free(dcQ.mat);
        parsec_data_free(dcB.mat);
        parsec_data_free(dcX.mat);
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcA0);
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcQ);
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcB);
        parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcX);
    }

    parsec_data_free(dcA.mat);
    parsec_data_free(dcT.mat);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcA);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcT);

    parsec_data_free(dcA2.mat);
    parsec_data_free(dcT2.mat);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcA2);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&dcT2);

    cleanup_parsec(parsec, iparam);

    return ret;
}

/*-------------------------------------------------------------------
 * Check the orthogonality of Q
 */
static int check_orthogonality(parsec_context_t *parsec, int loud, parsec_tiled_matrix_dc_t *Q)
{
    two_dim_block_cyclic_t *twodQ = (two_dim_block_cyclic_t *)Q;
    double normQ = 999999.0;
    double result;
    double eps = LAPACKE_dlamch_work('e');
    int info_ortho;
    int M = Q->m;
    int N = Q->n;
    int minMN = min(M, N);

    PASTE_CODE_ALLOCATE_MATRIX(Id, 1,
        two_dim_block_cyclic, (&Id, matrix_ComplexDouble, matrix_Tile,
                               Q->super.nodes, twodQ->grid.rank,
                               Q->mb, Q->nb, minMN, minMN, 0, 0,
                               minMN, minMN, twodQ->grid.krows, twodQ->grid.kcols, twodQ->grid.rows));

    dplasma_zlaset( parsec, dplasmaUpperLower, 0., 1., (parsec_tiled_matrix_dc_t *)&Id);

    /* Perform Id - Q'Q */
    if ( M >= N ) {
        dplasma_zherk( parsec, dplasmaUpper, dplasmaConjTrans,
                       1.0, Q, -1.0, (parsec_tiled_matrix_dc_t*)&Id );
    } else {
        dplasma_zherk( parsec, dplasmaUpper, dplasmaNoTrans,
                       1.0, Q, -1.0, (parsec_tiled_matrix_dc_t*)&Id );
    }

    normQ = dplasma_zlanhe(parsec, dplasmaInfNorm, dplasmaUpper, (parsec_tiled_matrix_dc_t*)&Id);

    result = normQ / (minMN * eps);
    if ( loud ) {
        printf("============\n");
        printf("Checking the orthogonality of Q \n");
        printf("||Id-Q'*Q||_oo / (N*eps) = %e \n", result);
    }

    if ( isnan(result) || isinf(result) || (result > 60.0) ) {
        if ( loud ) printf("-- Orthogonality is suspicious ! \n");
        info_ortho=1;
    }
    else {
        if ( loud ) printf("-- Orthogonality is CORRECT ! \n");
        info_ortho=0;
    }

    parsec_data_free(Id.mat);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&Id);
    return info_ortho;
}

/*-------------------------------------------------------------------
 * Check the orthogonality of Q
 */
static int
check_factorization(parsec_context_t *parsec, int loud,
                    parsec_tiled_matrix_dc_t *Aorig,
                    parsec_tiled_matrix_dc_t *A,
                    parsec_tiled_matrix_dc_t *Q)
{
    parsec_tiled_matrix_dc_t *subA;
    two_dim_block_cyclic_t *twodA = (two_dim_block_cyclic_t *)A;
    double Anorm, Rnorm;
    double result;
    double eps = LAPACKE_dlamch_work('e');
    int info_factorization;
    int M = A->m;
    int N = A->n;
    int minMN = min(M, N);

    PASTE_CODE_ALLOCATE_MATRIX(Residual, 1,
        two_dim_block_cyclic, (&Residual, matrix_ComplexDouble, matrix_Tile,
                               A->super.nodes, twodA->grid.rank,
                               A->mb, A->nb, M, N, 0, 0,
                               M, N, twodA->grid.krows, twodA->grid.kcols, twodA->grid.rows));

    PASTE_CODE_ALLOCATE_MATRIX(R, 1,
        two_dim_block_cyclic, (&R, matrix_ComplexDouble, matrix_Tile,
                               A->super.nodes, twodA->grid.rank,
                               A->mb, A->nb, N, N, 0, 0,
                               N, N, twodA->grid.krows, twodA->grid.kcols, twodA->grid.rows));

    /* Copy the original A in Residual */
    dplasma_zlacpy( parsec, dplasmaUpperLower, Aorig, (parsec_tiled_matrix_dc_t *)&Residual );

    /* Extract the R */
    dplasma_zlaset( parsec, dplasmaUpperLower, 0., 0., (parsec_tiled_matrix_dc_t *)&R);

    subA = tiled_matrix_submatrix( A, 0, 0, N, N );
    dplasma_zlacpy( parsec, dplasmaUpper, subA, (parsec_tiled_matrix_dc_t *)&R );
    free(subA);

    /* Perform Residual = Aorig - Q*R */
    dplasma_zgemm( parsec, dplasmaNoTrans, dplasmaNoTrans,
                   -1.0, Q, (parsec_tiled_matrix_dc_t *)&R,
                    1.0, (parsec_tiled_matrix_dc_t *)&Residual);

    /* Free R */
    parsec_data_free(R.mat);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&R);

    Rnorm = dplasma_zlange(parsec, dplasmaInfNorm, (parsec_tiled_matrix_dc_t*)&Residual);
    Anorm = dplasma_zlange(parsec, dplasmaInfNorm, Aorig);

    result = Rnorm / ( Anorm * minMN * eps);

    if ( loud ) {
        printf("============\n");
        printf("Checking the QR Factorization \n");
        printf("-- ||A-QR||_oo/(||A||_oo.N.eps) = %e \n", result );
    }

    if ( isnan(result) || isinf(result) || (result > 60.0) ) {
        if ( loud ) printf("-- Factorization is suspicious ! \n");
        info_factorization = 1;
    }
    else {
        if ( loud ) printf("-- Factorization is CORRECT ! \n");
        info_factorization = 0;
    }

    parsec_data_free(Residual.mat);
    parsec_tiled_matrix_dc_destroy( (parsec_tiled_matrix_dc_t*)&Residual);
    return info_factorization;
}

static int check_solution( parsec_context_t *parsec, int loud,
                           parsec_tiled_matrix_dc_t *dcA,
                           parsec_tiled_matrix_dc_t *dcB,
                           parsec_tiled_matrix_dc_t *dcX )
{
    parsec_tiled_matrix_dc_t *subX;
    int info_solution;
    double Rnorm = 0.0;
    double Anorm = 0.0;
    double Bnorm = 0.0;
    double Xnorm, result;
    double eps = LAPACKE_dlamch_work('e');

    subX = tiled_matrix_submatrix( dcX, 0, 0, dcA->n, dcX->n );

    Anorm = dplasma_zlange(parsec, dplasmaInfNorm, dcA);
    Bnorm = dplasma_zlange(parsec, dplasmaInfNorm, dcB);
    Xnorm = dplasma_zlange(parsec, dplasmaInfNorm, subX);

    /* Compute A*x-b */
    dplasma_zgemm( parsec, dplasmaNoTrans, dplasmaNoTrans, 1.0, dcA, subX, -1.0, dcB);

    /* Compute A' * ( A*x - b ) */
    dplasma_zgemm( parsec, dplasmaConjTrans, dplasmaNoTrans,
                   1.0, dcA, dcB, 0., subX );

    Rnorm = dplasma_zlange( parsec, dplasmaInfNorm, subX );
    free(subX);

    result = Rnorm / ( ( Anorm * Xnorm + Bnorm ) * dcA->n * eps ) ;

    if ( loud > 2 ) {
        printf("============\n");
        printf("Checking the Residual of the solution \n");
        if ( loud > 3 )
            printf( "-- ||A||_oo = %e, ||X||_oo = %e, ||B||_oo= %e, ||A X - B||_oo = %e\n",
                    Anorm, Xnorm, Bnorm, Rnorm );

        printf("-- ||Ax-B||_oo/((||A||_oo||x||_oo+||B||_oo).N.eps) = %e \n", result);
    }

    if (  isnan(Xnorm) || isinf(Xnorm) || isnan(result) || isinf(result) || (result > 60.0) ) {
        if( loud ) printf("-- Solution is suspicious ! \n");
        info_solution = 1;
    }
    else{
        if( loud ) printf("-- Solution is CORRECT ! \n");
        info_solution = 0;
    }

    return info_solution;
}
