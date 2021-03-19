#ifndef MIROS_H_
#define MIROS_H_



typedef struct {
	void * sp; /* stack pointer */
	uint32_t timeout; /* timeout delay down-counter */
	uint8_t prio; /* thread priority */
/* other attributes associated with a thread */
} OSThread;

typedef void (*OSThreadHandler)(void);

void OS_init(void * stkSto, uint32_t stkSize);
/* callback to handle the idle coniditon */
void OS_onIdle(void);
/* this function must be called with interrupts DISABLED! */
void OS_sched(void);

void OS_run(void);

void OS_onStartup(void);

void OS_delay(uint32_t ticks);
/* process all timeouts  */
void OS_tick(void);

void OSThread_start(
	OSThread * me, 
	uint8_t prio, /* thread priority */
  OSThreadHandler threadHandler,
	void * stkSto,
	uint32_t stkSize);

#endif /* MIROS_H_*/
