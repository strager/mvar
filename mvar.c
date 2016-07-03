#include <mvar-internal.h>

#include <assert.h>
#include <errno.h>
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
		/* .mutex */
		/* .reader_cond */
		.have_waiting_readers = false,
		/* .writer_cond */
		.have_waiting_writers = false,
	};
	int rc;
	rc = pthread_mutex_init (&mvar->mutex, NULL);
	if (rc != 0) {
		errno = rc;
		return false;
	}
	rc = pthread_cond_init (&mvar->reader_cond, NULL);
	if (rc != 0) {
		int tmp_rc = pthread_mutex_destroy (&mvar->mutex);
		assert (tmp_rc == 0);
		errno = rc;
		return false;
	}
	rc = pthread_cond_init (&mvar->writer_cond, NULL);
	if (rc != 0) {
		int tmp_rc;
		tmp_rc = pthread_cond_destroy (&mvar->reader_cond);
		assert (tmp_rc == 0);
		tmp_rc = pthread_mutex_destroy (&mvar->mutex);
		assert (tmp_rc == 0);
		errno = rc;
		return false;
	}
	return true;
}

void
mvar_destroy (MVar *mvar)
{
	int rc;
	rc = pthread_cond_destroy (&mvar->writer_cond);
	assert (rc == 0);
	rc = pthread_cond_destroy (&mvar->reader_cond);
	assert (rc == 0);
	rc = pthread_mutex_destroy (&mvar->mutex);
	assert (rc == 0);
}

void
mvar_put (MVar *mvar, void *value)
{
	assert (value != NULL);
	if (mvar_try_put (mvar, value)) {
		return;
	}

	int rc;
	rc = pthread_mutex_lock (&mvar->mutex);
	assert (rc == 0);

	for (;;) {
		atomic_store (&mvar->have_waiting_writers, true);
		if (mvar_try_put_locked (mvar, value)) {
			break;
		}
		rc = pthread_cond_wait (&mvar->writer_cond, &mvar->mutex);
		/* EINTR is erroneously returned by some implementations (e.g. Bionic). */
		assert (rc == 0 || rc == EINTR);
	}

	rc = pthread_mutex_unlock (&mvar->mutex);
	assert (rc == 0);
}

void *
mvar_take (MVar *mvar)
{
	void *value;

	value = mvar_try_take (mvar);
	if (value) {
		return value;
	}

	int rc;
	rc = pthread_mutex_lock (&mvar->mutex);
	assert (rc == 0);

	for (;;) {
		atomic_store (&mvar->have_waiting_readers, true);
		value = mvar_try_take_locked (mvar);
		if (value) {
			break;
		}
		rc = pthread_cond_wait (&mvar->reader_cond, &mvar->mutex);
		/* EINTR is erroneously returned by some implementations (e.g. Bionic). */
		assert (rc == 0 || rc == EINTR);
	}

	rc = pthread_mutex_unlock (&mvar->mutex);
	assert (rc == 0);

	return value;
}

bool
mvar_try_put (MVar *mvar, void *value)
{
	assert (value != NULL);
	void *expected = NULL;
	if (atomic_compare_exchange_strong (&mvar->value, &expected, value)) {
		mvar_wake_readers (mvar);
		return true;
	}
	return false;
}

bool
mvar_try_put_locked (MVar *mvar, void *value)
{
	assert (value != NULL);
	void *expected = NULL;
	if (atomic_compare_exchange_strong (&mvar->value, &expected, value)) {
		mvar_wake_readers_locked (mvar);
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
			mvar_wake_writers (mvar);
			return old_value;
		}
	}
}

void *
mvar_try_take_locked (MVar *mvar)
{
	/* FIXME(strager): This is sub-optimal on some architectures. */
	for (;;) {
		/* TODO(strager): Consider memory_order_relaxed. */
		void *old_value = atomic_load (&mvar->value);
		if (old_value == NULL) {
			return NULL;
		}
		if (atomic_compare_exchange_weak (&mvar->value, &old_value, NULL)) {
			mvar_wake_writers_locked (mvar);
			return old_value;
		}
	}
}

void
mvar_wake_readers (MVar *mvar)
{
	if (!atomic_load (&mvar->have_waiting_readers)) {
		return;
	}

	int rc;
	rc = pthread_mutex_lock (&mvar->mutex);
	assert (rc == 0);

	mvar_wake_readers_locked (mvar);

	rc = pthread_mutex_unlock (&mvar->mutex);
	assert (rc == 0);
}

void
mvar_wake_writers (MVar *mvar)
{
	if (!atomic_load (&mvar->have_waiting_writers)) {
		return;
	}

	int rc;
	rc = pthread_mutex_lock (&mvar->mutex);
	assert (rc == 0);

	mvar_wake_writers_locked (mvar);

	rc = pthread_mutex_unlock (&mvar->mutex);
	assert (rc == 0);
}

void
mvar_wake_readers_locked (MVar *mvar)
{
	if (atomic_exchange (&mvar->have_waiting_readers, false)) {
		pthread_cond_signal (&mvar->reader_cond);
	}
}

void
mvar_wake_writers_locked (MVar *mvar)
{
	if (atomic_exchange (&mvar->have_waiting_writers, false)) {
		pthread_cond_signal (&mvar->writer_cond);
	}
}
