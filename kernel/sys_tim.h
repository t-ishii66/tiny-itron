/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _SYS_TIM_H
#define _SYS_TIM_H

void tim_init(void);
ER sys_set_tim(W, SYSTIM*);
ER sys_get_tim(W, SYSTIM*);
ER sys_isig_tim(W);

#endif
