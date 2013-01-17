//   direct 2-D to 2-D Radon transform / double integral
//   Input f(w,x) complex
//   Output u(tau,p) complex
//   Call bfio.setup2 bfio.kernel2 bfio.diicheck2
//   In bfio.kernel2: fi=1 hyper Radon; fi=2 adjoint of hyper Radon;
//                    fi=3 x*k;         fi=4 -x*k;
//
//   Copyright (C) 2011 University of Texas at Austin
//  
//   This program is free software; you can redistribute it and/or modify
//   it under the terms of the GNU General Public License as published by
//   the Free Software Foundation; either version 2 of the License, or
//   (at your option) any later version.
//  
//   This program is distributed in the hope that it will be useful,
//   but WITHOUT ANY WARRANTY; without even the implied warranty of
//   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
//   GNU General Public License for more details.
//  
//   You should have received a copy of the GNU General Public License
//   along with this program; if not, write to the Free Software
//   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

#include "bfio.hh"
#include "serialize1.hh"

using namespace std;
using std::cerr;

int main(int argc, char** argv)
{
  srand48(time(NULL));
  clock_t ck0, ck1;
  time_t tt0, tt1;
    
  sf_init(argc,argv); // Initialize RSF
    
  // Get input
  iRSF input;

  int nw, nx;
  input.get("n1",nw);
  input.get("n2",nx);

  float w0, dw;
  input.get("o1",w0);
  input.get("d1",dw);  

  float x0, dx;
  input.get("o2",x0);
  input.get("d2",dx);


  std::valarray<sf_complex> fdata(nw*nx);
  //fdata.resize(nw*nx);

  input >> fdata;

  CpxNumMat f(nw,nx);  setvalue(f,cpx(0,0));
  for (int i=0; i<nw; i++)
    for (int j=0; j<nx; j++)
      f(i,j) = cpx(crealf(fdata[nw*j+i]),cimagf(fdata[nw*j+i]));
  FltNumVec w(nw);
  for (int i=0; i<nw; i++)  w(i) = w0+i*dw;
  FltNumVec x(nx);   
  for (int j=0; j<nx; j++)  x(j) = x0+j*dx;
 

  // Set output
  iRSF par(0);
  int ntau, np;
  par.get("ntau",ntau); 
  par.get("np",np); 

  float tau0, dtau;
  par.get("tau0",tau0);
  par.get("dtau",dtau);  

  float p0, dp;
  par.get("p0",p0);
  par.get("dp",dp);
 
  CpxNumMat u(ntau,np);  setvalue(u,cpx(0,0));
  FltNumVec tau(ntau);   
  for (int i=0; i<ntau; i++)  tau(i) = tau0+i*dtau;
  FltNumVec p(np);
  for (int j=0; j<np; j++)  p(j) = p0+j*dp;

  oRSF output;
  output.put("n1",ntau);
  output.put("n2",np);

  output.put("o1",tau0);
  output.put("d1",dtau);

  output.put("o2",p0);
  output.put("d2",dp);
 
  //output.type(SF_FLOAT);
  // this has be there if the input and output types are different

  // BFIO setup
  BFIO bfio("bfio_");
  iC( bfio.setup2(par,input) );

  float time_eval1, time_eval2;

  ck0 = clock(); 
  tt0 = time(0);
  iC( bfio.diicheck2(f,w,x,u,tau,p) );
  ck1 = clock();    
  tt1 = time(0);  
  time_eval1 = float(ck1-ck0)/CLOCKS_PER_SEC;
  time_eval2 = difftime(tt1,tt0);
  cerr<<"Teval1 "<<time_eval1<<endl;
  cerr<<"Teval2 "<<time_eval2<<endl;

  std::valarray<sf_complex> udata(ntau*np);
  //std::valarray<float> udata(ntau*np);
  //udata.resize(ntau*np);
    
  for (int i=0; i<ntau; i++)
    for (int j=0; j<np; j++)
      udata[ntau*j+i]=sf_cmplx(real(u(i,j)),imag(u(i,j)));
      //udata[ntau*j+i]=real(u(i,j));

  output << udata;

  exit(0);
}
