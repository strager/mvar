#include <mvar-internal.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

MVar *
mvar_new (void *value)
{
	MVar *mvar = malloc (sizeof (*mvar));
	if (mvar == NULL) {
		return NULL;
	}
	if (!mvar_init (mvar, value)) {
		free (mvar);
		return NULL;
	}
	return mvar;
}

void
mvar_free (MVar *mvar)
{
	mvar_destroy (mvar);
	free (mvar);
}

bool
mvar_init (MVar *mvar, void *value)
{
	*mvar = (struct MVar) {
		.value = value,
#if MVAR_USE_DISPATCH_SEMAPHORE
		.put_semaphore = dispatch_semaphore_create (value ? 0 : 1),
		.take_semaphore = dispatch_semaphore_create (value ? 1 : 0),
#endif
	};
#if MVAR_USE_DISPATCH_SEMAPHORE
	assert (mvar->put_semaphore);
	assert (mvar->take_semaphore);
#endif
	return true;
}

void
mvar_destroy (MVar *mvar)
{
	assert (!atomic_load_explicit (&mvar->value, memory_order_relaxed));
	dispatch_release (mvar->put_semaphore);
	dispatch_release (mvar->take_semaphore);
}

void
mvar_put (MVar *mvar, void *value)
{
	assert (value != NULL);
	for (;;) {
#if MVAR_USE_DISPATCH_SEMAPHORE
		if (mvar_try_put (mvar, value)) {
			break;
		}
		dispatch_semaphore_wait (mvar->put_semaphore, DISPATCH_TIME_FOREVER);
#else
#error "Unknown implementation"
#endif
	}
}

void *
mvar_take (MVar *mvar)
{
	void *value;

	for (;;) {
#if MVAR_USE_DISPATCH_SEMAPHORE
		value = mvar_try_take (mvar);
		if (value) {
			return value;
		}
		dispatch_semaphore_wait (mvar->take_semaphore, DISPATCH_TIME_FOREVER);
#else
#error "Unknown implementation"
#endif
	}

	return value;
}

bool
mvar_try_put (MVar *mvar, void *value)
{
	assert (value != NULL);
	void *expected = NULL;
	if (atomic_compare_exchange_strong (&mvar->value, &expected, value)) {
		dispatch_semaphore_signal (mvar->take_semaphore);
		return true;
	}
	return false;
}

void *
mvar_try_take (MVar *mvar)
{
	/* FIXME(strager): This is sub-optimal on some architectures. */
	for (;;) {
		/* TODO(strager): Consider memory_order_relaxed. */
		void *old_value = atomic_load (&mvar->value);
		if (old_value == NULL) {
			return NULL;
		}
		if (atomic_compare_exchange_weak (&mvar->value, &old_value, NULL)) {
			dispatch_semaphore_signal (mvar->put_semaphore);
			return old_value;
		}
	}
}
