#include <math.h>

#include "cconjgrad.h"
#include "alloc.h"
#include "c99.h"
#include "error.h"

static int np, nx, nr, nd;
static float complex *r, *d, *sp, *sx, *sr, *gp, *gx, *gr;
static float eps, tol;
static bool verb, hasp0;

static double norm (int n, const float complex* x) {
    double prod;
    double complex xi;
    int i;

    prod = 0.;
    for (i = 0; i < n; i++) {
	xi = (double complex) x[i];
	prod += xi*conj(xi);
    }
    return prod;
}

void sf_cconjgrad_init(int np1, int nx1, int nd1, int nr1, float eps1,
		       float tol1, bool verb1, bool hasp01) 
{
    np = np1; 
    nx = nx1;
    nr = nr1;
    nd = nd1;
    eps = eps1*eps1;
    tol = tol1;
    verb = verb1;
    hasp0 = hasp01;

    r = sf_complexalloc(nr);  
    d = sf_complexalloc(nd); 
    sp = sf_complexalloc(np);
    gp = sf_complexalloc(np);
    sx = sf_complexalloc(nx);
    gx = sf_complexalloc(nx);
    sr = sf_complexalloc(nr);
    gr = sf_complexalloc(nr);
}

void sf_cconjgrad_close(void) 
{
    free (r);
    free (d);
    free (sp);
    free (gp);
    free (sx);
    free (gx);
    free (sr);
    free (gr);
}

void sf_cconjgrad(sf_coperator prec, sf_coperator oper, sf_coperator shape, 
		  float complex* p, float complex* x, float complex* dat, 
		  int niter) 
{
    double gn, gnp, alpha, beta, g0, dg, r0, b0;
    int i, iter;
    
    if (NULL != prec) {
	for (i=0; i < nd; i++) {
	    d[i] = - dat[i];
	}
	prec(false,false,nd,nr,d,r);
    } else {
	for (i=0; i < nr; i++) {
	    r[i] = - dat[i];
	}
    }
    
    if (hasp0) { /* initial p */
	shape(false,false,np,nx,p,x);
	if (NULL != prec) {
	    oper(false,false,nx,nd,x,d);
	    prec(false,true,nd,nr,d,r);
	} else {
	    oper(false,true,nx,nr,x,r);
	}
    } else {
	for (i=0; i < np; i++) {
	    p[i] = 0.;
	}
	for (i=0; i < nx; i++) {
	    x[i] = 0.;
	}
    } 
    
    dg = g0 = b0 = gnp = 0.;
    r0 = verb? norm(nr,r): 0.;

    for (iter=0; iter < niter; iter++) {
	for (i=0; i < np; i++) {
	    gp[i] = eps*p[i];
	}
	for (i=0; i < nx; i++) {
	    gx[i] = -eps*x[i];
	}

	if (NULL != prec) {
	    prec(true,false,nd,nr,d,r);
	    oper(true,true,nx,nd,gx,d);
	} else {
	    oper(true,true,nx,nr,gx,r);
	}

	shape(true,true,np,nx,gp,gx);
	shape(false,false,np,nx,gp,gx);

	if (NULL != prec) {
	    oper(false,false,nx,nd,gx,d);
	    prec(false,false,nd,nr,d,gr);
	} else {
	    oper(false,false,nx,nr,gx,gr);
	}

	gn = norm(np,gp);

	if (iter==0) {
	    g0 = gn;
	    b0 = fabs(gn + eps*(norm(nr,gr)-norm(nx,gx)));

	    for (i=0; i < np; i++) {
		sp[i] = gp[i];
	    }
	    for (i=0; i < nx; i++) {
		sx[i] = gx[i];
	    }
	    for (i=0; i < nr; i++) {
		sr[i] = gr[i];
	    }
	} else {
	    alpha = gn / gnp;
	    dg = gn / g0;

	    if (alpha < tol || dg < tol) {
		if (verb) 
		    sf_warning(
			"convergence in %d iterations, alpha=%g, gd=%g",
			iter,alpha,dg);
		break;
	    }

	    for (i=0; i < np; i++) {
		sp[i] = gp[i] + alpha * sp[i];
	    }
	    for (i=0; i < nx; i++) {
		sx[i] = gx[i] + alpha * sx[i];
	    }
	    for (i=0; i < nr; i++) {
		sr[i] = gr[i] + alpha * sr[i];
	    }
	}

	beta = norm(nr,sr) + eps*(norm(np,sp) - norm(nx,sx));

	/*
	if (beta/b0 < tol) {
	    if (verb) 
		sf_warning("convergence in %d iterations, beta=%g",iter,beta);
	    break;
	}
	*/
	
	if (verb) sf_warning("iteration %d res: %f grad: %f",
			     iter,norm(nr,r)/r0,dg);

	alpha = - gn / beta;

	for (i=0; i < np; i++) {
	    p[i] += alpha * sp[i];
	}

	for (i=0; i < nx; i++) {
	    x[i] += alpha * sx[i];
	}

	for (i=0; i < nr; i++) {
	    r[i] += alpha * sr[i];
	}

	gnp = gn;
    }
}

/* 	$Id: cconjgrad.c,v 1.1 2004/05/13 22:26:56 fomels Exp $	 */
