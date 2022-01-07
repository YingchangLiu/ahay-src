/* All-pass plane-wave destruction filter coefficients */
/*
  Copyright (C) 2004 University of Texas at Austin
  
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

#include "abcpass2.h"

static int nx, nz, nh, nbt, nbb, nbl, nbr;
static float ct, cb, cl, cr;
static float *wt, *wb, *wl, *wr;

void bd2_init(int n1,  int n2    /*model size:x*/,
             int nb1, int nb2  /*top, bottom*/,
             int nb3, int nb4  /*left, right*/,
             float c1, float c2 /*top, bottom*/,
             float c3, float c4 /*left, right*/)
/*< initialization >*/
{
    int c;
    nx = n1;  
    nz = n2;  
    nbt = nb1;  
    nbb = nb2;  
    nbl = nb3;  
    nbr = nb4;  
    ct = c1;  
    cb = c2;  
    cl = c3;  
    cr = c4;  
    if(nbt) wt =  sf_floatalloc(nbt);
    if(nbb) wb =  sf_floatalloc(nbb);
    if(nbl) wl =  sf_floatalloc(nbl);
    if(nbr) wr =  sf_floatalloc(nbr);
    c=0;
    abc_cal(c,nbt,ct,wt);
    abc_cal(c,nbb,cb,wb);
    abc_cal(c,nbl,cl,wl);
    abc_cal(c,nbr,cr,wr);
}
   

void bd2_close(void)
/*< free memory allocation>*/
{
    if(nbt) free(wt);
    if(nbb) free(wb);
    if(nbl) free(wl);
    if(nbr) free(wr);
}

void bd2_decay(float **a /*2-D matrix*/) 
/*< boundary decay>*/
{
    int iz, ix;
    for (iz=0; iz < nbt; iz++) {  
        for (ix=nbl; ix < nx+nbl; ix++) {
            a[iz][ix] *= wt[iz];
        }
    }
    for (iz=0; iz < nbb; iz++) {  
        for (ix=nbl; ix < nx+nbl; ix++) {
            a[iz+nz+nbt][ix] *= wb[nbb-1-iz];
        }
    }
    for (iz=nbt; iz < nz+nbt; iz++) {  
        for (ix=0; ix < nbl; ix++) {
            a[iz][ix] *= wl[ix];
        }
    }
    for (iz=nbt; iz < nz+nbt; iz++) {  
        for (ix=0; ix < nbr; ix++) {
            a[iz][nx+nbl+ix] *= wr[nbr-1-ix];
        }
    }
    for (iz=0; iz < nbt; iz++) {  
        for (ix=0; ix < nbl; ix++) {
            /* a[iz][ix] *= (float)iz/(float)(ix+iz)*wl[ix]+(float)ix/(float)(ix+iz)*wt[iz]; */
            a[iz][ix] *= iz>ix? wl[ix]:wt[iz];
        }
    }
    for (iz=0; iz < nbt; iz++) {  
        for (ix=0; ix < nbr; ix++) {
            a[iz][nx+nbl+nbr-1-ix] *= iz>ix? wr[ix]:wt[iz];
        }
    }
    for (iz=0; iz < nbb; iz++) {  
        for (ix=0; ix < nbl; ix++) {
            a[nz+nbt+nbb-1-iz][ix] *= iz>ix? wl[ix]:wb[iz];
        }
    }
    for (iz=0; iz < nbb; iz++) {  
        for (ix=0; ix < nbr; ix++) {
            a[nz+nbt+nbb-1-iz][nx+nbl+nbr-1-ix] *= iz>ix? wr[ix]:wb[iz];
        }
    }
}

void bdz_init(int n1, int n2, int n3 /*model size:x*/,
             int nb1, int nb2  /*top, bottom*/,
             int nb3, int nb4  /*left, right*/,
             float c1, float c2 /*top, bottom*/,
             float c3, float c4 /*left, right*/)
/*< initialization >*/
{
    int c;
    nx = n1;  
    nz = n2;  
    nh = n3;
    nbt = nb1;  
    nbb = nb2;  
    nbl = nb3;  
    nbr = nb4;  
    ct = c1;  
    cb = c2;  
    cl = c3;  
    cr = c4;  
    if(nbt) wt =  sf_floatalloc(nbt);
    if(nbb) wb =  sf_floatalloc(nbb);
    if(nbl) wl =  sf_floatalloc(nbl);
    if(nbr) wr =  sf_floatalloc(nbr);
    c=0;
    abc_cal(c,nbt,ct,wt);
    abc_cal(c,nbb,cb,wb);
    abc_cal(c,nbl,cl,wl);
    abc_cal(c,nbr,cr,wr);
}
   

float **extmodel(float **init_model,int nz,int nx,int np)
/*< extended model >*/
{
	float **p;
	int ix,iz;
	int nx2=nx+2*np; 
	int nz2=nz+2*np;

//	p=alloc2dfloat(nz2,nx2);	
   p= sf_floatalloc2(nz2,nx2);
	
	for (ix=0; ix < np; ix++) {
		for (iz=np; iz < nz+np; iz++) {
			p[ix][iz] = init_model[0][iz-np];
		}
	}
	
	for (ix=nx+np; ix < nx2; ix++) {
		for (iz=np; iz < nz+np; iz++) {
			p[ix][iz] = init_model[nx-1][iz-np];
		}
	}
	
	for (ix=np; ix < nx+np; ix++) {
		for (iz=nz+np; iz < nz2; iz++) {
			p[ix][iz] = init_model[ix-np][nz-1];
		}
	}

	for (ix=np; ix < np+nx; ix++) {
		for (iz=0; iz < np; iz++) {
			p[ix][iz] = init_model[ix-np][0];
		}
	}

	for (ix=0; ix < np; ix++) {
		for (iz=0; iz < np; iz++) {
			p[ix][iz] = init_model[0][0];
		}
	}
			
	for (ix=nx+np; ix < nx2; ix++) {
		for (iz=0; iz < np; iz++) {
			p[ix][iz] = init_model[nx-1][0];
		}
	}						

	for (ix=0; ix < np; ix++) {
		for (iz=nz+np; iz < nz2; iz++) {
			p[ix][iz] = init_model[0][nz-1];
		}
	}
	
	for (ix=nx+np; ix < nx2; ix++) {
		for (iz=nz+np; iz < nz2; iz++) {
			p[ix][iz] = init_model[nx-1][nz-1];
		}
	}
	
	for (ix=np; ix < nx+np; ix++) {
		for (iz=np; iz < nz+np; iz++) {
			p[ix][iz] = init_model[ix-np][iz-np];
		}
	}
			
	return p;
}	






void bdz_decay(float ***a /*3-D matrix*/) 
/*< boundary decay>*/
{
    int iz, ix, ih;
    for (iz=0; iz < nbt; iz++) {  
        for (ix=0; ix < nx; ix++) {
            for (ih=0; ih < nh; ih++){
                a[iz][ix][ih] *= wt[iz];
            }
        }
    }
    for (iz=0; iz < nbb; iz++) {  
        for (ix=0; ix < nx; ix++) {
            for (ih=0; ih < nh; ih++){
                a[iz+nz+nbt][ix][ih] *= wb[nbb-1-iz];
            }
        }
    }
}

void abc_cal(int abc /* decaying type*/,
             int nb  /* absorbing layer length*/, 
             float c /* decaying parameter*/,
             float* w /* output weight[nb] */)
/*< find absorbing coefficients >*/
{
    int ib;
    const float pi=SF_PI;
    if(!nb) return;
    switch(abc){
          default:
              for(ib=0; ib<nb; ib++){
                 w[ib]=exp(-c*c*(nb-1-ib)*(nb-1-ib));
              }
          break;
          case(1): 
              for(ib=0; ib<nb; ib++){
                 w[ib]=powf((1.0+0.9*cosf(((float)(nb-1.0)-(float)ib)/(float)(nb-1.0)*pi))/2.0,abc);
              }
    }   
}

