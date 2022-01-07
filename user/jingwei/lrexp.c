/* 2-D FFT-based zero-offset exploding reflector modeling/migration linear operator */
/*
  Copyright (C) 2010 University of Texas at Austin
  
  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; either version 2 of the License, or
  (at your option) any later version.
  
  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.
  
  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/
#include <rsf.h>

#include "cfft2nsps.h"
/*******************************************************/

int lrexp(sf_complex *mod, sf_complex *dat, bool adj, sf_complex *lt, sf_complex *rt, int nt, int nx, int nz, int nkzx, int m2, int gpz)
/*< zero-offset exploding reflector modeling/migration >*/
{
    int it, iz, ix, im, ik, i, j;
    int nz2, nx2, nk, nzx, nzx2;
    int pad1 = 1;
    sf_complex **wave, *curr, *currm = NULL, *cwave, *cwavem = NULL, c;
 
    nk = cfft2_init(pad1,nz,nx,&nz2,&nx2);
    if (nk!=nkzx) sf_error("nk discrepancy!");

    nzx = nz*nx;
    nzx2 = nz2*nx2;

    curr = sf_complexalloc(nzx2);
    cwave  = sf_complexalloc(nk);
    wave = sf_complexalloc2(nzx2,m2);
    if (adj) {
	currm  = sf_complexalloc(nzx2);
	icfft2_allocate(cwave);
    } else {
	cwavem = sf_complexalloc(nk);
	icfft2_allocate(cwavem);
    }

    for (iz=0; iz < nzx2; iz++) {
	curr[iz] = sf_cmplx(0.,0.);
    }

    if (adj) { /* migration <- read data */

	/* time stepping */
	for (it=nt-1; it > -1; it--) {
	
	    /* matrix multiplication */
	    for (im = 0; im < m2; im++) {
		for (ix = 0; ix < nx; ix++) {
		    for (iz=0; iz < nz; iz++) {
			i = iz+ix*nz;  /* original grid */
			j = iz+ix*nz2; /* padded grid */
#ifdef SF_HAS_COMPLEX_H
			currm[j] = conjf(lt[im*nzx+i])*curr[j];
#else
			currm[j] = sf_cmul(conjf(lt[im*nzx+i]), curr[j]);
#endif
		    }
		}
		cfft2(currm,wave[im]);
	    }
	    for (ik = 0; ik < nk; ik++) {
		c = sf_cmplx(0.,0.);
		for (im = 0; im < m2; im++) {
#ifdef SF_HAS_COMPLEX_H
		    c += wave[im][ik]*conjf(rt[ik*m2+im]);
#else
		    c += sf_cmul(wave[im][ik],conjf(rt[ik*m2+im])); //complex multiplies complex
#endif
		}
		cwave[ik] = c;
	    }

	    icfft2(curr,cwave);

	    /* data injection */
	    for (ix=0; ix < nx; ix++) {
		curr[gpz+ix*nz2] += dat[ix*nt+it];
	    }
	} /*time iteration*/
	/*generate image*/
	for (ix=0; ix < nx; ix++) {
	    for (iz=0; iz < nz; iz++) {
		mod[ix*nz+iz] = curr[iz+ix*nz2];
	    }
	}
    } else { /* modeling -> write data */
	/*exploding reflector*/
	for (ix=0; ix < nx; ix++) {
	    for (iz=0; iz < nz; iz++) {
		curr[iz+ix*nz2] = mod[ix*nz+iz];
	    }
	}

	/* time stepping */
	for (it=0; it < nt; it++) {

	    /* record data */
	    for (ix=0; ix < nx; ix++) {
		dat[ix*nt+it] = curr[gpz+ix*nz2];
	    }
	    /* matrix multiplication */
	    cfft2(curr,cwave);
	    
	    for (im = 0; im < m2; im++) {
		for (ik = 0; ik < nk; ik++) {
#ifdef SF_HAS_COMPLEX_H
		    cwavem[ik] = cwave[ik]*rt[ik*m2+im];
#else
		    cwavem[ik] = sf_cmul(cwave[ik],rt[ik*m2+im]);
#endif
		}
		icfft2(wave[im],cwavem);
	    }
	    for (ix = 0; ix < nx; ix++) {
		for (iz=0; iz < nz; iz++) {
		    i = iz+ix*nz;  /* original grid */
		    j = iz+ix*nz2; /* padded grid */
		    
		    c = sf_cmplx(0.,0.); /* initialize */
		    
		    for (im = 0; im < m2; im++) {
#ifdef SF_HAS_COMPLEX_H
			c += lt[im*nzx+i]*wave[im][j];
#else
			c += sf_cmul(lt[im*nzx+i], wave[im][j]);
#endif	    
		    }
		    curr[j] = c;
		}
	    }
	}
    }

    cfft2_finalize();
    return 0;
}

