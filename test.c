#undef NDEBUG
#include <assert.h>

#include <mvar-internal.h>

#include <stdio.h>

typedef struct {
	MVar *mvar;
	int value;
} IncrementorState;

static void *
incrementor (void *data)
{
	IncrementorState *state = (IncrementorState *)data;
	int i;
	for (i = 0; i < 100000; ++i) {
		void *old = mvar_take (state->mvar);
		++state->value;
		mvar_put (state->mvar, (void *)((uintptr_t)old + 1));
	}
	return NULL;
}

int main ()
{
	{
		MVar *mvar = mvar_new ((void *)1);
		enum { THREAD_COUNT = 4 };
		pthread_t threads[THREAD_COUNT];
		IncrementorState state;
		state.mvar = mvar;
		state.value = 0;
		for (int i = 0; i < THREAD_COUNT; ++i) {
			pthread_create (&threads[i], NULL, incrementor, (void *)&state);
		}
		for (int i = 0; i < THREAD_COUNT; ++i) {
			pthread_join (threads[i], NULL);
		}
		void *final = mvar_take (state.mvar);
		mvar_free (mvar);
		assert (state.value == 100000 * THREAD_COUNT);
		assert (final == (void *) (100000 * THREAD_COUNT + 1));
	}

	printf ("All tests passed. :)\n");
	return 0;
}
