/* 2-D Low-rank One-step Least Pre-stack Reverse-Time-Migration in the complex domain (both img and data are complex valued)
     img :  crosscorrelation with source normalization (stdout)
*/
/*
  Copyright (C) 2014 University of Texas at Austin
  
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
#include <math.h>
#include <time.h>
#include <sys/time.h>
#ifdef _OPENMP
#include <omp.h>
#endif
#ifdef SF_HAS_FFTW
#include <fftw3.h>
#endif

#include "waveutils.h"
#include "muting.h"

/*******************************************************/
/* main function */
int main(int argc, char* argv[]) 
{
    clock_t tstart,tend;
    double duration;

    /*flags*/
    bool verb, adj; /* migration(adjoint) flag */
    bool wantwf; /* outputs wavefield snapshots */
    bool justrec; /* actually means "need record" */
    bool illum; /* source illumination flag*/
    bool roll; /* survey strategy */
    bool mute; /* muting the direct arrival */
    bool born; /* Exact born modelling and velocity sensitivity kernel imaging condition */

    /*I/O*/
    sf_file Fvel;
    sf_file left, right, leftb, rightb;
    sf_file Fsrc, Frcd/*source and record*/;
    sf_file Ftmpwf;
    sf_file Fimg=NULL;

    /*axis*/
    sf_axis at, ax, az, as;

    /*geopar variables*/
    int nx, nz;
    int nxb, nzb;
    float dx, dz, ox, oz;
    int spx, spz, gpz, gpx, gpl; /*source/geophone location*/
    int snpint;
    int top, bot, lft, rht; /*abc boundary*/
    int nt;
    float dt;
    float trunc; 

    /*wavefield time axis*/
    int wfnt;
    float wfdt;

    /*misc*/
    int nzx, nx2, nz2, n2, m2, m2b, pad1, nk;
    int ix, iz, it, is;

    /*muting parameters*/
    float vref;
    int wd;

    /*Propagator*/
    sf_complex **ltf, **rtf;
    sf_complex **ltb, **rtb;

    /*Data*/
    sf_complex ***wvfld;
    sf_complex ***record, **tmprec, **img, **imgsum;
    float **sill;
    float **vel,**vel2;

    /*source/shots*/
    sf_complex *ww;
    float *rr;
    int rectz,rectx,repeat; /*refl smoothing parameters*/
    int sht0,shtbgn,shtend,shtnum,shtnum0,shtint,shtcur;

    /*tmp*/
    int tmpint;

    /*parameter structs*/
    geopar geop;
    mpipar mpip;

    /*MPI*/
    int cpuid, numprocs;
    sf_complex *sendbuf, *recvbuf;

    MPI_Init(&argc, &argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &cpuid);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);

    sf_init(argc, argv);

    if(cpuid==0) sf_warning("numprocs=%d",numprocs);

    if (!sf_getbool("verb", &verb)) verb=false; /*verbosity*/
    if (!sf_getbool("justrec", &justrec)) justrec=false; /*just model for the seismic record*/
    if (!sf_getbool("adj", &adj)) adj=true; /*migration*/
    if (!sf_getbool("wantwf", &wantwf)) wantwf=false; /*output forward and backward wavefield*/
    if (!sf_getbool("illum", &illum)) illum=false; /*if n, no source illumination applied */
    if (!sf_getbool("roll", &roll)) roll=false; /*if n, receiver is independent of source location and gpl=nx*/
    if (!sf_getbool("born", &born)) born=false; /*use exact born approximation*/
    /* source/receiver info */
    if (!sf_getint("shtbgn", &shtbgn)) sf_error("Need shot starting location on grid!");
    if (!sf_getint("sht0", &sht0)) sht0=shtbgn; /*actual shot origin on grid*/
    if (!sf_getint("shtend", &shtend)) sf_error("Need shot ending location on grid!");
    if (!sf_getint("shtint", &shtint)) sf_error("Need shot interval on grid!");
    shtnum = (int)((shtend-shtbgn)/shtint) + 1;
    shtnum0 = shtnum;
    if (!sf_getint("spz", &spz)) sf_error("Need source depth!");
    if (!sf_getint("gpz", &gpz)) sf_error("Need receiver depth!");
    if (roll) if (!sf_getint("gpl", &gpl)) sf_error("Need receiver length");
    if (!sf_getint("snapinter", &snpint)) snpint=1;     /* snap interval */
    /*--- parameters of source ---*/
    if (!sf_getfloat("srctrunc", &trunc)) trunc=0.4;
    if (!sf_getint("rectz", &rectz)) rectz=1;
    if (!sf_getint("rectx", &rectx)) rectx=1;
    if (!sf_getint("repeat", &repeat)) repeat=0;
    /* abc parameters */
    if (!sf_getint("top", &top)) top=40;
    if (!sf_getint("bot", &bot)) bot=40;
    if (!sf_getint("lft", &lft)) lft=40;
    if (!sf_getint("rht", &rht)) rht=40;
    /* muting for migration after modeling */
    if (justrec) {
      if (cpuid==0) sf_warning("using justrec mode, preceding the adj parameter (setting to false)!");
      adj = false;
      if (!sf_getbool("mute", &mute)) mute=true; /*muting direct arrival*/
      if (mute) {
	if (!sf_getfloat("vref", &vref)) vref=1500;
	if (!sf_getint("wd", &wd)) wd=5;
      }
    }

    /*Set I/O file*/
    if (adj) { /* migration */
      Frcd = sf_input("--input");
      Fimg  = sf_output("--output");
    } else { /* modeling */
      if (!justrec) Fimg = sf_input("--input");
      Frcd = sf_output("--output");
    }
    Fsrc  = sf_input("src");   /*source wavelet*/      
    left  = sf_input("left");
    right = sf_input("right");
    leftb  = sf_input("leftb");
    rightb = sf_input("rightb");
    Fvel  = sf_input("vel");  /*velocity - just for model dimension if born=false*/
    if (wantwf) {
	Ftmpwf  = sf_output("tmpwf");/*wavefield snap*/
    } else {
	Ftmpwf  = NULL;
    }

    /*--- Axes parameters ---*/
    at = sf_iaxa(Fsrc, 1); nt = sf_n(at);  dt = sf_d(at);      
    az = sf_iaxa(Fvel, 1); nzb = sf_n(az); dz = sf_d(az); oz = sf_o(az);
    ax = sf_iaxa(Fvel, 2); nxb = sf_n(ax); dx = sf_d(ax); ox = sf_o(ax);
    nzx = nzb*nxb;
    nz = nzb - top - bot;
    nx = nxb - lft - rht;
    if (!roll) gpl = nx; /* global survey setting */
    /* wavefield axis */
    wfnt = (int)(nt-1)/snpint+1;
    wfdt = dt*snpint;

    /* propagator matrices */
    if (!sf_getint("pad1",&pad1)) pad1=1; /* padding factor on the first axis */
    nz2 = kiss_fft_next_fast_size(nzb*pad1);
    nx2 = kiss_fft_next_fast_size(nxb);
    nk = nz2*nx2; /*wavenumber*/
    if (!sf_histint(left,"n1",&n2) || n2 != nzx) sf_error("Need n1=%d in left",nzx);
    if (!sf_histint(left,"n2",&m2))  sf_error("Need n2= in left");
    if (!sf_histint(right,"n1",&n2) || n2 != m2) sf_error("Need n1=%d in right",m2);
    if (!sf_histint(right,"n2",&n2) || n2 != nk) sf_error("Need n2=%d in right",nk);

    if (!sf_histint(leftb,"n1",&n2) || n2 != nzx) sf_error("Need n1=%d in left",nzx);
    if (!sf_histint(leftb,"n2",&m2b))  sf_error("Need n2= in left");
    if (!sf_histint(rightb,"n1",&n2) || n2 != m2b) sf_error("Need n1=%d in right",m2b);
    if (!sf_histint(rightb,"n2",&n2) || n2 != nk) sf_error("Need n2=%d in right",nk);

    /*check record data*/
    if (adj) {
	sf_histint(Frcd,"n1", &tmpint);
	if (tmpint != nt ) sf_error("Error parameter n1 in record!");
	sf_histint(Frcd,"n2", &tmpint);
	if (tmpint != gpl ) sf_error("Error parameter n2 in record!");
	sf_histint(Frcd,"n3", &tmpint);
	if (tmpint != shtnum0 ) sf_error("Error parameter n3 in record!");
    }

    /*allocate memory*/
    if (born) {
      vel = sf_floatalloc2(nzb, nxb);
      vel2 = sf_floatalloc2(nz, nx);
    } else { vel = NULL; vel2 = NULL; }
    ww=sf_complexalloc(nt);
    rr=sf_floatalloc(nzx);
    ltf = sf_complexalloc2(nzx,m2);
    rtf = sf_complexalloc2(m2,nk);
    ltb = sf_complexalloc2(nzx,m2b);
    rtb = sf_complexalloc2(m2b,nk);
    geop = (geopar) sf_alloc(1, sizeof(*geop));
    mpip = (mpipar) sf_alloc(1, sizeof(*mpip));
    tmprec = sf_complexalloc2(nt, gpl);
    if (shtnum%numprocs!=0) {
      shtnum += numprocs-shtnum%numprocs;
      if (verb) sf_warning("Total shot number is not divisible by total number of nodes! shunum padded to %d.", shtnum);
    }
    if (cpuid==0) {
      record = sf_complexalloc3(nt, gpl, shtnum);
    } else record = NULL;
    wvfld = sf_complexalloc3(nz, nx, wfnt);
    if (illum) sill = sf_floatalloc2(nz, nx);
    else sill = NULL;
    img = sf_complexalloc2(nz, nx); /*not actually used when justrec=true*/
    if (adj) {
      imgsum = sf_complexalloc2(nz, nx);
#ifdef _OPENMP
#pragma omp parallel for private(ix,iz)
#endif
	for (ix=0; ix<nx; ix++)
	  for (iz=0; iz<nz; iz++)
	    imgsum[ix][iz] = sf_cmplx(0.,0.);
    } else imgsum = NULL;

    /*read from files*/
    if (born) {
      sf_floatread(vel[0],nzx,Fvel);
#ifdef _OPENMP
#pragma omp parallel for private(iz,ix)
#endif
      for (ix=0; ix<nx; ix++)
	for (iz=0; iz<nz; iz++)
	  vel2[ix][iz] = vel[ix+lft][iz+top]*vel[ix+lft][iz+top]*wfdt*wfdt;
    }
    sf_complexread(ww,nt,Fsrc);
    sf_complexread(ltf[0],nzx*m2,left);
    sf_complexread(rtf[0],m2*nk,right);
    sf_complexread(ltb[0],nzx*m2b,leftb);
    sf_complexread(rtb[0],m2b*nk,rightb);
    if (!adj && !justrec) sf_complexread(img[0],nx*nz,Fimg);
    if (cpuid==0) {
      if (adj) {
	sf_complexread(record[0][0], shtnum0*gpl*nt, Frcd);
	if (shtnum0%numprocs!=0) {
#ifdef _OPENMP
#pragma omp parallel for private(is,ix,it)
#endif
	  for (is=shtnum0; is<shtnum; is++)
	    for (ix=0; ix<gpl; ix++)
	      for (it=0; it<nt; it++)
		record[is][ix][it] = sf_cmplx(0.,0.);
	}
      } else {
#ifdef _OPENMP
#pragma omp parallel for private(is,ix,it)
#endif
	for (is=0; is<shtnum; is++)
	  for (ix=0; ix<gpl; ix++)
	    for (it=0; it<nt; it++)
	      record[is][ix][it] = sf_cmplx(0.,0.);
      }
    }
    
    /*load constant geopar elements*/
    mpip->cpuid=cpuid;
    mpip->numprocs=numprocs;
    /*load constant geopar elements*/
    geop->nx  = nx;
    geop->nz  = nz;
    geop->nxb = nxb;
    geop->nzb = nzb;
    geop->dx  = dx;
    geop->dz  = dz;
    geop->ox  = ox;
    geop->oz  = oz;
    geop->snpint = snpint;
    geop->spz = spz;
    geop->gpz = gpz;
    geop->gpl = gpl;
    geop->top = top;
    geop->bot = bot;
    geop->lft = lft;
    geop->rht = rht;
    geop->nt = nt;
    geop->dt = dt;
    geop->trunc = trunc;
    geop->adj   = adj;
    geop->verb  = verb;
    geop->illum = illum;
    geop->m2 = m2;
    geop->m2b = m2b;
    geop->pad1 = pad1;
    geop->ltf = ltf;
    geop->rtf = rtf;
    geop->ltb = ltb;
    geop->rtb = rtb;
    geop->ww  = ww;
    geop->rr  = rr;
    geop->mode= 0;

    /* output RSF files */

    if (cpuid==0) {
      if (adj) { /* migration */
	sf_setn(ax, nx);
	sf_setn(az, nz);
	/*write image*/
	sf_oaxa(Fimg, az, 1);
	sf_oaxa(Fimg, ax, 2);
	sf_settype(Fimg,SF_COMPLEX);
      } else { /* modeling */
	sf_setn(ax, gpl);
	as = sf_iaxa(Fvel, 2); /*how to allocate a rsf axis?*/
	sf_setn(as,shtnum0);
	sf_setd(as,shtint*dx);
	sf_seto(as,shtbgn*dx+ox);
	sf_oaxa(Frcd, at, 1);
	sf_oaxa(Frcd, ax, 2);
	sf_oaxa(Frcd, as ,3);
	sf_settype(Frcd,SF_COMPLEX);
      }
      
      if (wantwf) {
	sf_setn(ax, nx);
	sf_setn(az, nz);
	/*write temp wavefield */
	sf_setn(at, wfnt);
	sf_setd(at, wfdt);
	sf_oaxa(Ftmpwf, az, 1);
	sf_oaxa(Ftmpwf, ax, 2);
	sf_oaxa(Ftmpwf, at, 3);
	sf_settype(Ftmpwf,SF_COMPLEX);
      }
    }

    /*close RSF files*/
    sf_fileclose(Fvel);
    sf_fileclose(Fsrc);
    sf_fileclose(left);
    sf_fileclose(right);
    sf_fileclose(leftb);
    sf_fileclose(rightb);

    /*Now start the real work*/
    tstart = clock();

    for (is=0; is*numprocs<shtnum; is++){

      shtcur = is*numprocs+cpuid; // current shot index

      if (shtcur<shtnum0) {
	spx = shtbgn + shtint*(shtcur);
	if (roll)
	  gpx = spx - (int)(gpl/2);
	else
	  gpx = 0;
	geop->spx = spx;
	geop->gpx = gpx;
	
	if (verb) {
	  sf_warning("============================");
	  sf_warning("processing shot #%d", shtcur);
	  sf_warning("nx=%d nz=%d nt=%d", geop->nx, geop->nz, geop->nt);
	  sf_warning("nxb=%d nzb=%d ", geop->nxb, geop->nzb);
	  sf_warning("dx=%f dz=%f dt=%f", geop->dx, geop->dz, geop->dt);
	  sf_warning("top=%d bot=%d lft=%d rht=%d", geop->top, geop->bot, geop->lft, geop->rht);
	  sf_warning("rectz=%d rectx=%d repeat=%d srctrunc=%f",rectz,rectx,repeat,geop->trunc);
	  sf_warning("spz=%d spx=%d gpz=%d gpx=%d gpl=%d", spz, spx, gpz, gpx, gpl);
	  sf_warning("snpint=%d wfdt=%f wfnt=%d ", snpint, wfdt, wfnt);
	  sf_warning("sht0=%d shtbgn=%d shtend=%d shtnum0=%d shtnum=%d", sht0, shtbgn, shtend, shtnum0, shtnum);
	  if (roll) sf_warning("Rolling survey!");
	  else sf_warning("Global survey (gpl=nx)!");
	  if (illum) sf_warning("Using source illumination!");
	  else sf_warning("No source illumination!");
	  if (born) sf_warning("Using exact born modeling and adj operators!");
	  sf_warning("============================");
	}
	
	/*generate reflectivity map*/
	reflgen(nzb, nxb, spz+top, spx+lft, rectz, rectx, repeat, rr);
	
	lrosfor2(wvfld, sill, tmprec, geop);
	
	if (justrec && mute)
	  mutingc(nt, gpl, dt, dx, dz, spx, spz, gpz, vref, wd, tmprec);
      }

      if (!justrec) {

	if (born) { /*adjusting the background wavefield*/
	  if (shtcur<shtnum0) {

	    /*2nd order forward FD*/
#ifdef _OPENMP
#pragma omp parallel for private(iz,ix)
#endif
	    for (ix=0; ix<nx; ix++) {
	      for (iz=0; iz<nz; iz++) {
#ifdef SF_HAS_COMPLEX_H
		wvfld[0][ix][iz] = (wvfld[2][ix][iz] - 2.f*wvfld[1][ix][iz] + wvfld[0][ix][iz])/vel2[ix][iz];
#else
		wvfld[0][ix][iz] = sf_crmul( (wvfld[2][ix][iz] - sf_crmul(wvfld[1][ix][iz],2.f) + wvfld[0][ix][iz]) , 1.f/vel2[ix][iz] );
#endif
	      }
	    }

	    for (it=1; it<wfnt-1; it++) { /*2nd order central FD*/
#ifdef _OPENMP
#pragma omp parallel for private(iz,ix)
#endif
	      for (ix=0; ix<nx; ix++) {
		for (iz=0; iz<nz; iz++) {
#ifdef SF_HAS_COMPLEX_H
		  wvfld[it][ix][iz] = (wvfld[it+1][ix][iz] - 2.f*wvfld[it][ix][iz] + wvfld[it-1][ix][iz])/vel2[ix][iz];
#else
		  wvfld[it][ix][iz] = sf_crmul( (wvfld[it+1][ix][iz] - sf_crmul(wvfld[it][ix][iz],2.f) + wvfld[it-1][ix][iz]) , 1.f/vel2[ix][iz] );
#endif
		}
	      }
	    } /*it iteration*/

	    /*2nd order backward FD*/
#ifdef _OPENMP
#pragma omp parallel for private(iz,ix)
#endif
	    for (ix=0; ix<nx; ix++) {
	      for (iz=0; iz<nz; iz++) {
#ifdef SF_HAS_COMPLEX_H
		wvfld[wfnt-1][ix][iz] = (wvfld[wfnt-1][ix][iz] - 2.f*wvfld[wfnt-2][ix][iz] + wvfld[wfnt-3][ix][iz])/vel2[ix][iz];
#else
		wvfld[wfnt-1][ix][iz] = sf_crmul( (wvfld[wfnt-1][ix][iz] - sf_crmul(wvfld[wfnt-2][ix][iz],2.f) + wvfld[wfnt-3][ix][iz]) , 1.f/vel2[ix][iz] );
#endif
	      }
	    }
	    
	  } /*if (shtcur<shtnum0)*/
	} /*if (born)*/

	if(adj) {
	  if (cpuid==0) sendbuf = record[is*numprocs][0];
	  else sendbuf = NULL;
	  recvbuf = tmprec[0];
	  MPI_Scatter(sendbuf, gpl*nt, MPI_COMPLEX, recvbuf, gpl*nt, MPI_COMPLEX, 0, MPI_COMM_WORLD); // tmprec[ix][it] = record[is][ix][it];
	}
      
	if (shtcur<shtnum0) {
	  lrosback2(img, wvfld, sill, tmprec, geop);
	  if (adj) { /*local image reduction*/
#ifdef _OPENMP
#pragma omp parallel for private(ix,iz)
#endif
	    for (ix=0; ix<nx; ix++) {
	      for (iz=0; iz<nz; iz++) {
#ifdef SF_HAS_COMPLEX_H
		imgsum[ix][iz] += img[ix][iz];
#else
		imgsum[ix][iz] = sf_cadd(imgsum[ix][iz],img[ix][iz]);
#endif      
	      }
	    }
	  }
	}

      } /*if (!justrec)*/

      if (!adj) {
	if (cpuid==0) recvbuf = record[is*numprocs][0];
	else recvbuf = NULL;
	sendbuf = tmprec[0];
	MPI_Gather(sendbuf, gpl*nt, MPI_COMPLEX, recvbuf, gpl*nt, MPI_COMPLEX, 0, MPI_COMM_WORLD); // record[is][ix][it] = tmprec[ix][it];
      }

      if (wantwf && shtcur==0)
	sf_complexwrite(wvfld[0][0], wfnt*nx*nz, Ftmpwf);
    } /*shot iteration*/

    MPI_Barrier(MPI_COMM_WORLD);
    /*write record/image*/
    if (adj) {
      if (cpuid==0) {
#if MPI_VERSION >= 2
	sendbuf = (sf_complex *) MPI_IN_PLACE;
#else /* will fail */
	sendbuf = NULL;
#endif 
	recvbuf = imgsum[0];
      } else {
	sendbuf = imgsum[0];
      	recvbuf = NULL;
      }
      MPI_Reduce(sendbuf, recvbuf, nx*nz, MPI_COMPLEX, MPI_SUM, 0, MPI_COMM_WORLD); 
      if (cpuid==0)
	sf_complexwrite(imgsum[0], nx*nz, Fimg);
    } else {
      if (cpuid==0)
	sf_complexwrite(record[0][0], shtnum0*gpl*nt, Frcd);
    }

    /*free memory*/
    if (born) { free(*vel); free(vel); free(*vel2); free(vel2); }
    free(ww); free(rr);
    free(*ltf); free(ltf);
    free(*rtf); free(rtf);
    free(*ltb); free(ltb);
    free(*rtb); free(rtb);
    free(geop); free(mpip);
    free(*tmprec); free(tmprec);
    if (cpuid==0) {free(**record); free(*record); free(record);}
    free(**wvfld); free(*wvfld); free(wvfld);
    if (illum) {
      free(*sill); free(sill);
    }
    free(*img); free(img);
    if (adj) {
      free(*imgsum); free(imgsum);
    }

    tend = clock();
    duration=(double)(tend-tstart)/CLOCKS_PER_SEC;
    if (verb)
        sf_warning(">> The CPU time of single shot migration is: %f seconds << ", duration);

    MPI_Finalize();
    exit(0);
}

