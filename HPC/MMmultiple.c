/* Hi, everybody!
 * =====================================================================================
 *
 *       Filename:  MMmultiple.c
 *
 *    Description:  Do Matrix Multiplication C = A x B with A and B blocks generated by
 *		    mkmatrices.c.
 *
 *        Version:  1.0
 *        Created:  09/21/2016 22:53:31
 *       Revision:  none
 *       Compiler:  gcc
 *
 *         Author:  Xiukun Hu 
 *   Organization:  University of Wyoming, Department of Mathematics
 *
 * =====================================================================================
 */
#ifdef _OPENMP
#include <omp.h>
#else
#define omp_get_num_threads() 1;
#endif

#include <stdio.h>
#include <stdlib.h>
#include "matrices.h"

#define WIDTH 30

void ClearMatrix( double** matrix, int nrows, int ncols ) {
    int i, j;
    for ( i = 0 ; i < nrows ; i++ ) 
	for ( j = 0 ; j < ncols ; j++ ) 
	    matrix[i][j] = 0;
}

int main(){
    /* Local declarations */
    int         acols = 0;      /* Block columns in A */
    int         arows = 0;      /* Block rows    in A */

    int		bcols = 0;      /* Block columns in B */
    int		brows = 0;      /* Block rows	 in B */

    int		ccols = 0;      /* Block columns in C */
    int		crows = 0;      /* Block rows	 in C */

    int         blk_cols = 0;   /* Columns in a block */
    int         blk_rows = 0;   /* Rows    in a block */

    double **ablock[2];  /* Pointer to one block of A */
    double **bblock[2];  /* Pointer to one block of B */
    double **cblock[2];  /* Pointer to one block of C */
    double	cpblock;        /* Private pointer for saving results */

    int         mopt_a = 0;     /* How to allocate space in A blocks */
    int         mopt_b = 1;     /* How to allocate space in B blocks */
    int		mopt_c = 1;     /* How to allocate space in C blocks */

    char        c = ' ';        /* Input character */

    int		colleft;        /* Block columns residue by WIDTH */
    
    int         i = 0;          /* Loop index */
    int         j = 0;          /* Loop index */
    int		k = 0;          /* Loop index */
    int		I,J,K;          /* Loop index */
    int		iplus;          /* Loop index */
    int		jplus;          /* Loop index */
    int		kplus;          /* Loop index */
    int		tog  = 0;       /* Toggle 0 1 */
    int		ctog = 0;       /* Toggle for cblock */

    int		ar;             /* ablock row index */
    int		ac;             /* ablock col index */

    int		nThreads;
    
    double      t1;             /* Time keeper */
    double      t2;             /* Time keeper */
    double	tio1;           /* Private I/O time keeper */
    double	tio2 = 0;       /* Private I/O time keeper */
    double	tio  = 0;       /* I/O time keeper */
    double	tc1;            /* Compute time */
    double	tc   = 0;       /* Compute time */
    double      mrun();         /* Get timing information */


    /* Get matrix information from disk */
    matrix_info_read( &blk_rows, &blk_cols, 
	    &arows, &acols, 
	    &brows, &bcols,
	    &crows, &ccols );

    colleft = blk_cols % WIDTH;
    int nI = blk_rows * (blk_cols / WIDTH);
    int AC = blk_cols - colleft;
    t1 = mrun();


    /* Allocate 6 block matrices (two each for A, B and C) */
    ablock[0] = block_allocate( blk_rows, blk_cols, mopt_a );
    bblock[0] = block_allocate( blk_rows, blk_cols, mopt_b );
    cblock[0] = block_allocate( blk_rows, blk_cols, mopt_c );
    ablock[1] = block_allocate( blk_rows, blk_cols, mopt_a );
    bblock[1] = block_allocate( blk_rows, blk_cols, mopt_b );
    cblock[1] = block_allocate( blk_rows, blk_cols, mopt_c );

    ClearMatrix( cblock[0], blk_rows, blk_cols );
    ClearMatrix( cblock[1], blk_rows, blk_cols );

#pragma omp parallel default(none)  \
    shared(blk_cols, blk_rows,	    \
	    ablock, bblock, cblock, \
	    mopt_a, mopt_b, mopt_c, \
	    acols, crows, ccols,    \
	    colleft, nI, nThreads,  \
	    tio, AC, tc1, tc ) \
    firstprivate( tog, ctog, i, j, k, tio2 ) \
    private( I, J, K, iplus, jplus, kplus, cpblock, ar, ac, tio1 )
    {
#pragma omp single
	{
	    tio1 = mrun();
	nThreads = omp_get_num_threads();
	    block_readdisk( blk_rows, blk_cols, "A", 0, 0, ablock[0], mopt_a, 0 );
	    block_readdisk( blk_rows, blk_cols, "B", 0, 0, bblock[0], mopt_a, 0 );
	    tio2 += mrun() - tio1;
	} /* single thread reading A00 B00 */

	while ( i < crows ){
	    kplus = (k+1) % acols;
	    jplus = (kplus==0)? ((j+1)%ccols) : j;
	    iplus = (jplus==0 && kplus==0)? i+1 : i;

#pragma omp single nowait
	    {
		if ( iplus < crows ) {
		    tio1 = mrun();
		    block_readdisk( blk_rows, blk_cols, "A", iplus, kplus, ablock[1-tog], mopt_a, 0 );
		    block_readdisk( blk_rows, blk_cols, "B", kplus, jplus, bblock[1-tog], mopt_b, 0 );
		    tio2 += mrun() - tio1;
		}
	    }

#pragma omp master
	    tc1 = mrun();

#pragma omp for nowait schedule(runtime)
	    for ( I = 0 ; I < nI; I++ ) {
		ar = I % blk_rows, ac = (I / blk_rows) * WIDTH;
		for ( K = 0 ; K < blk_cols ; K++ ) {
		    cpblock = 0;
		    for ( J = 0 ; J < WIDTH ; J++ ) 
		    cpblock += ablock[tog][ar][ac+J] * bblock[tog][ac+J][K];

#pragma omp atomic update
		    cblock[ctog][ar][K] += cpblock;
		}	
	    }

	    if ( colleft ) {
#pragma omp for nowait schedule(runtime)
		for ( ar = 0 ; ar < blk_rows ; ar++ ) {
		    ac = AC;
		    for ( K = 0 ; K < blk_cols ; K++ ) {
			cpblock = 0;
			for ( J = 0 ; J < colleft ; J++ ) 
			    cpblock += ablock[tog][ar][ac+J] * bblock[tog][ac+J][K];

#pragma omp atomic update
			cblock[ctog][ar][K] += cpblock;
		    }
		}
	    }

#pragma omp barrier

#pragma omp master
	    tc += mrun() - tc1;
	    
	    if ( kplus==0 ) {
#pragma omp single nowait
		{
		    tio1 = mrun();
		    block_write2disk( blk_rows, blk_cols, "C", i, j, cblock[ctog][0] );
		    ClearMatrix( cblock[ctog], blk_rows, blk_cols );
		    tio2 += mrun() - tio1;
		} /* Write cblock: OMP single nowait */
		ctog = 1-ctog;
	    }

	    tog = 1 - tog;
	    i = iplus;
	    j = jplus;
	    k = kplus;
	} /* While loop for blocks */
#pragma omp atomic update
	tio += tio2;
    }

    t2 = mrun() - t1;
    /* Time */
    printf( "Matrices multiplication done in %le seconds\n", t2);
    printf( "I/O time: %le seconds\n", tio );
    printf( "Compute time: %le\n", tc );
    printf( "%d threads are used.\n", nThreads );

    /* End */
    return 0;
} 
