/*
 * This file is part of the UCB release of Plan 9. It is subject to the license
 * terms in the LICENSE file found in the top-level directory of this
 * distribution and at http://akaros.cs.berkeley.edu/files/Plan9License. No
 * part of the UCB release of Plan 9, including this file, may be copied,
 * modified, propagated, or distributed except according to the terms contained
 * in the LICENSE file.
 */

#include <u.h>
#include <libc.h>
#include <thread.h>
#include <keyboard.h>
#include <fcall.h>
#include "dat.h"
#include "fns.h"

static Channel*	ctimer;	/* chan(Timer*)[100] */
static Timer *timer;

static
uint
msec(void)
{
	print_func_entry();
	print_func_exit();
	return nsec()/1000000;
}

void
timerstop(Timer *t)
{
	print_func_entry();
	t->next = timer;
	timer = t;
	print_func_exit();
}

void
timercancel(Timer *t)
{
	print_func_entry();
	t->cancel = TRUE;
	print_func_exit();
}

static
void
timerproc(void* vacio)
{
	print_func_entry();
	int i, nt, na, dt, del;
	Timer **t, *x;
	uint old, new;

	rfork(RFFDG);
	threadsetname("TIMERPROC");
	t = nil;
	na = 0;
	nt = 0;
	old = msec();
	for(;;){
		sleep(1);	/* will sleep minimum incr */
		new = msec();
		dt = new-old;
		old = new;
		if(dt < 0)	/* timer wrapped; go around, losing a tick */
			continue;
		for(i=0; i<nt; i++){
			x = t[i];
			x->dt -= dt;
			del = 0;
			if(x->cancel){
				timerstop(x);
				del = 1;
			}else if(x->dt <= 0){
				/*
				 * avoid possible deadlock if client is
				 * now sending on ctimer
				 */
				if(nbsendul(x->c, 0) > 0)
					del = 1;
			}
			if(del){
				memmove(&t[i], &t[i+1], (nt-i-1)*sizeof t[0]);
				--nt;
				--i;
			}
		}
		if(nt == 0){
			x = recvp(ctimer);
	gotit:
			if(nt == na){
				na += 10;
				t = realloc(t, na*sizeof(Timer*));
				if(t == nil)
					abort();
			}
			t[nt++] = x;
			old = msec();
		}
		if(nbrecv(ctimer, &x) > 0)
			goto gotit;
	}
	print_func_exit();
}

void
timerinit(void)
{
	print_func_entry();
	ctimer = chancreate(sizeof(Timer*), 100);
	proccreate(timerproc, nil, STACK);
	print_func_exit();
}

/*
 * timeralloc() and timerfree() don't lock, so can only be
 * called from the main proc.
 */

Timer*
timerstart(int dt)
{
	print_func_entry();
	Timer *t;

	t = timer;
	if(t)
		timer = timer->next;
	else{
		t = emalloc(sizeof(Timer));
		t->c = chancreate(sizeof(int), 0);
	}
	t->next = nil;
	t->dt = dt;
	t->cancel = FALSE;
	sendp(ctimer, t);
	print_func_exit();
	return t;
}
