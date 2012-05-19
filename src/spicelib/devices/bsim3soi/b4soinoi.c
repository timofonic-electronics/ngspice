/***  B4SOI 12/16/2010 Released by Tanvir Morshed  ***/


/**********
 * Copyright 2010 Regents of the University of California.  All rights reserved.
 * Authors: 1998 Samuel Fung, Dennis Sinitsky and Stephen Tang
 * Authors: 1999-2004 Pin Su, Hui Wan, Wei Jin, b3soinoi.c
 * Authors: 2005- Hui Wan, Xuemei Xi, Ali Niknejad, Chenming Hu.
 * Authors: 2009- Wenwei Yang, Chung-Hsun Lin, Ali Niknejad, Chenming Hu.
 * File: b4soinoi.c
 * Modified by Hui Wan, Xuemei Xi 11/30/2005
 * Modified by Wenwei Yang, Chung-Hsun Lin, Darsen Lu 03/06/2009
 * Modified by Tanvir Morshed 09/22/2009
 * Modified by Tanvir Morshed 12/31/2009
 **********/

#include "ngspice/ngspice.h"
#include "b4soidef.h"
#include "ngspice/cktdefs.h"
#include "ngspice/iferrmsg.h"
#include "ngspice/noisedef.h"
#include "ngspice/suffix.h"
#include "ngspice/const.h"  /* jwan */

/*
 * B4SOInoise (mode, operation, firstModel, ckt, data, OnDens)
 *    This routine names and evaluates all of the noise sources
 *    associated with MOSFET's.  It starts with the model *firstModel and
 *    traverses all of its insts.  It then proceeds to any other models
 *    on the linked list.  The total output noise density generated by
 *    all of the MOSFET's is summed with the variable "OnDens".
 */

/*
 Channel thermal and flicker noises are calculated based on the value
 of model->B4SOItnoiMod and model->B4SOIfnoiMod
 If model->B4SOItnoiMod = 0,
    Channel thermal noise = Charge based model
 If model->B4SOItnoiMod = 1,
    Channel thermal noise = Holistic noise model
 If model->B4SOItnoiMod = 2,
    Channel thermal noise = SPICE2 model
 If model->B4SOIfnoiMod = 0,
    Flicker noise         = Simple model
 If model->B4SOIfnoiMod = 1,
    Flicker noise         = Unified model
*/

static double
B4SOIEval1ovFNoise(
double vds,
B4SOImodel *model,
B4SOIinstance *here,
double freq, double temp)
{
struct b4soiSizeDependParam *pParam;
double cd, esat, DelClm, EffFreq, N0, Nl;
double T0, T1, T2, T3, T4, T5, T6, T7, T8, T9, Ssi;

    pParam = here->pParam;
    cd = fabs(here->B4SOIcd);
    esat = 2.0 * here->B4SOIvsattemp / here->B4SOIueff;
/* v2.2.3 bug fix */
    if(model->B4SOIem<=0.0) DelClm = 0.0;
    else {
            T0 = ((((vds - here->B4SOIVdseff) / pParam->B4SOIlitl)
                + model->B4SOIem) / esat);
            DelClm = pParam->B4SOIlitl * log (MAX(T0, N_MINLOG));
    }

    EffFreq = pow(freq, model->B4SOIef);
    T1 = CHARGE * CHARGE * CONSTboltz * cd * temp * here->B4SOIueff;
    T2 = 1.0e10 * EffFreq * here->B4SOIAbulk * model->B4SOIcox
       * pParam->B4SOIleff * pParam->B4SOIleff;

/* v2.2.3 bug fix */
    N0 = model->B4SOIcox * here->B4SOIVgsteff / CHARGE; 
    Nl = model->B4SOIcox * here->B4SOIVgsteff
         * (1.0 - here->B4SOIAbovVgst2Vtm * here->B4SOIVdseff) / CHARGE; 

    T3 = model->B4SOIoxideTrapDensityA
       * log(MAX(((N0 + here->B4SOInstar) / (Nl + here->B4SOInstar)), N_MINLOG));
    T4 = model->B4SOIoxideTrapDensityB * (N0 - Nl);
    T5 = model->B4SOIoxideTrapDensityC * 0.5 * (N0 * N0 - Nl * Nl);

    T6 = CONSTboltz * temp * cd * cd;
    T7 = 1.0e10 * EffFreq * pParam->B4SOIleff
       * pParam->B4SOIleff * pParam->B4SOIweff * here->B4SOInf;
    T8 = model->B4SOIoxideTrapDensityA + model->B4SOIoxideTrapDensityB * Nl
       + model->B4SOIoxideTrapDensityC * Nl * Nl;
    T9 = (Nl + here->B4SOInstar) * (Nl + here->B4SOInstar);

    Ssi = T1 / T2 * (T3 + T4 + T5) + T6 / T7 * DelClm * T8 / T9;

    return Ssi;
}

int
B4SOInoise (
int mode, int operation,
GENmodel *inModel,
CKTcircuit *ckt,
Ndata *data,
double *OnDens)
{
register B4SOImodel *model = (B4SOImodel *)inModel;
register B4SOIinstance *here;
struct b4soiSizeDependParam *pParam;
char name[N_MXVLNTH];
double tempOnoise;
double tempInoise;
double noizDens[B4SOINSRCS];
double lnNdens[B4SOINSRCS];

double vgs, vds;
double T0, T1, T2, T5, T10, T11;
double Ssi, Swi;

/* v3.2 */
double npart_theta, npart_beta, igsquare, esat;
/* v3.2 end */
double gspr, gdpr;
double tempRatioSH, Vdseffovcd; /* v4.2 bugfix */

int i;

double m;

    /* define the names of the noise sources */
    static char *B4SOInNames[B4SOINSRCS] =
    {   /* Note that we have to keep the order */
        ".rd",              /* noise due to rd */
                            /* consistent with the index definitions */
        ".rs",              /* noise due to rs */
                            /* in B4SOIdefs.h */
        ".rg",                     /* noise due to rgeltd, v3.2 */
        ".id",              /* noise due to id */
        ".1overf",          /* flicker (1/f) noise */
        ".fb_ibs",             /* noise due to floating body by ibs */
        ".fb_ibd",             /* noise due to floating body by ibd */
        ".igs",                    /* shot noise due to IGS, v3.2 */
        ".igd",             /* shot noise due to IGD, v3.2 */
        ".igb",                    /* shot noise due to IGB, v3.2 */
        ".rbsb",            /* noise due to rbsb  v4.0 */
        ".rbdb",            /* noise due to rbdb  v4.0 */
        ".rbody",           /* noise due to body contact  v4.0 */
                
        ""                  /* total transistor noise */
    };

    for (; model != NULL; model = model->B4SOInextModel)
    { for (here = model->B4SOIinstances; here != NULL;
            here = here->B4SOInextInstance)
            {    
              if (here->B4SOIowner != ARCHme) continue;

              m = here->B4SOIm;

              pParam = here->pParam;
              switch (operation)
              {  case N_OPEN:
                     /* see if we have to to produce a summary report */
                     /* if so, name all the noise generators */

                      if (((NOISEAN*)ckt->CKTcurJob)->NStpsSm != 0)
                      {   switch (mode)
                          {  case N_DENS:
                                  for (i = 0; i < B4SOINSRCS; i++)
                                  {    (void) sprintf(name, "onoise.%s%s",
                                                      here->B4SOIname,
                                                      B4SOInNames[i]);
                                       data->namelist = TREALLOC(IFuid, data->namelist, data->numPlots + 1);
                                       if (!data->namelist)
                                           return(E_NOMEM);
                                       (*(SPfrontEnd->IFnewUid)) (ckt,
                                          &(data->namelist[data->numPlots++]),
                                          (IFuid) NULL, name, UID_OTHER,
                                           NULL);
                                       /* we've added one more plot */
                                  }
                                  break;
                             case INT_NOIZ:
                                  for (i = 0; i < B4SOINSRCS; i++)
                                  {    (void) sprintf(name, "onoise_total.%s%s",
                                                      here->B4SOIname,
                                                      B4SOInNames[i]);
                                       data->namelist = TREALLOC(IFuid, data->namelist, data->numPlots + 1);
                                       if (!data->namelist)
                                           return(E_NOMEM);
                                       (*(SPfrontEnd->IFnewUid)) (ckt,
                                          &(data->namelist[data->numPlots++]),
                                          (IFuid) NULL, name, UID_OTHER,
                                           NULL);
                                       /* we've added one more plot */

                                       (void) sprintf(name, "inoise_total.%s%s",
                                                      here->B4SOIname,
                                                      B4SOInNames[i]);
                                       data->namelist = TREALLOC(IFuid, data->namelist, data->numPlots + 1);
                                       if (!data->namelist)
                                           return(E_NOMEM);
                                       (*(SPfrontEnd->IFnewUid)) (ckt,
                                          &(data->namelist[data->numPlots++]),
                                          (IFuid) NULL, name, UID_OTHER,
                                          NULL);
                                       /* we've added one more plot */
                                  }
                                  break;
                          }
                      }
                      break;
                 case N_CALC:
                      switch (mode)
                      {  case N_DENS:
                              /*v4.2 implementing SH temp */ 
                              if ((model->B4SOIshMod == 1) && (here->B4SOIrth0 != 0.0))  
                                  tempRatioSH = here->B4SOITempSH / ckt->CKTtemp;
                              else
                                  tempRatioSH = 1.0;
                                                          /*v4.2 implementing limit on Vdseffovcd*/
                                                          if (here->B4SOIcd != 0)
                                                          {
                                                              Vdseffovcd = here->B4SOIVdseff / here->B4SOIcd;
                                                              if (Vdseffovcd >= 1.0e9) Vdseffovcd = 1.0e9 ;
                                                          }
                                                          else
                                                              Vdseffovcd = 1.0e9;
                             /* if (model->B4SOItnoiMod == 0) *//* v4.0 */ /* v4.2 bugfix: consider tnoiMod = 2*/
                                        if (model->B4SOItnoiMod != 1)                                
                                                  {   if (model->B4SOIrdsMod == 0)
                                  {   gspr = here->B4SOIsourceConductance;
                                      gdpr = here->B4SOIdrainConductance;
                                  }
                                  else
                                  {   gspr = here->B4SOIgstot;
                                      gdpr = here->B4SOIgdtot;
                                  }
                              }
                              else
                              {  
                                                          esat = 2.0 * here->B4SOIvsattemp / here->B4SOIueff;
                                                          T5 = here->B4SOIVgsteff / esat
                                     / pParam->B4SOIleff;
                                  T5 *= T5;
                                  npart_beta = model->B4SOIrnoia * (1.0 +
                                                 T5 * model->B4SOItnoia * 
                                                 pParam->B4SOIleff);
                                  npart_theta = model->B4SOIrnoib * (1.0 +
                                                  T5 * model->B4SOItnoib *
                                                  pParam->B4SOIleff);

                                                  /* v4.2 bugfix: implement bugfix from bsim4.6.2 */
                                  if(npart_theta > 0.9)
                                     npart_theta = 0.9;
                                  if(npart_theta > 0.9 * npart_beta)
                                     npart_theta = 0.9 * npart_beta;

                                                  
                                  if (model->B4SOIrdsMod == 0)
                                  {   gspr = here->B4SOIsourceConductance;
                                      gdpr = here->B4SOIdrainConductance;
                                  }
                                  else
                                  {   gspr = here->B4SOIgstot;
                                      gdpr = here->B4SOIgdtot;
                                  }
                                  if ( (*(ckt->CKTstates[0] + here->B4SOIvds))
                                         >= 0.0 )
                                      gspr = gspr * (1.0 + npart_theta 
                                           * npart_theta * gspr
                                           / here->B4SOIidovVds);
                                  else
                                      gdpr = gdpr * (1.0 + npart_theta
                                           * npart_theta * gdpr
                                           / here->B4SOIidovVds);
                              }

                              NevalSrc(&noizDens[B4SOIRDNOIZ],
                                       &lnNdens[B4SOIRDNOIZ], ckt, THERMNOISE,
                                       here->B4SOIdNodePrime, here->B4SOIdNode,
                                       gdpr * tempRatioSH * m); /* v4.2 self-heating temp */

                              NevalSrc(&noizDens[B4SOIRSNOIZ],
                                       &lnNdens[B4SOIRSNOIZ], ckt, THERMNOISE,
                                       here->B4SOIsNodePrime, here->B4SOIsNode,
                                       gspr * tempRatioSH * m); /* v4.2 self-heating temp */

                                           /* v4.2 bugfix: implement correct thermal noise model (bsim4.6.0)*/
                            /*  if ((here->B4SOIrgateMod == 1) ||
                                  (here->B4SOIrgateMod == 2)) */ 
                                                                if (here->B4SOIrgateMod == 1)
                              {   NevalSrc(&noizDens[B4SOIRGNOIZ],
                                       &lnNdens[B4SOIRGNOIZ], ckt, THERMNOISE,
                                       here->B4SOIgNode,
                                       here->B4SOIgNodeExt,
                                       here->B4SOIgrgeltd * tempRatioSH * m); /* v4.2 self-heating temp */
                              }
                                                          else if (here->B4SOIrgateMod == 2)        /*v4.2*/
                              {
                                T0 = 1.0 + here->B4SOIgrgeltd/here->B4SOIgcrg;
                                T1 = T0 * T0;
                                  NevalSrc(&noizDens[B4SOIRGNOIZ],
                                       &lnNdens[B4SOIRGNOIZ], ckt, THERMNOISE,
                                       here->B4SOIgNode,
                                       here->B4SOIgNodeExt,
                                       here->B4SOIgrgeltd/T1 * tempRatioSH * m);                                /*v4.2*/
                              }
                              else if (here->B4SOIrgateMod == 3)
                              {   NevalSrc(&noizDens[B4SOIRGNOIZ],
                                       &lnNdens[B4SOIRGNOIZ], ckt, THERMNOISE,
                                       here->B4SOIgNodeMid,
                                       here->B4SOIgNodeExt,
                                       here->B4SOIgrgeltd * tempRatioSH * m); /* v4.2 self-heating temp */
                              }
                              else
                              {    noizDens[B4SOIRGNOIZ] = 0.0;
                                   lnNdens[B4SOIRGNOIZ] =
                                          log(MAX(noizDens[B4SOIRGNOIZ],
                                              N_MINLOG));
                              }

                              if (here->B4SOIrbodyMod)
                              {
                                  NevalSrc(&noizDens[B4SOIRBSBNOIZ],
                                      &lnNdens[B4SOIRBSBNOIZ], ckt, THERMNOISE,
                                      here->B4SOIbNode, here->B4SOIsbNode,
                                      here->B4SOIgrbsb * m);
                                 NevalSrc(&noizDens[B4SOIRBDBNOIZ],
                                      &lnNdens[B4SOIRBDBNOIZ], ckt, THERMNOISE,
                                      here->B4SOIbNode, here->B4SOIdbNode,
                                      here->B4SOIgrbdb * tempRatioSH * m); /* v4.2 self-heating temp */
                              }
                              else
                              {    noizDens[B4SOIRBSBNOIZ] = 0.0;
                                   noizDens[B4SOIRBDBNOIZ] = 0.0;
                                   lnNdens[B4SOIRBSBNOIZ] =
                                    log(MAX(noizDens[B4SOIRBSBNOIZ], N_MINLOG));
                                   lnNdens[B4SOIRBDBNOIZ] =
                                    log(MAX(noizDens[B4SOIRBDBNOIZ], N_MINLOG));
                              }

                              if (here->B4SOIbodyMod == 1)
                              {
                                  NevalSrc(&noizDens[B4SOIRBODYNOIZ],
                                      &lnNdens[B4SOIRBODYNOIZ], ckt, THERMNOISE,
                                      here->B4SOIbNode, here->B4SOIpNode,
                                      tempRatioSH / (here->B4SOIrbodyext + /* v4.2 self-heating temp */
                                       pParam->B4SOIrbody) * m);
                              }
                              else
                              {    noizDens[B4SOIRBODYNOIZ] = 0.0;
                                   lnNdens[B4SOIRBODYNOIZ] =
                                   log(MAX(noizDens[B4SOIRBODYNOIZ], N_MINLOG));
                              }
 
                              switch( model->B4SOItnoiMod )
                              {  
                                 case 0:
                                      NevalSrc(&noizDens[B4SOIIDNOIZ],
                                               &lnNdens[B4SOIIDNOIZ], ckt, 
                                               THERMNOISE,
                                               here->B4SOIdNodePrime,
                                               here->B4SOIsNodePrime,
                                               (here->B4SOIueff
                                               * fabs(here->B4SOIqinv
                                               / (pParam->B4SOIleff
                                               * pParam->B4SOIleff
                                               + here->B4SOIueff*fabs
                                                 (here->B4SOIqinv)
                                               *  here->B4SOIrds)))
                                               * tempRatioSH /* v4.2 self-heating temp */
                                               * model->B4SOIntnoi * m );
                                      break;

/* v2.2.3 bug fix */
                                 case 1:
                                      T0 = here->B4SOIgm + here->B4SOIgmbs +
                                           here->B4SOIgds;
                                      T0 *= T0;
                                      esat = 2.0 * here->B4SOIvsattemp /
                                             here->B4SOIueff;
                                      T5 = here->B4SOIVgsteff / esat /
                                           pParam->B4SOIleff;
                                      T5 *= T5;
                                      npart_beta = model->B4SOIrnoia * (1.0 +
                                                 T5 * model->B4SOItnoia * 
                                                 pParam->B4SOIleff);
                                      npart_theta = model->B4SOIrnoib * (1.0 +
                                                  T5 * model->B4SOItnoib *
                                                  pParam->B4SOIleff);
                                      /*igsquare = npart_theta * npart_theta *
                                                  T0 * here->B4SOIVdseff / here->B4SOIcd;        v4.2 implementing limit on Vdseffovcd*/
                                                 igsquare = npart_theta * npart_theta * T0 * Vdseffovcd;
                                      T1 = npart_beta * (here->B4SOIgm
                                         + here->B4SOIgmbs) + here->B4SOIgds;
                                      /*T2 = T1 * T1 * here->B4SOIVdseff / here->B4SOIcd; v4.2 implementing limit on Vdseffovcd*/
                                                                                T2 = T1 * T1 * Vdseffovcd;
                                      NevalSrc(&noizDens[B4SOIIDNOIZ],
                                               &lnNdens[B4SOIIDNOIZ], ckt,
                                               THERMNOISE,
                                               here->B4SOIdNodePrime,
                                               here->B4SOIsNodePrime, 
                                               /* (T2 - igsquare)); */
                                               (T2 - igsquare) * tempRatioSH * m); /* v4.2 self-heating temp */
                                      break;

                                 case 2:
                                      NevalSrc(&noizDens[B4SOIIDNOIZ],
                                               &lnNdens[B4SOIIDNOIZ], ckt,
                                               THERMNOISE,
                                               here->B4SOIdNodePrime,
                                               here->B4SOIsNodePrime,
                                               model->B4SOIntnoi *
                                               tempRatioSH * /* v4.2 self-heating temp */
                                               (2.0 / 3.0 * fabs(here->B4SOIgm
                                               + here->B4SOIgds
                                               + here->B4SOIgmbs)) * m);
                                      break;
                              }

                              NevalSrc(&noizDens[B4SOIFLNOIZ], (double*) NULL,
                                       ckt, N_GAIN, here->B4SOIdNodePrime,
                                       here->B4SOIsNodePrime, (double) 0.0);

                              switch( model->B4SOIfnoiMod )
                              {  case 0:
                                   if (model->B4SOIw0flk > 0) { /* v4.0 */
                                      noizDens[B4SOIFLNOIZ] *= here->B4SOInf
                                      * pParam->B4SOIweff/model->B4SOIw0flk 
                                      * model->B4SOIkf * exp(model->B4SOIaf
                                      * log(MAX(fabs(here->B4SOIcd
                                      / pParam->B4SOIweff / here->B4SOInf
                                      * model->B4SOIw0flk), N_MINLOG)))
                                      / (pow(data->freq, model->B4SOIef)
                                      * pow(pParam->B4SOIleff, 
                                        model->B4SOIbf) * model->B4SOIcox);
                                      break;
                                   }
                                  else {
                                      noizDens[B4SOIFLNOIZ] *=
                                      model->B4SOIkf * exp(model->B4SOIaf
                                      * log(MAX(fabs(here->B4SOIcd), N_MINLOG)))
                                      / (pow(data->freq, model->B4SOIef)
                                      * pow(pParam->B4SOIleff,
                                      model->B4SOIbf) * model->B4SOIcox);
                                      break;
                                  }
                                 case 1:
                                      vgs = *(ckt->CKTstates[0] + here->B4SOIvgs);
                                      vds = *(ckt->CKTstates[0] + here->B4SOIvds);
                                      if (vds < 0.0)
                                      {   vds = -vds;
                                          vgs = vgs + vds;
                                      }
                                                 /*v4.2 implementing SH temp */ 
                                                if ((model->B4SOIshMod == 1) && (here->B4SOIrth0 != 0.0))  
                                                  Ssi = B4SOIEval1ovFNoise(vds, model, here,
                                            data->freq, here->B4SOITempSH);
                                    else 
                                                  Ssi = B4SOIEval1ovFNoise(vds, model, here,
                                            data->freq, ckt->CKTtemp); /*v4.2 implementing SH temp */
                                     
                                                 /*v4.2 implementing SH temp */
                                                if ((model->B4SOIshMod == 1) && (here->B4SOIrth0 != 0.0))  
                                                T10 = model->B4SOIoxideTrapDensityA
                                            * CONSTboltz * here->B4SOITempSH;
                                                else
                                                T10 = model->B4SOIoxideTrapDensityA
                                            * CONSTboltz * ckt->CKTtemp; /*v4.2 implementing SH temp */
                                     
                                         T11 = pParam->B4SOIweff * here->B4SOInf 
                                            * pParam->B4SOIleff
                                            * pow(data->freq, model->B4SOIef)
                                            * 1.0e10 * here->B4SOInstar
                                            * here->B4SOInstar ;
                                      Swi = T10 / T11 * here->B4SOIcd
                                            * here->B4SOIcd;
                                      T1 = Swi + Ssi;
                                      if (T1 > 0.0)
                                           noizDens[B4SOIFLNOIZ] *= (Ssi
                                                                  * Swi) / T1;
                                      else
                                           noizDens[B4SOIFLNOIZ] *= 0.0;
                                      break;
                              }

                              lnNdens[B4SOIFLNOIZ] =
                                     log(MAX(noizDens[B4SOIFLNOIZ], N_MINLOG));

                              /* v3.2 for gate tunneling shot noise */
                              NevalSrc(&noizDens[B4SOIIGSNOIZ],
                                   &lnNdens[B4SOIIGSNOIZ], ckt, SHOTNOISE,
                                   here->B4SOIgNode, here->B4SOIsNodePrime,
                                   (here->B4SOIIgs + here->B4SOIIgcs) * m);

                              NevalSrc(&noizDens[B4SOIIGDNOIZ],
                                   &lnNdens[B4SOIIGDNOIZ], ckt, SHOTNOISE,
                                   here->B4SOIgNode, here->B4SOIdNodePrime,
                                   (here->B4SOIIgd + here->B4SOIIgcd) * m);

                              NevalSrc(&noizDens[B4SOIIGBNOIZ],
                                   &lnNdens[B4SOIIGBNOIZ], ckt, SHOTNOISE,
                                   here->B4SOIgNode, here->B4SOIbNode,
                                   here->B4SOIig * m);
                              /* v3.2 for gate tunneling shot noise end */

                              /* Low frequency excess noise due to FBE */
/*                              NevalSrc(&noizDens[B4SOIFBNOIZ], 
                              &lnNdens[B4SOIFBNOIZ], ckt, SHOTNOISE, 
                              here->B4SOIsNodePrime, here->B4SOIbNode, 
                              2.0 * model->B4SOInoif * here->B4SOIibs); */
/* v4.0 */
                              NevalSrc(&noizDens[B4SOIFB_IBSNOIZ], 
                              &lnNdens[B4SOIFB_IBSNOIZ], ckt, SHOTNOISE, 
                              here->B4SOIsNodePrime, here->B4SOIbNode, 
                              model->B4SOInoif * here->B4SOIibs * m); 

                             /* NevalSrc(&noizDens[B4SOIFB_IBDNOIZ], 
                              &lnNdens[B4SOIFB_IBDNOIZ], ckt, SHOTNOISE, 
                              here->B4SOIdNodePrime, here->B4SOIbNode, 
                              model->B4SOInoif * fabs(here->B4SOIibd)); */ /*v4.2*/

                              NevalSrc(&noizDens[B4SOIFB_IBDNOIZ], 
                              &lnNdens[B4SOIFB_IBDNOIZ], ckt, SHOTNOISE, 
                              here->B4SOIdNodePrime, here->B4SOIbNode, 
                              model->B4SOInoif * (here->B4SOIibd) * m);         /*v4.2 extra fabs()removed */                                                  
                          
                                   noizDens[B4SOITOTNOIZ] = noizDens[B4SOIRDNOIZ]
                                                     + noizDens[B4SOIRSNOIZ]
                                                     + noizDens[B4SOIRGNOIZ]
                                                     + noizDens[B4SOIIDNOIZ]
                                                     + noizDens[B4SOIFLNOIZ]
                                                  /* + noizDens[B4SOIFBNOIZ] */
                                                     + noizDens[B4SOIFB_IBSNOIZ]
                                                     + noizDens[B4SOIFB_IBDNOIZ]
                                                     + noizDens[B4SOIIGSNOIZ]
                                                     + noizDens[B4SOIIGDNOIZ]
                                                     + noizDens[B4SOIIGBNOIZ]
                                                     + noizDens[B4SOIRBSBNOIZ]
                                                     + noizDens[B4SOIRBDBNOIZ]
                                                     + noizDens[B4SOIRBODYNOIZ];
                              lnNdens[B4SOITOTNOIZ] = 
                                     log(MAX(noizDens[B4SOITOTNOIZ], N_MINLOG));

                              *OnDens += noizDens[B4SOITOTNOIZ];

                              if (data->delFreq == 0.0)
                              {   /* if we haven't done any previous 
                                     integration, we need to initialize our
                                     "history" variables.
                                    */

                                  for (i = 0; i < B4SOINSRCS; i++)
                                  {    here->B4SOInVar[LNLSTDENS][i] =
                                             lnNdens[i];
                                  }

                                  /* clear out our integration variables
                                     if it's the first pass
                                   */
                                  if (data->freq ==
                                      ((NOISEAN*) ckt->CKTcurJob)->NstartFreq)
                                  {   for (i = 0; i < B4SOINSRCS; i++)
                                      {    here->B4SOInVar[OUTNOIZ][i] = 0.0;
                                           here->B4SOInVar[INNOIZ][i] = 0.0;
                                      }
                                  }
                              }
                              else
                              {   /* data->delFreq != 0.0,
                                     we have to integrate.
                                   */
                                  for (i = 0; i < B4SOINSRCS; i++)
                                  {    if (i != B4SOITOTNOIZ)
                                       {   tempOnoise = Nintegrate(noizDens[i],
                                                lnNdens[i],
                                                here->B4SOInVar[LNLSTDENS][i],
                                                data);
                                           tempInoise = Nintegrate(noizDens[i]
                                                * data->GainSqInv, lnNdens[i]
                                                + data->lnGainInv,
                                                here->B4SOInVar[LNLSTDENS][i]
                                                + data->lnGainInv, data);
                                           here->B4SOInVar[LNLSTDENS][i] =
                                                lnNdens[i];
                                           data->outNoiz += tempOnoise;
                                           data->inNoise += tempInoise;
                                           if (((NOISEAN*)
                                               ckt->CKTcurJob)->NStpsSm != 0)
                                           {   here->B4SOInVar[OUTNOIZ][i]
                                                     += tempOnoise;
                                               here->B4SOInVar[OUTNOIZ][B4SOITOTNOIZ]
                                                     += tempOnoise;
                                               here->B4SOInVar[INNOIZ][i]
                                                     += tempInoise;
                                               here->B4SOInVar[INNOIZ][B4SOITOTNOIZ]
                                                     += tempInoise;
                                           }
                                       }
                                  }
                              }
                              if (data->prtSummary)
                              {   for (i = 0; i < B4SOINSRCS; i++)
                                  {    /* print a summary report */
                                       data->outpVector[data->outNumber++]
                                             = noizDens[i];
                                  }
                              }
                              break;
                         case INT_NOIZ:
                              /* already calculated, just output */
                              if (((NOISEAN*)ckt->CKTcurJob)->NStpsSm != 0)
                              {   for (i = 0; i < B4SOINSRCS; i++)
                                  {    data->outpVector[data->outNumber++]
                                             = here->B4SOInVar[OUTNOIZ][i];
                                       data->outpVector[data->outNumber++]
                                             = here->B4SOInVar[INNOIZ][i];
                                  }
                              }
                              break;
                      }
                      break;
                 case N_CLOSE:
                      /* do nothing, the main calling routine will close */
                      return (OK);
                      break;   /* the plots */
              }       /* switch (operation) */
         }    /* for here */
    }    /* for model */

    return(OK);
}



