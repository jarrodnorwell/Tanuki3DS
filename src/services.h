#ifndef SERVICES_H
#define SERVICES_H

#include "services/am.h"
#include "services/apt.h"
#include "services/cfg.h"
#include "services/dsp.h"
#include "services/fs.h"
#include "services/gsp.h"
#include "services/hid.h"
#include "services/ldr.h"
#include "services/ndm.h"
#include "services/news.h"
#include "services/nim.h"
#include "services/ptm.h"
#include "services/y2r.h"

#include "thread.h"

typedef struct {

    KSemaphore notif_sem;

    APTData apt;
    GSPData gsp;
    HIDData hid;
    DSPData dsp;
    FSData fs;

} ServiceData;

#endif