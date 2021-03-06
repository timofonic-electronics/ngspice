/**********
Copyright 1990 Regents of the University of California.  All rights reserved.
Author: 1985 Thomas L. Quarles
**********/

#include "ngspice/ngspice.h"
#include <stdio.h>
#include "ngspice/ifsim.h"
#include "ngspice/iferrmsg.h"
#include "ngspice/sen2defs.h"
#include "ngspice/cktdefs.h"
#include "ngspice/suffix.h"


int
SENaskQuest(CKTcircuit *ckt, JOB *anal, int which, IFvalue *value)
{
    NG_IGNORE(ckt);
    NG_IGNORE(anal);
    NG_IGNORE(value);

    switch(which) {

    default:
        break;
    }

    return E_BADPARM;
}
