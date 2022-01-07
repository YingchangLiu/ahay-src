/* Variable-density acoustic Forward Modeling, FWI, and RTM */
/*
 Copyright (C) 2017 University of Texas at Austin
 
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
#include <mpi.h>
#include "Pmodeling.h"
#include "Pdfwi.h"
#include "Prtm.h"

int main(int argc, char* argv[])
{
	int function, para_type;
	bool verb;

	sf_mpi mpipar;
	sf_sou soupar;
	sf_acqui acpar;
	sf_vec_d array;
	sf_fwi_d fwipar;
	sf_optim optpar=NULL;

	MPI_Comm comm=MPI_COMM_WORLD;

	sf_file Fv, Fd, Fw, Fdat, Fgrad, Finv=NULL, Ferr=NULL, Fmod=NULL;

	sf_init(argc, argv);

	MPI_Init(&argc, &argv);
	MPI_Comm_rank(comm, &mpipar.cpuid);
	MPI_Comm_size(comm, &mpipar.numprocs);

	Fv=sf_input("Fvel"); /* velocity model */
	Fd=sf_input("Fd"); /* density model */
	Fw=sf_input("Fwavelet"); /* wavelet */

	soupar=(sf_sou)sf_alloc(1, sizeof(*soupar));
	acpar=(sf_acqui)sf_alloc(1, sizeof(*acpar));
	array=(sf_vec_d)sf_alloc(1, sizeof(*array));

	/* parameters I/O */
	if(!sf_getint("function", &function)) function=2;
	/* if 1, forward modeling; if 2, FWI; if 3, RTM */
	if(!sf_getint("para_type", &para_type)) para_type=1;
	/* if 1, velocity and density; if 2, velocity and impedance */

	if(!sf_histint(Fv, "n1", &acpar->nz)) sf_error("No n1= in Fv");
	if(!sf_histint(Fv, "n2", &acpar->nx)) sf_error("No n2= in Fv");
	if(!sf_histfloat(Fv, "d1", &acpar->dz)) sf_error("No d1= in Fv");
	if(!sf_histfloat(Fv, "d2", &acpar->dx)) sf_error("No d2= in Fv");
	if(!sf_histfloat(Fv, "o1", &acpar->z0)) sf_error("No o1= in Fv");
	if(!sf_histfloat(Fv, "o2", &acpar->x0)) sf_error("No o2= in Fv");
	if(!sf_histint(Fw, "n1", &acpar->nt)) sf_error("No n1= in Fw");
	if(!sf_histfloat(Fw, "d1", &acpar->dt)) sf_error("No d1= in Fw");
	if(!sf_histfloat(Fw, "o1", &acpar->t0)) sf_error("No o1= in Fw");

	if(!sf_getbool("verb", &verb)) verb=false; /* verbosity flag */
	if(!sf_getint("nb", &acpar->nb)) acpar->nb=20; /* PML boundary width */
	if(!sf_getfloat("coef", &acpar->coef)) acpar->coef=5.; /* maximum velocity of the medium */

	if(!sf_getint("acqui_type", &acpar->acqui_type)) acpar->acqui_type=1;
	/* if 1, fixed acquisition; if 2, marine acquisition; if 3, symmetric acquisition */
	if(!sf_getint("ns", &acpar->ns)) sf_error("shot number required"); /* shot number */
	if(!sf_getfloat("ds", &acpar->ds)) sf_error("shot interval required"); /* shot interval */
	if(!sf_getfloat("s0", &acpar->s0)) sf_error("shot origin required"); /* shot origin */
	if(!sf_getint("sz", &acpar->sz)) acpar->sz=3; /* source depth */
	if(!sf_getint("nr", &acpar->nr)) acpar->nr=acpar->nx; /* number of receiver */
	if(!sf_getfloat("dr", &acpar->dr)) acpar->dr=acpar->dx; /* receiver interval */
	if(!sf_getfloat("r0", &acpar->r0)) acpar->r0=acpar->x0; /* receiver origin */
	if(!sf_getint("rz", &acpar->rz)) acpar->rz=3; /* receiver depth */

	if(!sf_getint("interval", &acpar->interval)) acpar->interval=1; /* wavefield storing interval */

	if(!sf_getfloat("fhi", &soupar->fhi)) soupar->fhi=0.5/acpar->dt; /* high frequency in band, default is Nyquist */
	if(!sf_getfloat("flo", &soupar->flo)) soupar->flo=0.; /* low frequency in band, default is zero */
	if(!sf_getint("frectx", &soupar->frectx)) soupar->frectx=2; /* source smoothing in x */
	if(!sf_getint("frectz", &soupar->frectz)) soupar->frectz=2; /* source smoothing in z */

	/* get prepared */
	preparation_d(Fv, Fd, Fw, acpar, soupar, array);

	if(function == 1){ // forward modeling

		Fdat=sf_output("output"); /* shot data */

		/* dimension set up */
		sf_putint(Fdat, "n1", acpar->nt);
		sf_putfloat(Fdat, "d1", acpar->dt);
		sf_putfloat(Fdat, "o1", acpar->t0);
		sf_putstring(Fdat, "label1", "Time");
		sf_putstring(Fdat, "unit1", "s");
		sf_putint(Fdat, "n2", acpar->nr);
		sf_putfloat(Fdat, "d2", acpar->dr);
		sf_putfloat(Fdat, "o2", acpar->r0);
		sf_putstring(Fdat, "label2", "Receiver");
		sf_putstring(Fdat, "unit2", "km");
		sf_putint(Fdat, "n3", acpar->ns);
		sf_putfloat(Fdat, "d3", acpar->ds);
		sf_putfloat(Fdat, "o3", acpar->s0);
		sf_putstring(Fdat, "label3", "Shot");
		sf_putstring(Fdat, "unit3", "km");

		forward_modeling_d(Fdat, &mpipar, soupar, acpar, array, para_type, verb);

		sf_fileclose(Fdat);
	}
	else if(function == 2){ // FWI

		fwipar=(sf_fwi_d)sf_alloc(1, sizeof(*fwipar));
		if(!sf_getbool("onlygrad", &fwipar->onlygrad)) fwipar->onlygrad=false; 
		/* only calculate gradident or not */
		if(!sf_getint("grad_type",&fwipar->grad_type)) fwipar->grad_type=1;
		/* if 1, velocity; if 2, impedance or density */
		if(!sf_getint("rfwi",&fwipar->rfwi)) fwipar->rfwi=0;
		/* if 0, fwi gradient; if 1, rfwi gradient with Vp-Ip scale separation */

		if(!sf_getfloat("wt1", &fwipar->wt1)) fwipar->wt1=acpar->t0; /* window data residual: tmin */
		if(!sf_getfloat("wt2", &fwipar->wt2)) fwipar->wt2=acpar->t0+(acpar->nt-1)*acpar->dt; /* window data residual: tmax */
		if(!sf_getfloat("woff1", &fwipar->woff1)) fwipar->woff1=acpar->r0; /* window data residual: rmin */
		if(!sf_getfloat("woff2", &fwipar->woff2)) fwipar->woff2=acpar->r0+(acpar->nr-1)*acpar->dr; /* window data residual: rmax */
		if(!sf_getfloat("v0", &fwipar->v0)) fwipar->v0=1.5; /* surface velocity for cutting direct wave */
		if(!sf_getfloat("t0", &fwipar->t0)) fwipar->t0=-1.; /* starting time for cutting direct wave */
		if(!sf_getfloat("gain", &fwipar->gain)) fwipar->gain=1; /* vertical gain power of data residual */
		if(!sf_getint("waterz", &fwipar->waterz)) fwipar->waterz=51; /* water layer depth */
		if(!sf_getint("grectx", &fwipar->grectx)) fwipar->grectx=3; /* gradient smoothing radius in x */
		if(!sf_getint("grectz", &fwipar->grectz)) fwipar->grectz=3; /* gradient smoothing radius in z */

		if(!sf_getfloat("v1", &fwipar->v1)) fwipar->v1=0.; /* lower limit of estimated velocity */
		if(!sf_getfloat("v2", &fwipar->v2)) fwipar->v2=10.; /* upper limit of estimated velocity */
		if(!sf_getfloat("den1", &fwipar->den1)) fwipar->den1=0.; /* lower limit of estimated density or impedance */
		if(!sf_getfloat("den2", &fwipar->den2)) fwipar->den2=10.; /* upper limit of estimated density or impedance */

		Fdat=sf_input("Fdat"); /* input data */
		if(!fwipar->onlygrad){
			Finv=sf_output("output"); /* FWI result */
			Ferr=sf_output("Ferr"); /* data misfit convergence curve */
			Fmod=sf_output("Fmod"); /* all iteration models */
		}
		Fgrad=sf_output("Fgrad"); /* FWI gradient at first iteration */

		sf_putint(Fgrad, "n1", acpar->nz);
		sf_putfloat(Fgrad, "d1", acpar->dz);
		sf_putfloat(Fgrad, "o1", acpar->z0);
		sf_putstring(Fgrad, "label1", "Depth");
		sf_putstring(Fgrad, "unit1", "km");
		sf_putint(Fgrad, "n2", acpar->nx);
		sf_putfloat(Fgrad, "d2", acpar->dx);
		sf_putfloat(Fgrad, "o2", acpar->x0);
		sf_putstring(Fgrad, "label2", "Distance");
		sf_putstring(Fgrad, "unit2", "km");

		if(!fwipar->onlygrad){
			optpar=(sf_optim)sf_alloc(1, sizeof(*optpar));
			if(!sf_getint("niter", &optpar->niter)) sf_error("iteration number required"); /* iteration number */
			if(!sf_getfloat("conv_error", &optpar->conv_error)) sf_error("convergence error required"); /* final convergence error */
			if(!sf_getint("npair", &optpar->npair)) optpar->npair=20; /* number of l-BFGS pairs */
			if(!sf_getint("nls", &optpar->nls)) optpar->nls=20; /* line search number */
			if(!sf_getfloat("factor", &optpar->factor)) optpar->factor=10; /* step length increase factor */
			if(!sf_getint("repeat", &optpar->repeat)) optpar->repeat=5; /* after how many iterations the step length goes back to 1 */
			optpar->c1=1e-4;
			optpar->c2=0.9;
			optpar->err=sf_floatalloc(optpar->niter+1);
		}
		/* dimension set up */
		if(Finv != NULL){
			sf_putint(Finv, "n1", acpar->nz);
			sf_putfloat(Finv, "d1", acpar->dz);
			sf_putfloat(Finv, "o1", acpar->z0);
			sf_putstring(Finv, "label1", "Depth");
			sf_putstring(Finv, "unit1", "km");
			sf_putint(Finv, "n2", acpar->nx);
			sf_putfloat(Finv, "d2", acpar->dx);
			sf_putfloat(Finv, "o2", acpar->x0);
			sf_putstring(Finv, "label2", "Distance");
			sf_putstring(Finv, "unit2", "km");
			
			sf_putint(Ferr, "n1", optpar->niter+1);
			sf_putfloat(Ferr, "d1", 1);
			sf_putfloat(Ferr, "o1", 0);
			sf_putstring(Ferr, "label1", "Iterations");
			sf_putstring(Ferr, "unit1", "");
			sf_putint(Ferr, "n2", 1);
			sf_putfloat(Ferr, "d2", 1);
			sf_putfloat(Ferr, "o2", 0);

			sf_putint(Fmod, "n1", acpar->nz);
			sf_putfloat(Fmod, "d1", acpar->dz);
			sf_putfloat(Fmod, "o1", acpar->z0);
			sf_putstring(Fmod, "label1", "Depth");
			sf_putstring(Fmod, "unit1", "km");
			sf_putint(Fmod, "n2", acpar->nx);
			sf_putfloat(Fmod, "d2", acpar->dx);
			sf_putfloat(Fmod, "o2", acpar->x0);
			sf_putstring(Fmod, "label2", "Distance");
			sf_putstring(Fmod, "unit2", "km");
			sf_putint(Fmod, "n3", optpar->niter+1);
			sf_putfloat(Fmod, "d3", 1);
			sf_putfloat(Fmod, "o3", 0);
			sf_putstring(Fmod, "label3", "Iteration");
		}

		fwi(Fdat, Finv, Ferr, Fmod, Fgrad, &mpipar, soupar, acpar, array, fwipar, optpar, para_type, verb);

		if(!fwipar->onlygrad){
			sf_fileclose(Finv);
			sf_fileclose(Ferr);
			sf_fileclose(Fmod);
		}
		sf_fileclose(Fgrad);
	}
	else if(function == 3){ // RTM
//		Fdat=sf_input("Fdat"); /* input data */
//		Fimg=sf_output("output"); /* rtm image */
//
//		/* dimension set up */
//		sf_putint(Fimg, "n1", acpar->nz);
//		sf_putfloat(Fimg, "d1", acpar->dz);
//		sf_putfloat(Fimg, "o1", acpar->z0);
//		sf_putstring(Fimg, "label1", "Depth");
//		sf_putstring(Fimg, "unit1", "km");
//		sf_putint(Fimg, "n2", acpar->nx);
//		sf_putfloat(Fimg, "d2", acpar->dx);
//		sf_putfloat(Fimg, "o2", acpar->x0);
//		sf_putstring(Fimg, "label2", "Distance");
//		sf_putstring(Fimg, "unit2", "km");
//
//		rtm_q(Fdat, Fimg, &mpipar, soupar, acpar, array, verb);
//
//		sf_fileclose(Fimg);
	}

	MPI_Finalize();
	exit(0);
}
