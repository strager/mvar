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
		pthread_t thread1, thread2;
		IncrementorState state;
		state.mvar = mvar;
		state.value = 0;
		pthread_create (&thread1, NULL, incrementor, (void *)&state);
		pthread_create (&thread2, NULL, incrementor, (void *)&state);
		pthread_join (thread1, NULL);
		pthread_join (thread2, NULL);
		void *final = mvar_take (state.mvar);
		mvar_free (mvar);
		assert (state.value == 200000);
		assert (final == (void *) 200001);
	}

	printf ("All tests passed. :)\n");
	return 0;
}
