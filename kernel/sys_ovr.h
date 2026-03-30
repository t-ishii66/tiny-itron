/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_OVR_H
#define _SYS_OVR_H

void ovr_init(void);
ER sys_def_ovr(W, T_DOVR*);
ER sys_sta_ovr(W, ID, OVRTIM);
ER sys_stp_ovr(W, ID);
ER sys_ref_ovr(W, ID, T_ROVR*);

#endif
