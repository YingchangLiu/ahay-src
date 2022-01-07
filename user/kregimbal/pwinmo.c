/* Inverse normal moveout. */
/*
  Copyright (C) 2015 University of Texas at Austin
  
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

#include <math.h>

#include <rsf.h>
#include <rsfpwd.h>
#include "bandpass.h"

static int nh, nt, ns, jump, mode;
static float *stretch, **dense2, **sparse2, **dense3, **d; 
static sf_map4* nmo;
static sf_bands spl;
static bool nmod;

void inmo_init(const float velocity /* velocity */,
	       const float* off /* offset */,
	       int nh1 /* number of offsets */,
	       float h0, float dh, int CDPtype, int ix,
	       int nt1 /* time samples */, bool slow,
	       float t0, float dt, bool half, 
               float eps /* stretch */, 
	       float eps1 /* regularization */, 
	       int order   /* accuracy order */, 
	       int jump1 /* subsampling */, 
	       int mode1 /* mode of pwstack */, 
	       bool nmo1 /* constant vel nmo */)
/*< initialization >*/
{
    int it, ih;
    float f, h, *coord, *vel;
    const int nw=4;

    nh = nh1;
    ns = nt1;
    jump = jump1;
    nt = (ns-1)*jump+1;
    dt = dt/jump;
    mode = mode1;
    nmod = nmo1;
   
    vel = sf_floatalloc(nt);
    for (it=0; it < nt; it++) {
	vel[it] = velocity;
    }    

    nmo = (sf_map4*) sf_alloc(nh,sizeof(sf_map4));
    stretch = sf_floatalloc(nt);

    for (ih=0; ih < nh; ih++) {
	nmo[ih] = sf_stretch4_init (nt, t0, dt, nt, eps);
	
	h = off[ih] + (dh/CDPtype)*(ix%CDPtype); 
	if (half) h *= 2;
	h = h*h - h0*h0;
	    
	for (it=0; it < nt; it++) {
	    f = t0 + it*dt;
	    if (slow) {
		f = f*f + h*vel[it]*vel[it];
	    } else {
		f = f*f + h/(vel[it]*vel[it]);
	    }
	    if (f < 0.) {
		stretch[it]=t0-10.*dt;
	    } else {
		stretch[it] = sqrtf(f);
	    }
	}

	sf_stretch4_define (nmo[ih],stretch,false);
    }

    coord = sf_floatalloc(nt);
    for (it=0; it < nt; it++) {
	coord[it] = it;
    }

    spl = sf_spline_init(nw,ns);
    sf_int1_init (coord, 0.0, jump, ns, sf_spline_int, nw, nt, 0.0);
    free(coord);
    free(vel);
    
    predict_init (nt, nh, eps1*eps1, order, 1, false);
    
    dense2  = sf_floatalloc2(nt,nh);
    dense3  = sf_floatalloc2(nt,nh);
    //dip  = sf_floatalloc2(nt,nh);
    sparse2 = sf_floatalloc2(ns,nh);
}

void seislet_set(float **dip /* local slope */)
/*< set local slope >*/
{
    d = dip;
}

void inmo_close(void)
/*< free allocated storage >*/
{
    int ih;

    for (ih=0; ih < nh; ih++) {
	sf_stretch4_close(nmo[ih]);
    }
    free(nmo);
    free(stretch);
     
    sf_int1_close();
    sf_banded_close(spl);

    predict_close();

    free(*dense2);
    free(dense2);
    free(*dense3);
    free(dense3);
    free(*sparse2);
    free(sparse2);
}

void inmo(const float *trace   /* input trace [nt] */,
	  float **gather /* output CMP gather [nt][nh] */)
/*< apply inverse nmo >*/
{
    int ih;

    for (ih = 0; ih < nh; ih++) {
	sf_stretch4_apply (false,nmo[ih],(float*)trace,gather[ih]);
    }
}

void inmo2(float **ngath   /* input trace [nt][nh] */,
	  float **gather /* output CMP gather [nt][nh] */)
/*< apply inverse nmo >*/
{
    int ih, it;
    for (ih = 0; ih < nh; ih++) {
        for (it=0; it < nt; it++) {
	    gather[ih][it] = 0.0f;
        }
    }
    for (ih = 0; ih < nh; ih++) {
	sf_stretch4_apply (false,nmo[ih],ngath[ih],gather[ih]);
    }
}

void pwpaint(const float *trace   /* input trace [nt] */,
	  float **gather /* output CMP gather [nt][nh] */)
/*< apply predictive painting >*/
{
    int ih, it;
    float *trace1;
    trace1 = sf_floatalloc(nt);

    for (it=0; it < nt; it++) {
	    gather[0][it] = trace[it];
	    trace1[it] = trace[it];
    }
	for (ih=-1; ih >= 0; ih--) {
	    predict_step(false,false,trace1,d[ih]);
	    for (it=0; it < nt; it++) {
		gather[ih][it] = trace1[it];
	    }
	}
	for (it=0; it < nt; it++) {
	    trace1[it] = gather[0][it];
	}
	for (ih=1; ih < nh; ih++) {
	    predict_step(false,true,trace1,d[ih-1]);
	    for (it=0; it < nt; it++) {
		gather[ih][it] = trace1[it];
	    }
	}
      free(trace1);
}

void nmostack(float **gather /* input CMP gather */,
	      float *trace /* output trace */)
/*< apply NMO and stack >*/
{
    int ih, it;

    for (it=0; it < nt; it++) {
	trace[it] = 0.0f;
    }

    for (ih = 0; ih < nh; ih++) {
	sf_stretch4_invert (false,nmo[ih],stretch,gather[ih]);
	for (it=0; it < nt; it++) {
	    trace[it] += stretch[it];
	}
    }

    /* normalize */
    for (it=0; it < nt; it++) {
	trace[it] /= nh;
    }
}

void cnmo(float **gather /* input CMP gather */,
	      float **ngath /* output NMO gather */)
/*< apply NMO and stack >*/
{
    int ih, it;
    for (ih = 0; ih < nh; ih++) {
        for (it=0; it < nt; it++) {
	    ngath[ih][it] = 0.0f;
        }
    }
    for (ih = 0; ih < nh; ih++) {
	sf_stretch4_invert (false,nmo[ih],stretch,gather[ih]);	
	for (it=0; it < nt; it++) {
	    ngath[ih][it] = stretch[it];
	}
    }

}

void pwstack(float **gather /* input CMP gather */,
	      float *trace /* output trace */,
	     int mode1)
/*< apply pwstack >*/
{
    int ih, it;
    float *t1, *t2;
    t1 = sf_floatalloc(nt);
    t2 = sf_floatalloc(nt);
    
    for (it=0; it < nt; it++) {
	trace[it] = 0.0f;
    }
    if (mode1 == 1) {
    	/* load the last trace */
    	for (it=0; it < nt; it++) {
		trace[it] = gather[nh-1][it];
   	} 
    	/* Predict backward */
    	for (ih=nh-2; ih >= 0; ih--) {
		predict_step(false,false,trace,d[ih]);
		for (it=0; it < nt; it++) {
	    	trace[it] += gather[ih][it];
		}
    	} 
    	/* normalize */
    	for (it=0; it < nt; it++) {
		trace[it] /= nh;
    	}
    }
    else {	
	for (ih=nh-2; ih >= 0; ih--) {
		for (it=0; it < nt; it++){
			t1[it] = gather[ih][it];     
		}
		predict_step(false,true,t1,d[ih]);
		for (it=0; it < nt; it++) {
			gather[ih+1][it] -= t1[it];     
			/* d = o - P[e] */
		} 
		for (it=0; it < nt; it++) {
			t2[it] = gather[ih+1][it];     
		}
		predict_step(false,false,t2,d[ih]);
		for (it=0; it < nt; it++) {
			gather[ih][it] += t2[it]/2;
			/* s = e + U[d] */
		}
   	} 
    	for (it=0; it < nt; it++) {
		trace[it] = gather[0][it];     
    	}
    }
    free(t1);   
    free(t2);
}

void subsample(float **dense, float **sparse) 
/*< subsample a gather >*/
{
    int ih, it, is;

    for (ih = 0; ih < nh; ih++) {
	for (it=is=0; it < nt; it+=jump, is++) {
	    sparse[ih][is] = dense[ih][it];
	}
    }
}


void interpolate(float **sparse, float **dense)
/*< interpolate to a denser sampling >*/
{
    int ih;

    for (ih = 0; ih < nh; ih++) {
	sf_banded_solve(spl,sparse[ih]);
	sf_int1_lop (false,false,ns,nt,sparse[ih],dense[ih]);
    }

}

void inmo_oper(int nx, const float *trace, float *trace2, void* user_data)
/*< operator to invert by GMRES >*/
{
    int it;
    
    /* forward operator */
    inmo(trace,dense2);
    subsample(dense2,sparse2);
    /* backward operator */
    interpolate(sparse2,dense2);
    nmostack(dense2,trace2);

    /* I + S (BF - I) */
    for (it=0; it < nt; it++) {
	trace2[it] -= trace[it];
    }
    bandpass(trace2);
    for (it=0; it < nt; it++) {
	trace2[it] += trace[it];
    }
}
	    

void pwstack_oper(int nx, const float *trace, float *trace2, void* user_data)
/*< operator to invert by GMRES >*/
{
    int it;
    
    if (nmod == false) {
    	/* forward operator */
    	pwpaint(trace,dense2); 
    	subsample(dense2,sparse2);
    	/* backward operator */
    	interpolate(sparse2,dense2);
    	pwstack(dense2,trace2,mode);

    	/* I + S (BF - I) */
    	for (it=0; it < nt; it++) {
		trace2[it] -= trace[it];
    	}
    	bandpass(trace2);
    	for (it=0; it < nt; it++) {
		trace2[it] += trace[it];
    	}
    }
    else {
      	/* forward operator */
    	pwpaint(trace,dense2); 
	inmo2(dense2,dense3);
    	subsample(dense3,sparse2);
    	/* backward operator */
    	interpolate(sparse2,dense2);
        cnmo(dense2,dense3);
    	pwstack(dense3,trace2,mode); 

    	/* I + S (BF - I) */
    	for (it=0; it < nt; it++) {
		trace2[it] -= trace[it];
    	}
    	bandpass(trace2);
    	for (it=0; it < nt; it++) {
		trace2[it] += trace[it];
    	}
    }
}
   
