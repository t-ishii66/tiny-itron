/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _CONFIG_H
#define _CONFIG_H

/* priority -----------------------------------------------------------------*/
#define TMIN_TPRI	1	/* minimum number of task priority */
#define TMAX_TPRI	16	/* maximum number of task priority */

#define TMIN_MPRI	1	/* minimum number of message priority */
#define TMAX_MPRI	16	/* maximum number of message priority */

/* kernel information -------------------------------------------------------*/
#define TKERNEL_MAKER	0x0000	/* maker code (experiment system) */
#define TKERNEL_PRID	0x0001	/* (?) */
#define TKERNEL_SPVER	0x5400	/* Micro ITRON ver4.0.0 */
#define TKERNEL_PRVER	0x0000	/* kernel version 0.0.0 */

/* maximum number of queueing/nest ------------------------------------------*/
#define TMAX_ACTCNT	16
#define TMAX_WUPCNT	16
#define TMAX_SUSCNT	16

/* bit number of bit pattern ------------------------------------------------*/
#define TBIT_TEXPTN	32	/* (?) */
#define TBIT_FLGPTN	32	/* (?) */
#define TBIT_RDVPTN	32	/* (?) */

/* time tick ----------------------------------------------------------------*/
#define TIC_NUME	17	/* ~17ms (PIT: 1193182 Hz / 60 Hz ≈ 16.7ms) */
#define TIC_DENO	1000	/* return (TIC_NUME/TIC_DENO) */


#define TMAX_MAXSEM	65535
/* none itron specification =================================================*/
#define MAX_TSKID	16
#define MAX_SEMID	16
#define MAX_FLGID	16
#define MAX_MBXID	16
#define MAX_MPFID	16
#define MAX_MBFID	16
#define MAX_PORID	16
#define MAX_DTQID	16
#define MAX_MTXID	16
#define MAX_MPLID	16
#define MAX_CYCID	16
#define MAX_ALMID	16
#define MAX_INHID	16
#define MAX_EXCID	16
#define MAX_SVCID	16

#define MAX_ISRID	15	/* 0 - 15 */
#define MAX_FNCD	16
#define MAX_EXC		15	/* 0 - 15 */

#endif
