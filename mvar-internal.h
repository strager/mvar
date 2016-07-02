#ifndef MVAR_INTERNAL_H_2222195A_A36B_4D94_9066_E8645E3AA5D9
#define MVAR_INTERNAL_H_2222195A_A36B_4D94_9066_E8645E3AA5D9

#include <mvar.h>

#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>

struct MVar {
	void *_Atomic value;

	pthread_mutex_t mutex;
	pthread_cond_t cond;
	bool _Atomic have_waiters;
};

bool mvar_init (MVar *mvar, void *value);
void mvar_destroy (MVar *mvar);

bool mvar_try_put_locked (MVar *mvar, void *value);
void *mvar_try_take_locked (MVar *mvar);

void mvar_wake_readers (MVar *mvar);
void mvar_wake_writers (MVar *mvar);
void mvar_wake_readers_locked (MVar *mvar);
void mvar_wake_writers_locked (MVar *mvar);

#endif /* MVAR_INTERNAL_H_2222195A_A36B_4D94_9066_E8645E3AA5D9 */
