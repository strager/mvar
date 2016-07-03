#ifndef MVAR_INTERNAL_H_2222195A_A36B_4D94_9066_E8645E3AA5D9
#define MVAR_INTERNAL_H_2222195A_A36B_4D94_9066_E8645E3AA5D9

#include "mvar-internal-config.h"
#include <mvar.h>

#include <stdatomic.h>
#include <stdbool.h>

#if MVAR_USE_MACH_SEMAPHORE
#include <mach/mach_types.h>
#include <stdint.h>
#endif

struct MVar {
	void *_Atomic value;
#if MVAR_USE_MACH_SEMAPHORE
        /* put_sem is signalled when mvar_put is unblocked. */
        /* take_sem is signalled when mvar_take is unblocked. */
        semaphore_t _Atomic put_sem;
        semaphore_t _Atomic take_sem;
        int32_t _Atomic put_sem_value;
        int32_t _Atomic take_sem_value;
#endif
#if MVAR_USE_GENERATION_FUTEX
        /* put_generation is bumped on every mvar_put. */
        unsigned _Atomic put_generation;
        /* take_generation is bumped on every mvar_take. */
        unsigned _Atomic take_generation;
#endif
};

#if MVAR_USE_GENERATION_FUTEX
#define MVAR_GENERATION_HAVE_WAITERS ((unsigned) 1 << 0)
#define MVAR_GENERATION_BUMP ((unsigned) 1 << 1)
#endif

bool mvar_init (MVar *mvar, void *value);
void mvar_destroy (MVar *mvar);

#if MVAR_USE_MACH_SEMAPHORE
semaphore_t mvar_sem_init (semaphore_t _Atomic *sem);
void mvar_sem_signal (semaphore_t _Atomic *sem, int32_t _Atomic *sem_value);
void mvar_sem_wait (semaphore_t _Atomic *sem, int32_t _Atomic *sem_value);
#endif

#if MVAR_USE_GENERATION_FUTEX
void mvar_generation_signal (unsigned _Atomic *generation);
void mvar_generation_wait (unsigned _Atomic *generation);
#endif

#endif /* MVAR_INTERNAL_H_2222195A_A36B_4D94_9066_E8645E3AA5D9 */
