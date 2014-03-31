#ifndef _H8300_SWITCH_TO_H
#define _H8300_SWITCH_TO_H


asmlinkage void resume(void);
#define switch_to(prev,next,last) {                         \
  void *_last;						    \
  __asm__ __volatile__(					    \
  			"mov.l	%1, er0\n\t"		    \
			"mov.l	%2, er1\n\t"		    \
                        "mov.l  %3, er2\n\t"                \
			"jsr @_resume\n\t"                  \
                        "mov.l  er2,%0\n\t"                 \
		       : "=r" (_last)			    \
		       : "r" (&(prev->thread)),		    \
			 "r" (&(next->thread)),		    \
                         "g" (prev)                         \
		       : "cc", "er0", "er1", "er2", "er3"); \
  (last) = _last; 					    \
}

#endif 
