/* Trace And Header GET Header Word prints trace headers.

tah is the abbreviation of Trace And Header.  Madagascar programs 
that begin with sftah are a designed to:
1- read trace and headers from separate rsf files and write them to 
   standard output (ie sftahread)
2- filter programs that read and write standard input/output and 
   process the tah data (eg sftahnmo, sftahstack)
3- read tah data from standard input and write separate rsf files for 
   the trace and headers data (ie sftahwrite)

These programs allow Seismic Unix (su) like processing in Madagascar.  
Some programs have su like names.

Some programs in this suite are sftahread, sftahgethw, ftahhdrmath, 
and sftahwrite.

The sftahgethw program prints headers.  List the headers you want to
print in the key parameter.
EXAMPLE:

sftahread \\
   verbose=1 \\
   input=npr3_gathers.rsf \\
| sftahgethw \\
   verbose=0  \\
   key=sx,sy,gx,gy,offset  \\
>/dev/null

The headers are in the file npr3_gathers_hdr.rsf, 
the headers parameter default.  The headers are merged with the trace 
amplitudes and the tah data sent down the pipe for sftahgethw.  The 
source and group coordinates and offset (sx,sy,gx,gy,offset) are 
printed to STDERR.  Traces are sent to STDOUT, which is directed to
/dev/null (the bit bucket).

PARAMETERS
   strings key= no default

        list of header keys print.  Look at the sfsegyread for a list
	of header names.

*/

/*
  Copyright (C) 2013 University of Texas at Austin
  
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
/*
   Program change history:
   date       Who             What
   04/26/2013 Karl Schleicher Original program
   10/22/2013 Karl Schleicher Factor reused functions into tahsub.c
*/
#include <string.h>
#include <rsf.h>
#include <rsfsegy.h>

#include "tahsub.h"
#include "extraalloc.h"

int main(int argc, char* argv[])
{
  int verbose;
  sf_file in=NULL, out=NULL;
  int n1_traces;
  int n1_headers;

  char* header_format=NULL;
  sf_datatype typehead;
  /* kls do I need to add this?  sf_datatype typein; */
  float* fheader=NULL;
  float* intrace=NULL;
  int numkeys;
  int ikey;
  char** list_of_keys;
  int *indx_of_keys;
  int indx_sx, indx_sy, indx_gx, indx_gy;
  int superbin_maxfold;
  float** superbin_traces;
  float** superbin_headers;
  int itrace;
  int iitrace;
  bool eof_get_tah;
  bool end_of_gather;
  float sx, sy, gx, gy, offx, offy;
  int fold;
  float* outtrace=NULL;
  float* outheader=NULL;  
  int* ioutheader=NULL;  

  /*****************************/
  /* initialize verbose switch */
  /*****************************/
  sf_init (argc,argv);

  if(!sf_getint("verbose",&verbose))verbose=1;
      /* \n
     flag to control amount of print
     0 terse, 1 informative, 2 chatty, 3 debug
  */
  sf_warning("verbose=%d",verbose);
 
  /******************************************/
  /* input and output data are stdin/stdout */
  /******************************************/

  if(verbose>0)fprintf(stderr,"read in file name\n");  
  in = sf_input ("in");

  if(verbose>0)fprintf(stderr,"read out file name\n");
  out = sf_output ("out");

  if (!sf_histint(in,"n1_traces",&n1_traces))
    sf_error("input data not define n1_traces");
  if (!sf_histint(in,"n1_headers",&n1_headers)) 
    sf_error("input data does not define n1_headers");

  header_format=sf_histstring(in,"header_format");
  if(strcmp (header_format,"native_int")==0) typehead=SF_INT;
  else                                       typehead=SF_FLOAT;

  if(verbose>0)fprintf(stderr,"allocate headers.  n1_headers=%d\n",n1_headers);
  fheader = sf_floatalloc(n1_headers);
 
  if(verbose>0)fprintf(stderr,"allocate intrace.  n1_traces=%d\n",n1_traces);
  intrace= sf_floatalloc(n1_traces);

  if(verbose>0)fprintf(stderr,"call list of keys\n");
 
  list_of_keys=sf_getnstring("key",&numkeys);
  if(list_of_keys==NULL)
    sf_error("The required parameter \"key\" was not found.");
  /* I wanted to use sf_getstrings, but it seems to want a colon seperated
     list of keys (eg key=offset:ep:fldr:cdp) and I wanted a comma seperated
     list of keys (eg key=offset:ep:fldr:cdp).
  numkeys=sf_getnumpars("key");
  if(numkeys==0)
    sf_error("The required parameter \"key\" was not found.");
  fprintf(stderr,"alloc list_of_keys numkeys=%d\n",numkeys);
  list_of_keys=(char**)sf_alloc(numkeys,sizeof(char*)); 
  sf_getstrings("key",list_of_keys,numkeys);
  */
  /* print the list of keys */
  if(verbose>1){
    fprintf(stderr,"numkeys=%d\n",numkeys);
    for(ikey=0; ikey<numkeys; ikey++){
      fprintf(stderr,"list_of_keys[%d]=%s\n",ikey,list_of_keys[ikey]);
    }
  }
  
  /* maybe I should add some validation that n1== n1_traces+n1_headers+2
     and the record length read in the second word is consistent with 
     n1.  */

  /**********************************************************/
  /* end code block for standard tah Trace And Header setup */
  /* continue with any sf_puthist this tah program calls to */
  /* add to the history file                                */
  /**********************************************************/

  /* put the history from the input file to the output */
  sf_fileflush(out,in);

  /********************************************************/
  /* continue initialization specific to this tah program */
  /********************************************************/

  /* segy_init gets the list header keys required by segykey function  */
  segy_init(n1_headers,in);
  indx_of_keys=sf_intalloc(numkeys);
  for (ikey=0; ikey<numkeys; ikey++){
    /* kls need to check each of these key names are in the segy header and
       make error message for invalid keys.  Of does segykey do this? NO, just
       segmentation fault. */
    indx_of_keys[ikey]=segykey(list_of_keys[ikey]);
  }

  /* I used Mtahstack.c and Mtahgethw to for inspiration for data flow */

  /* more initalization before starting the trace read loop */
  indx_sx=segykey("sx");
  indx_sy=segykey("sy");
  indx_gx=segykey("gx");
  indx_gy=segykey("gy");

  /* allocate processing arrays */
  superbin_maxfold=1000;
  superbin_traces =sf_floatalloc2(n1_traces , superbin_maxfold);
  superbin_headers=sf_floatalloc2(n1_headers, superbin_maxfold);
  
  /* allocate output trace arrays */
  outtrace  = sf_floatalloc(n1_traces);
  outheader = sf_floatalloc(n1_headers);
  ioutheader=(int*)outheader;

   /***************************/
  /* start trace loop        */
  /***************************/
  if(verbose>0)fprintf(stderr,"start trace loop\n");

  if(verbose>0)fprintf(stderr,"start the the trace read loop\n");
  itrace=0;
  eof_get_tah=get_tah(intrace, fheader, n1_traces, n1_headers, in);
  fold=0;
  while (!eof_get_tah){
    if(verbose>1 || (verbose==1 && itrace<5)){
      fprintf(stderr,"process the tah in sftahstack\n");
      for(ikey=0; ikey<numkeys; ikey++){
	fprintf(stderr," %s=",list_of_keys[ikey]);
	if(typehead == SF_INT){
	  /* just cast the header to int so the print works */
	  fprintf(stderr,"%d",((int*)fheader)[indx_of_keys[ikey]]);
	} else {
	  fprintf(stderr,"%f",fheader[indx_of_keys[ikey]]);
	}
      } /* end of the for(ikey..) loop */
      fprintf(stderr,"\n");
    }
    /********************/
    /* process the tah. */
    /********************/
    /* loop reading traces in supergather */
    
    if(fold==0){
      if(verbose>0)fprintf(stderr,"start a new supergather\n");
      /* apply any initialization */
    }
    if(fold>=superbin_maxfold){
      /* increase buffer sizes */
      superbin_maxfold*=1.2;
      superbin_traces =sf_floatrealloc2(superbin_traces, 
					n1_traces , superbin_maxfold);
      superbin_headers=sf_floatrealloc2(superbin_headers, 
					n1_headers, superbin_maxfold);
    }
    memcpy(superbin_headers[fold],fheader,n1_headers*sizeof(int));
    memcpy(superbin_traces [fold],intrace,n1_traces*sizeof(int));
    fold++;

    /* get the next trace so you can test the current and next headers
       to determine if this is the end of a superbin */
    eof_get_tah=get_tah(intrace, fheader, n1_traces, n1_headers, in);

    /* this is the end of gather if eof was encounterred or if at least one 
       of the header keys in indx_of_keys changed. */
    end_of_gather=eof_get_tah;

    /* if there is no next trace, do not try to test the header keys.  
       You already know it is the end of the superbin. */
    if(!eof_get_tah){ 
      if(fold>1){
	/* kls for now turn off the header key.  Need to turn it back on
	   when we start doing multiple suberbins */
	if(0)for(ikey=0; ikey<0*numkeys; ikey++){
	    if(typehead == SF_INT){
	      /* kls need to define prevheader and use it instead of stkheader.
		 stkheader was used in sftahstack and a more suitable name
		 needs to be seleted for the array holding the previous header
	      */
	      /*
	      if(((int*)fheader  )[indx_of_keys[ikey]]!=
		 ((int*)stkheader)[indx_of_keys[ikey]]){
		end_of_gather=true;
		break;
	      }
	      */
	    } else {
	      /* kls need to repeat the updates from previous code segment 
	      if(fheader[indx_of_keys[ikey]]!=stkheader[indx_of_keys[ikey]]){
		end_of_gather=true;
		break;
	      }
	      */
	    } /* end if(typehead == SF_INT)...else */
	  } /* end for(ikey=0; ikey<0*numkeys; ikey++) */
      }
    } /* end if(!eof_get_tah) the code segment to test hdrs for key change */


    /* if this is the last trace in the gather, process the gather and
       set fold=0.  Fold=0 will cause gather initialization on the next
       iteration through the loop */
    if(end_of_gather){
      /***********************************/
      /* process the supergather         */
      /***********************************/
      /* OK.  You have a superbin in memory.  Do your 5D interpolation. */
      if(verbose>1)fprintf(stderr,"end_of_gather.  process it.\n");

      /* this loop just prints some headers. */
      if(verbose>1){
	for(iitrace=0; iitrace<fold; iitrace++){
	  if (typehead == SF_INT){
	    sx=((int**)superbin_headers)[iitrace][indx_sx];
	    sy=((int**)superbin_headers)[iitrace][indx_sy];
	    gx=((int**)superbin_headers)[iitrace][indx_gx];
	    gy=((int**)superbin_headers)[iitrace][indx_gy];
	  } else {
	    sx=((float**)superbin_headers)[iitrace][indx_sx];
	    sy=((float**)superbin_headers)[iitrace][indx_sy];
	    gx=((float**)superbin_headers)[iitrace][indx_gx];
	    gy=((float**)superbin_headers)[iitrace][indx_gy];
	  }
	  offx=sx-gx;	
	  offy=sy-gy;
	  fprintf(stderr,"iitrace=%d,sx=%f,sy=%f,gx=%f,gy=%f,offx=%f,offy=%f\n",
		  iitrace   ,sx   ,sy   ,gx   ,gy   ,offx   ,offy);
	}
      }
      /* Do your 5D interpolation. */




      /* now you need loop to create the  output traces and hdrs and write 
	 them to the output pipe with put_tah */
      /***************************/
      /* write trace and headers */
      /***************************/
      /* kls as example I will just write the input traces back out.  
	 the real code will need to output all the interpolated traces, 
	 probable many more than fold traces.  Maybe the interpolated 
         results are in model domain, eg (kx,ky,koffx,koffy) or 
	 (tau,px,py,soffx,soffy,soffxy), and you will inverse transform
         to create traces at desired locations inside the output trace
         loop.
      */
      for (iitrace=0; iitrace<fold; iitrace++){
	if(typehead == SF_INT){
	  memcpy(ioutheader,superbin_headers[iitrace],n1_headers*sizeof(int));
	  /* kls you should set sx,sy,gx,gy,iline,xline,offset,cdpx,cdpy */
	  /* kls i know headers on Teapot are integer at this stage.  
	     Code should support integer of float.  Lots of madagascar used
	     floating point headers */
	} else {
	  sf_error("someone needs to update sftah5dinterp for float headers");
	}

	memcpy(outtrace,superbin_traces [iitrace],n1_traces*sizeof(int));
	memcpy(outheader,superbin_headers [iitrace],n1_traces*sizeof(int));

	put_tah(outtrace, outheader, n1_traces, n1_headers, out);
      
      }
      fold=0;
    }
    itrace++;
  }

  exit(0);
}

  
