#include <stdint.h>
#include "miros.h"
#include "csp.h"
#include "qassert.h"

Q_DEFINE_THIS_FILE

OSThread * volatile OS_curr; /* pointer to the current thread */
OSThread * volatile OS_next; /* pointer to the next thread */

OSThread * OS_thread[32 + 1]; /* array of threads started so far */
uint32_t OS_readySet; /* bitmask of threads that are ready*/
uint32_t OS_delayedSet; /* bitmask of threads that are being delayed */

/* Compiler dependent */
#define LOG2(x) (32 - __clz(x))


		/* Idle Thread Definition for CPU Idle or standby condition */
OSThread idleThread;
// uint32_t stack_idleThread[40] __attribute__ ((aligned (8)));
// uint32_t *sp_idleThread = &stack_idleThread[40];

void main_idleThread() {
    while (1) {
			OS_onIdle();
		}
}




void OS_init(void * stkSto, uint32_t stkSize){
		/* set the PendSV interrupt priority to the lowest level */
		*(uint32_t volatile *)0xE000ED20U |= (0xFFU << 16);
	 /* Start the idle thread */
	OSThread_start(&idleThread,
									0U, /* Idle Thread priority */
								 &main_idleThread,
								 stkSto, stkSize);
}



void OS_sched(void) {
		//_disable_irq();
		/* OS_next = ...*/
	if(OS_readySet == 0U){ /* idle condition? */
		OS_next = OS_thread[0]; /* the idle thread*/
	}
	else {
			OS_next = OS_thread[LOG2(OS_readySet)];
			Q_ASSERT(OS_next != (OSThread*)0);
	}
	/* trigger PendSV, if needed */
		if(OS_next != OS_curr) {
			*(uint32_t volatile *)0xE000ED04U = (1U << 28);
		}
		//__enable_irq();
}

void OS_run(void) {
	/* callback to configure and start interrupts */
	OS_onStartup();
	
	__disable_irq();
	OS_sched();
	__enable_irq();
	
	/* the following code should never execute */
	Q_ERROR();
}

void OS_tick(void){
		uint32_t workingSet = OS_delayedSet;
		while (workingSet != 0U){
				OSThread *t = OS_thread[LOG2(workingSet)];
			uint32_t bit;
				Q_ASSERT((t != (OSThread *)0U) && (t->timeout != 0U));
			
				bit = (1U << (t->prio - 1U));
				--t->timeout;
				if(t->timeout == 0U){
						OS_readySet |=bit; /* insert to Ready Set */
						OS_delayedSet &= ~bit; /* Remove from Delayed Set */
				}
				workingSet &= ~bit; /* remove from workingSet */
		}
}

void OS_delay(uint32_t ticks) {
	uint32_t bit;
  __disable_irq();
	
	/* never call OS_delay from the idleThread */
	Q_REQUIRE(OS_curr != OS_thread[0]);
	
	OS_curr->timeout = ticks;
	bit = (1U << (OS_curr->prio - 1U));
	OS_readySet &= ~bit;
	OS_delayedSet |= bit;
	OS_sched();
	__enable_irq();
}



void OSThread_start(
	OSThread * me,
	uint8_t prio, /* thread priority */
	OSThreadHandler threadHandler,
	void * stkSto,
	uint32_t stkSize) 
	{
		uint32_t * sp = (uint32_t *)((((uint32_t)stkSto + stkSize) / 8) * 8);
		uint32_t * stk_limit;
		
		
		Q_REQUIRE((prio < Q_DIM(OS_thread)) && (OS_thread[prio] == (OSThread *)0));
		
	*(--sp) = (1U << 24); /* xPSR - Programme Status Word Register */
	*(--sp) = (uint32_t)threadHandler; /* PC */
	*(--sp) = 0x0000000EU; /* LR */
	*(--sp) = 0x0000000CU; /* R12 */
	*(--sp) =  0x0000003U; /* R3 */
	*(--sp) =  0x0000002U; /* R2 */
	*(--sp) =  0x0000001U; /* R1 */
	*(--sp) =  0x0000000U; /* R0 */
		/* additionally, fake registers R4-R11 */
		*(--sp) =  0x000000BU;
		*(--sp) =  0x000000AU;
		*(--sp) =  0x0000009U;
		*(--sp) =  0x0000008U;
		*(--sp) =  0x0000007U;
		*(--sp) =  0x0000006U;
		*(--sp) =  0x0000005U;
		*(--sp) =  0x0000004U;
		/* save the top of the stack in the thread's attribute */
		me->sp = sp;
		
		/* round up the bottom of the stack to the 8-byte boundary */
		stk_limit = (uint32_t *)(((((uint32_t)stkSto - 1U) / 8)+ 1U) * 8);
		/* pre-fill the unused part of the stack with 0xDEADBEEFU */
		for(sp-= 1U;  sp >= stk_limit; --sp) {
			*sp = 0xDEADBEEFU;
			}
		
			
			/* register thread with the OS */
			OS_thread[prio] = me;
			me->prio = prio;
			/* make the thread ready to run */
			if (prio > 0U) {
				OS_readySet |= (1U << (prio - 1U)); /* */
			}
		}
	
_ASM_
		void PendSV_Handler(void) { // automating context switching
		//	void * sp;
			
		//	__disable_irq(); // disabling interrupts before making context switching
			
		//	if(OS_curr != (OSThread *)0U) {
				/* push registers r4-r12 in the thread stack frame */
		//		OS_curr->sp = sp;
		//	}
		//					
		//		sp = OS_next->sp;
		//		OS_curr = OS_next;
			/* pop registers r4-r11 from the thread stack frame */
		//	__enable_irq();
			
			IMPORT OS_curr /* extern variable */
			IMPORT OS_next /* extern variable */
			
			/* Disable interrupt requests */
			CPSID	I
			
			/* if OS_curr != (OSThread*)0U */
			LDR	r1,=OS_curr
			LDR r1, [r1,#0x00]
			CBZ	r1,PendSV_restore
			
			/* Push registers r4-r11 */
			PUSH	{r4-r11}
			LDR	r1,=OS_curr
			LDR	r1,[r1,#0x00]
			
			/* OS_curr->sp = sp */
			STR	sp,[r1,#00]
			
PendSV_restore // label
			
			/* sp = OS_next->sp */
			LDR	r1,=OS_next
			LDR	r1,[r1,#0x00]
			LDR	sp,[r1,#0x00]
			
			/* OS_curr = OS_next */
			LDR	r1,=OS_next
			LDR	r1,[r1,#0x00]
			LDR	r2,=OS_curr
			STR	r1,[r2,#0x00]
			
			/* pop registers r4-r11 */
			POP	{r4-r11}
			
			/* enabling interrupt requests */
			CPSIE	I
			
			/* return to the next thread */
			BX	lr
		}
