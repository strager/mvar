#ifndef MVAR_INTERNAL_H_2222195A_A36B_4D94_9066_E8645E3AA5D9
#define MVAR_INTERNAL_H_2222195A_A36B_4D94_9066_E8645E3AA5D9

#define MVAR_USE_DISPATCH_SEMAPHORE 1

#include <mvar.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

#if MVAR_USE_DISPATCH_SEMAPHORE
#include <dispatch/dispatch.h>
#endif

struct MVar {
	void *_Atomic value;
#if MVAR_USE_DISPATCH_SEMAPHORE
        /* Signalled when mvar_put is unblocked. */
	dispatch_semaphore_t put_semaphore;
        /* Signalled when mvar_take is unblocked. */
	dispatch_semaphore_t take_semaphore;
#endif
};

bool mvar_init (MVar *mvar, void *value);
void mvar_destroy (MVar *mvar);

#endif /* MVAR_INTERNAL_H_2222195A_A36B_4D94_9066_E8645E3AA5D9 */
