#include <mvar-internal.h>

#include <assert.h>
#include <stdatomic.h>
#include <stdlib.h>

#if MVAR_USE_MACH_SEMAPHORE
#include <mach/kern_return.h>
#include <mach/mach_init.h>
#include <mach/semaphore.h>
#include <mach/task.h>
#endif

#if MVAR_USE_GENERATION_FUTEX
#include <errno.h>
#include <linux/futex.h>
#include <sys/syscall.h>
#include <sys/time.h>
#include <unistd.h>
#endif

#if MVAR_USE_GENERATION_FUTEX
static int
futex(int *uaddr, int op, int val, const struct timespec *timeout, int *uaddr2, int val3)
{
	return syscall(SYS_futex, uaddr, op, val, timeout, uaddr2, val3);
}
#endif

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
#if MVAR_USE_MACH_SEMAPHORE
		.put_sem_value = 0,
		.take_sem_value = 0,
		.put_sem = SEMAPHORE_NULL,
		.take_sem = SEMAPHORE_NULL,
#endif
#if MVAR_USE_GENERATION_FUTEX
		.put_generation = 0,
		.take_generation = 0,
#endif
	};
	return true;
}

void
mvar_destroy (MVar *mvar)
{
	assert (!atomic_load_explicit (&mvar->value, memory_order_relaxed));
#if MVAR_USE_MACH_SEMAPHORE
	if (mvar->put_sem != SEMAPHORE_NULL) {
		kern_return_t rc = semaphore_destroy (mach_task_self(), mvar->put_sem);
		assert (rc == KERN_SUCCESS);
	}
	if (mvar->take_sem != SEMAPHORE_NULL) {
		kern_return_t rc = semaphore_destroy (mach_task_self(), mvar->take_sem);
		assert (rc == KERN_SUCCESS);
	}
#endif
}

void
mvar_put (MVar *mvar, void *value)
{
	assert (value != NULL);
	for (;;) {
		if (mvar_try_put (mvar, value)) {
			break;
		}
#if MVAR_USE_MACH_SEMAPHORE
		mvar_sem_wait (&mvar->put_sem, &mvar->put_sem_value);
#elif MVAR_USE_GENERATION_FUTEX
		mvar_generation_wait (&mvar->put_generation);
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
		value = mvar_try_take (mvar);
		if (value) {
			return value;
		}
#if MVAR_USE_MACH_SEMAPHORE
		mvar_sem_wait (&mvar->take_sem, &mvar->take_sem_value);
#elif MVAR_USE_GENERATION_FUTEX
		mvar_generation_wait (&mvar->take_generation);
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
#if MVAR_USE_MACH_SEMAPHORE
		mvar_sem_signal (&mvar->take_sem, &mvar->take_sem_value);
#elif MVAR_USE_GENERATION_FUTEX
		mvar_generation_signal (&mvar->take_generation);
#else
#error "Unknown implementation"
#endif
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
#if MVAR_USE_MACH_SEMAPHORE
			mvar_sem_signal (&mvar->put_sem, &mvar->put_sem_value);
#elif MVAR_USE_GENERATION_FUTEX
			mvar_generation_signal (&mvar->put_generation);
#else
#error "Unknown implementation"
#endif
			return old_value;
		}
	}
}

#if MVAR_USE_MACH_SEMAPHORE
semaphore_t
mvar_sem_init (semaphore_t _Atomic *sem)
{
	/* N.B. semaphore_t itself doesn't need to be synchronized. */
	semaphore_t old_sem = atomic_load_explicit (sem, memory_order_relaxed);
	if (old_sem != SEMAPHORE_NULL) {
		return old_sem;
	}

	semaphore_t new_sem;
	kern_return_t rc;
	rc = semaphore_create (mach_task_self (), &new_sem, SYNC_POLICY_FIFO, 0);
	assert (rc == KERN_SUCCESS);

	if (!atomic_compare_exchange_strong (sem, &old_sem, new_sem)) {
		assert (old_sem != SEMAPHORE_NULL);
		rc = semaphore_destroy (mach_task_self (), new_sem);
		assert (rc == KERN_SUCCESS);
		return old_sem;
	}
	return new_sem;
}

void
mvar_sem_signal (semaphore_t _Atomic *sem, int32_t _Atomic *sem_value)
{
	int32_t old_value = atomic_fetch_add (sem_value, 1);
	if (old_value >= 0) {
		return;
	}
	semaphore_t s = mvar_sem_init (sem);
	kern_return_t rc = semaphore_signal (s);
	assert (rc == KERN_SUCCESS);
}

void
mvar_sem_wait (semaphore_t _Atomic *sem, int32_t _Atomic *sem_value)
{
	int32_t old_value = atomic_fetch_sub (sem_value, 1);
	if (old_value > 0) {
		return;
	}
	semaphore_t s = mvar_sem_init (sem);
	kern_return_t rc;
	do {
		rc = semaphore_wait (s);
		assert (rc == KERN_SUCCESS || rc == KERN_ABORTED);
	} while (rc == KERN_ABORTED); /* EINTR */
}
#endif

#if MVAR_USE_GENERATION_FUTEX
void
mvar_generation_signal (unsigned _Atomic *generation)
{
	unsigned old = atomic_fetch_add (generation, MVAR_GENERATION_BUMP);
	if (old & MVAR_GENERATION_HAVE_WAITERS) {
		atomic_fetch_and (generation, ~(unsigned) MVAR_GENERATION_HAVE_WAITERS);
		_Static_assert (sizeof (unsigned _Atomic) == sizeof (int), "Expected unsigned _Atomic to match int");
		int rc = futex ((int *) generation, FUTEX_WAKE | FUTEX_PRIVATE_FLAG, 1, NULL, NULL, 0);
		assert (rc == 0 || rc == 1);
	}
}

void
mvar_generation_wait (unsigned _Atomic *generation)
{
	unsigned old = atomic_fetch_or (generation, MVAR_GENERATION_HAVE_WAITERS);
	_Static_assert (sizeof (unsigned _Atomic) == sizeof (int), "Expected unsigned _Atomic to match int");
	int rc = futex ((int *) generation, FUTEX_WAIT | FUTEX_PRIVATE_FLAG, old | MVAR_GENERATION_HAVE_WAITERS, NULL, NULL, 0);
	if (rc == -1) {
		assert (errno == EWOULDBLOCK || errno == EINTR);
		return;
	}
	assert (rc == 0);
}
#endif
