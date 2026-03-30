/* Copyright (c) 2000 by t-ishii66. All rights reserved. ---------------*/
#ifndef _KEYBOARD_H
#define _KEYBOARD_H

void key_init(void);
int key_intr(void);
void key_start(void);

extern int key_dtq_id;

#endif
