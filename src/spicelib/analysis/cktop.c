/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1985 Thomas L. Quarles
Modified: 2000  AlansFixes
**********/

#include "ngspice.h"
#include <stdio.h>
#include "cktdefs.h"
#include "devdefs.h"
#include "sperror.h"


int
CKTop(CKTcircuit *ckt, long int firstmode, long int continuemode, int iterlim)
{
    int converged;
    int i;
    CKTnode *n;                         
    double  raise, ConvFact, NumNodes;  
    double  *OldRhsOld, *OldCKTstate0; 
    int     iters;
    
    ckt->CKTmode = firstmode;
    if(!ckt->CKTnoOpIter) {
        converged = NIiter(ckt,iterlim);
    } else {
        converged = 1;  /* the 'go directly to gmin stepping' option */
    }
    if(converged != 0) {
        /* no convergence on the first try, so we do something else */
        /* first, check if we should try gmin stepping */
        /* note that no path out of this code allows ckt->CKTdiagGmin to be 
         * anything but 0.000000000
         */
    
        if(ckt->CKTnumGminSteps ==1) {

            double OldGmin, gtarget, factor;
            int    success, failed;

            ckt->CKTmode = firstmode;
            (*(SPfrontEnd->IFerror))(ERR_INFO,
                    "trying dynamic Gmin stepping",(IFuid *)NULL);
            NumNodes=0;
            for (n = ckt->CKTnodes; n; n = n->next) {
                 NumNodes++;
            };
            OldRhsOld=(double *)MALLOC((NumNodes+1)*sizeof(double));
            OldCKTstate0=(double *)
                              MALLOC((ckt->CKTnumStates+1)*sizeof(double));
            for (n = ckt->CKTnodes; n; n = n->next) {
              *(ckt->CKTrhsOld+n->number)=0;
            };
            for(i=0;i<ckt->CKTnumStates;i++) {
                *(ckt->CKTstate0+i) = 0;
            };
            factor = ckt->CKTgminFactor;
            OldGmin = 1e-2;
            ckt->CKTdiagGmin = OldGmin / factor;
            gtarget = MAX(ckt->CKTgmin,ckt->CKTgshunt);
            success = failed = 0; 
            
            while ( (!success) && (!failed) ) {
                fprintf(stderr, "\rTrying gmin = %12.4E ", ckt->CKTdiagGmin);
                ckt->CKTnoncon =1;
                iters = ckt->CKTstat->STATnumIter;
                converged = NIiter(ckt,ckt->CKTdcTrcvMaxIter);
                iters = (ckt->CKTstat->STATnumIter)-iters;
                if(converged == 0) {
                   ckt->CKTmode=continuemode;
                   (*(SPfrontEnd->IFerror))(ERR_INFO,
                           "One successful Gmin step",(IFuid *)NULL);
                   if (ckt->CKTdiagGmin <= gtarget) {
                        success = 1;
                   } else {
                     i=0;
                     for (n = ckt->CKTnodes; n; n = n->next) {
                       OldRhsOld[i]=*(ckt->CKTrhsOld+n->number);
                       i++;
                     };
                     for(i=0;i<ckt->CKTnumStates;i++) {
                         *(OldCKTstate0+i) = *(ckt->CKTstate0+i);
                     };
                     if (iters <= (ckt->CKTdcTrcvMaxIter/4)) {
                       factor *= sqrt(factor);
                       if (factor > ckt->CKTgminFactor)
                                                factor = ckt->CKTgminFactor;
                     };
                     if (iters > (3*ckt->CKTdcTrcvMaxIter/4)) {
                       factor = sqrt(factor);
                     };
                     OldGmin = ckt->CKTdiagGmin;
                     if ((ckt->CKTdiagGmin) < (factor*gtarget)) {
                         factor = ckt->CKTdiagGmin / gtarget;
                         ckt->CKTdiagGmin = gtarget;
                     } else {
                         ckt->CKTdiagGmin /= factor;
                     };
                   };
                } else {
                   if (factor < 1.00005) {
                     failed = 1;
                     (*(SPfrontEnd->IFerror))(ERR_WARNING,
                            "last gmin step failed",(IFuid *)NULL);
                   } else {
                     factor=sqrt(sqrt(factor));
                     ckt->CKTdiagGmin = OldGmin / factor;
                     i=0;
                     for (n = ckt->CKTnodes; n; n = n->next) {
                       *(ckt->CKTrhsOld+n->number)=OldRhsOld[i];
                       i++;
                     };
                     for(i=0;i<ckt->CKTnumStates;i++) {
                         *(ckt->CKTstate0+i) = *(OldCKTstate0+i);
                     };
                   };
                }    
            }
            ckt->CKTdiagGmin=ckt->CKTgshunt;
            FREE(OldRhsOld);
            FREE(OldCKTstate0);
              converged = NIiter(ckt,iterlim);
            if (converged!=0) {
                    (*(SPfrontEnd->IFerror))(ERR_WARNING,
                            "dynamic gmin stepping failed",(IFuid *)NULL);
            } else {
              (*(SPfrontEnd->IFerror))(ERR_INFO,
                      "Dynamic gmin stepping completed",(IFuid *)NULL);
              return(0);
            };
            
            } else if(ckt->CKTnumGminSteps >1) {
            
            ckt->CKTmode = firstmode;
            (*(SPfrontEnd->IFerror))(ERR_INFO,
                    "starting Gmin stepping",(IFuid *)NULL);
            
            if (ckt->CKTgshunt==0) {                  
                 ckt->CKTdiagGmin = ckt->CKTgmin;    
            } else {                                 
                 ckt->CKTdiagGmin = ckt->CKTgshunt;  
            };
                    
      
            for(i=0;i<ckt->CKTnumGminSteps;i++) {
            	ckt->CKTdiagGmin *= ckt->CKTgminFactor;
            	
             
            }
            for(i=0;i<=ckt->CKTnumGminSteps;i++) {
             	 fprintf(stderr, "Trying gmin = %12.4E ", ckt->CKTdiagGmin);
                ckt->CKTnoncon =1;
                converged = NIiter(ckt,ckt->CKTdcTrcvMaxIter);
                if(converged != 0) {
                	ckt->CKTdiagGmin = ckt->CKTgshunt;
                   
                    (*(SPfrontEnd->IFerror))(ERR_WARNING,
                            "Gmin step failed",(IFuid *)NULL);
                    break;
                }
                ckt->CKTdiagGmin /= ckt->CKTgminFactor;
                ckt->CKTmode=continuemode;
                (*(SPfrontEnd->IFerror))(ERR_INFO,
                        "One successful Gmin step",(IFuid *)NULL);
            }
             ckt->CKTdiagGmin = ckt->CKTgshunt; 
            converged = NIiter(ckt,iterlim);
            if(converged == 0) {
                (*(SPfrontEnd->IFerror))(ERR_INFO,
                        "Gmin stepping completed",(IFuid *)NULL);
                return(0);
            }
            (*(SPfrontEnd->IFerror))(ERR_WARNING,
                    "Gmin stepping failed",(IFuid *)NULL);

        }
        /* now, we'll try source stepping - we scale the sources
         * to 0, converge, then start stepping them up until they
         * are at their normal values
	 *
         * note that no path out of this code allows ckt->CKTsrcFact to be 
         * anything but 1.000000000
         */
   
              
        if(ckt->CKTnumSrcSteps >=1) {
            ckt->CKTmode = firstmode;
            (*(SPfrontEnd->IFerror))(ERR_INFO,
                    "starting source stepping",(IFuid *)NULL);
            if(ckt->CKTnumSrcSteps==1) {
                ckt->CKTsrcFact=0; raise=0.001; ConvFact=0;
                NumNodes=0;
                for (n = ckt->CKTnodes; n; n = n->next) {
                     NumNodes++;
                };
                OldRhsOld=(double *)MALLOC((NumNodes+1)*sizeof(double));
                OldCKTstate0=(double *)
                                  MALLOC((ckt->CKTnumStates+1)*sizeof(double));
                for (n = ckt->CKTnodes; n; n = n->next) {
                  *(ckt->CKTrhsOld+n->number)=0;
                };
                for(i=0;i<ckt->CKTnumStates;i++) {
                    *(ckt->CKTstate0+i) = 0;
                };

/*  First, try a straight solution with all sources at zero */

                fprintf(stderr, "\rSupplies reduced to %8.4f%% ",
                                                         ckt->CKTsrcFact*100);
                converged = NIiter(ckt,ckt->CKTdcTrcvMaxIter);

/*  If this doesn't work, try gmin stepping as well for the first solution */

                if(converged != 0) {
                    fprintf(stderr, "\n");
                    if (ckt->CKTgshunt<=0) {
                         ckt->CKTdiagGmin = ckt->CKTgmin;
                    } else {
                         ckt->CKTdiagGmin = ckt->CKTgshunt;
                    };
                    for(i=0;i<10;i++) {
                        ckt->CKTdiagGmin *= 10;
                    }
                    for(i=0;i<=10;i++) {
                        fprintf(stderr, "Trying gmin = %12.4E ",
                                                            ckt->CKTdiagGmin);
                        ckt->CKTnoncon =1;
                        converged = NIiter(ckt,ckt->CKTdcTrcvMaxIter);
                        if(converged != 0) {
                            ckt->CKTdiagGmin = ckt->CKTgshunt;
                            (*(SPfrontEnd->IFerror))(ERR_WARNING,
                                    "Gmin step failed",(IFuid *)NULL);
                            break;
                        }

                        ckt->CKTdiagGmin /= 10;
                        ckt->CKTmode=continuemode;
                        (*(SPfrontEnd->IFerror))(ERR_INFO,
                                "One successful Gmin step",(IFuid *)NULL);
                    }
                    ckt->CKTdiagGmin = ckt->CKTgshunt;
                };

/*  If we've got convergence, then try stepping up the sources  */

                if(converged == 0) {
                   i=0;
                   for (n = ckt->CKTnodes; n; n = n->next) {
                     OldRhsOld[i]=*(ckt->CKTrhsOld+n->number);
                     i++;
                   };
                   for(i=0;i<ckt->CKTnumStates;i++) {
                       *(OldCKTstate0+i) = *(ckt->CKTstate0+i);
                   };
                   (*(SPfrontEnd->IFerror))(ERR_INFO,
                               "One successful source step",(IFuid *)NULL);
                   ckt->CKTsrcFact=ConvFact+raise;
                };

                if(converged == 0) do {
                   fprintf(stderr, "\rSupplies reduced to %8.4f%% ",
                                                         ckt->CKTsrcFact*100);

                   iters = ckt->CKTstat->STATnumIter;
                   converged = NIiter(ckt,ckt->CKTdcTrcvMaxIter);
                   iters = (ckt->CKTstat->STATnumIter)-iters;

                   ckt->CKTmode = continuemode;
                   if (converged == 0) {
                      ConvFact=ckt->CKTsrcFact;
                      i=0;
                      for (n = ckt->CKTnodes; n; n = n->next) {
                        OldRhsOld[i]=*(ckt->CKTrhsOld+n->number);
                        i++;
                      };
                      for(i=0;i<ckt->CKTnumStates;i++) {
                          *(OldCKTstate0+i) = *(ckt->CKTstate0+i);
                      };
                      (*(SPfrontEnd->IFerror))(ERR_INFO,
                            "One successful source step",(IFuid *)NULL);
                      ckt->CKTsrcFact=ConvFact+raise;
                      if (iters <= (ckt->CKTdcTrcvMaxIter/4)) {
                        raise=raise*1.5;
                      };
                      if (iters > (3*ckt->CKTdcTrcvMaxIter/4)) {
                        raise=raise*0.5;
                      };
/*                    if (raise>0.01) raise=0.01; */
                   } else {
                      if ((ckt->CKTsrcFact-ConvFact)<1e-8) break;
                      raise=raise/10;
                      if (raise>0.01) raise=0.01;
                      ckt->CKTsrcFact=ConvFact;
                      i=0;
                      for (n = ckt->CKTnodes; n; n = n->next) {
                        *(ckt->CKTrhsOld+n->number)=OldRhsOld[i];
                        i++;
                      };
                      for(i=0;i<ckt->CKTnumStates;i++) {
                          *(ckt->CKTstate0+i) = *(OldCKTstate0+i);
                      };
                   };
                   if ((ckt->CKTsrcFact)>1) ckt->CKTsrcFact=1;
                } while ((raise>=1e-7) && (ConvFact<1));

                FREE(OldRhsOld);
                FREE(OldCKTstate0);
                ckt->CKTsrcFact = 1;
                if (ConvFact!=1) {
                        ckt->CKTsrcFact = 1;
                        ckt->CKTcurrentAnalysis = DOING_TRAN;
                        (*(SPfrontEnd->IFerror))(ERR_WARNING,
                                "source stepping failed",(IFuid *)NULL);
                        return(E_ITERLIM);
                } else {
                  (*(SPfrontEnd->IFerror))(ERR_INFO,
                          "Source stepping completed",(IFuid *)NULL);
                  return(0);
                };
            } else {
                for(i=0;i<=ckt->CKTnumSrcSteps;i++) {
                    ckt->CKTsrcFact = ((double)i)/((double)ckt->CKTnumSrcSteps);
                    converged = NIiter(ckt,ckt->CKTdcTrcvMaxIter);
                    ckt->CKTmode = continuemode;
                    if(converged != 0) {
                        ckt->CKTsrcFact = 1;
                        ckt->CKTcurrentAnalysis = DOING_TRAN;
                        (*(SPfrontEnd->IFerror))(ERR_WARNING,
                                "source stepping failed",(IFuid *)NULL);
                        return(converged);
                    }
                    (*(SPfrontEnd->IFerror))(ERR_INFO,
                        "One successful source step",(IFuid *)NULL);
                }
                (*(SPfrontEnd->IFerror))(ERR_INFO,
                        "Source stepping completed",(IFuid *)NULL);
                ckt->CKTsrcFact = 1;
                return(0);
            }; 
        } else {
            return(converged);
        }
    } 
    return(0);
}

/* CKTconvTest(ckt)
 *    this is a driver program to iterate through all the various
 *    convTest functions provided for the circuit elements in the
 *    given circuit 
 */

int
CKTconvTest(CKTcircuit *ckt)
{
    extern SPICEdev *DEVices[];
    int i;
    int error = OK;
#ifdef PARALLEL_ARCH
    int ibuf[2];
    long type = MT_CONV, length = 2;
#endif /* PARALLEL_ARCH */

    for (i=0;i<DEVmaxnum;i++) {
        if (((*DEVices[i]).DEVconvTest != NULL) && (ckt->CKThead[i] != NULL)) {
            error = (*((*DEVices[i]).DEVconvTest))(ckt->CKThead[i],ckt);
        }
#ifdef PARALLEL_ARCH
	if (error || ckt->CKTnoncon) goto combine;
#else
	if (error) return(error);
	if (ckt->CKTnoncon) {
            /* printf("convTest: device %s failed\n",
                    (*DEVices[i]).DEVpublic.name); */
            return(OK);
        }
#endif /* PARALLEL_ARCH */
    }
#ifdef PARALLEL_ARCH
combine:
    /* See if any of the DEVconvTest functions bailed. If not, proceed. */
    ibuf[0] = error;
    ibuf[1] = ckt->CKTnoncon;
    IGOP_( &type, ibuf, &length, "+" );
    ckt->CKTnoncon = ibuf[1];
    if ( ibuf[0] != error ) {
	error = E_MULTIERR;
    }
    return (error);
#else
    return(OK);
#endif /* PARALLEL_ARCH */
}
