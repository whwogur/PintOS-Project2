/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
/* 세마포어 SEMA를 VALUE로 초기화 */
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value;
	list_init (&sema->waiters);
}



/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */	 
void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;        /* 인터럽트 레벨(enum)을 저장 */

	ASSERT (sema != NULL);            /* 세마포어 포인터가 널(NULL)이 아님을 확인 */
	ASSERT (!intr_context ());				/* 인터럽트 컨텍스트 내에서 호출되지 않았음을 확인 */

	old_level = intr_disable ();      /* 현재 인터럽트 레벨을 비활성화하고, 이전 인터럽트 레벨을 old_level에 저장 */
	while (sema->value == 0) {        /* 세마포어 값이 0인 동안 실행 */

		list_push_back (&sema->waiters, &thread_current ()->elem);   /* 현재 스레드의 elem을 세마포어의 대기자 목록에 추가 */
		// list_insert_ordered(&sema->waiters, &thread_current()->elem, cmp_priority, 0);
		thread_block ();                /* 현재 스레드를 block, 다른 스레드가 실행 */
	}
	sema->value--;                    /* 세마포어의 값을 원자적으로 감소 */
	intr_set_level (old_level);       /* 이전 인터럽트 레벨을 복원 */
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else {
		success = false;
	}
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
/* 공유자원 사용 완료 후 V ,signal 실행해서 다음 스레드 깨우는 함수*/
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;           /* 인터럽트 레벨(enum)을 저장 */

	ASSERT (sema != NULL);               /* 세마포어 포인터가 널(NULL)이 아님을 확인 */


	old_level = intr_disable ();         /* 현재 인터럽트 레벨을 비활성화하고, 이전 인터럽트 레벨을 old_level에 저장 */
	if (!list_empty (&sema->waiters))    /* 세마포어의 대기자 목록이 비어있지 않은 경우에만 실행 */
		{  /* 대기자 목록에서 가장 앞에 있는 스레드를 꺼내어 차단 해제(unblock)하고 실행을 재개 */
			list_sort(&sema->waiters, cmp_priority, 0);
			ASSERT (!list_empty(&sema->waiters));     /* ?  */
			thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));
		}
	sema->value++;                       /* 세마포어의 값을 증가 */
	test_max_priority();
	intr_set_level (old_level);          /* 이전 인터럽트 레벨을 복원 */
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
/* 세마포어 초기화 , 0이면 사용중 1이면 사용가능*/
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));
	
	struct thread *cur = thread_current();
	/* 만약 해당 lock을 누가 사용하고 있다면 */
	if (lock->holder) {
		cur->lock_to_wait_on = lock;    /* 현재 스레드의 wait_on_lock에 해당 lock을 저장 */
		/* 지금 lock을 소유하고 있는 스레드의 donations에 현재 스레드를 저장 */
		list_push_back(&lock->holder->donators_list, &cur->d_elem);
		donate_priority();
	}

	sema_down (&lock->semaphore);
	cur->lock_to_wait_on = NULL;      /* lock을 획득했으므로 대기하고 있는 lock이 이제는 없음 */
	lock->holder = cur;
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
/* LOCK을 획득하려고 시도하고 성공하면 true를 반환하고 실패하면 false를 반환
	잠금이 현재 스레드에 의해 아직 유지되지 않아야 함

이 기능은 sleep 으로 전환되지 않으므로 인터럽트 핸들러 내에서 호출될 수 있음*/
bool
lock_try_acquire (struct lock *lock) { 
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	/* donations 리스트에서 해당 lock을 필요로 하는 스레드를 삭제 */
	remove_with_lock(lock);
	/* 현재 스레드의 priority를 업데이트 */
	refresh_priority();

	lock->holder = NULL;
	/* 해당 lock에서 기다리고 있는 스레드 하나 동작 */
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
/* 조건 변수에서 대기하는 스레드를 설정 -> condition variable의 waiters list에 우선순위 순서로 삽입되도록 수정 */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);      /* waiter를 cond의 waiters 리스트에 추가 */
	
	list_push_back (&cond->waiters, &waiter.elem);
	// sema_down 전에는 waiter.semaphore.waiters에 thread의 list_elem이 연결이 안 됨
	// list_insert_ordered (&cond->waiters, &waiter.elem, cmp_sema_priority,0);

	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
/* 조건 변수에서 대기 중인 스레드 중 하나를 깨우는 역할 -> condition variable의 waiters list를 우선순위로 재 정렬 */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		list_sort(&cond->waiters, cmp_sema_priority, NULL);
		sema_up (&list_entry (list_pop_front (&cond->waiters),struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}

/**
 * semaphore_elem으로부터 각 semaphore_elem의 쓰레드 디스크립터를 획득
 * 첫 번째 인자의 우선순위가 두 번째 인자의 우선순위보다 높으면 1을 반환 낮으면 0을 반환
*/
bool cmp_sema_priority (const struct list_elem *a, const struct list_elem *b, void *aux)
{
	struct semaphore_elem *sa = list_entry(a, struct semaphore_elem, elem);
	struct semaphore_elem *sb = list_entry(b, struct semaphore_elem, elem);
	
	// sema_down에서 semaphore.waiters에 thread의 list_elem이 들어갔기 때문에 아래 두 줄 가능.
	struct list_elem *sa_e = list_begin(&(sa->semaphore.waiters));
	struct list_elem *sb_e = list_begin(&(sb->semaphore.waiters));
 
	struct thread *sa_t = list_entry(sa_e, struct thread, elem);
	struct thread *sb_t = list_entry(sb_e, struct thread, elem);

	return (sa_t->priority) > (sb_t->priority);
}


void donate_priority(void)
{
	int depth;
	struct thread *t = thread_current();
	struct thread *holder;

	/* 최대 depth 8로 설정 */
	for (depth = 0; depth < 8; depth++) {
		/* 더 이상 nested가 없으면 탈출*/
		if (!t->lock_to_wait_on) break; 

		holder = t->lock_to_wait_on->holder;
		holder->priority = t->priority;    /* 우선 순위를 donation */ 
		t = holder;                        /* 다음 depth 시작 */
	}
}

/* LOCK을 release함과 동시에 해당 LOCK을 얻기 위해 priority를 기부했던
   donator threads들을 LOCK을 release하는 thread의 donators list에서 제거
 */
void remove_with_lock(struct lock *lock)
{
	struct thread *cur = thread_current();
	struct list_elem *e;
	struct thread *t;

	for (e = list_begin(&cur->donators_list); e != list_end(&cur->donators_list);) {
		t = list_entry(e, struct thread, d_elem);
		e = list_next(e);
		if (t->lock_to_wait_on == lock) {
			list_remove(&t->d_elem);
		}
	}
}

/**
 * 해당 스레드가 donation을 받고 있다면, 초기 우선 순위보다 더 큰 값으로 우선 순위가 갱신
*/
void refresh_priority(void)
{
	struct thread *cur = thread_current();
	cur->priority = cur->original_priority;     /* 미리 저장해둔 진짜 우선순위*/ 

	/* donation을 받고 있다면 */
	if (!list_empty(&cur->donators_list)) {
		list_sort(&cur->donators_list, cmp_donator_priority, NULL);

		struct thread *top_pri_donator = list_entry(list_front(&cur->donators_list), struct thread, d_elem);
		if (top_pri_donator->priority > cur->priority) {      /* 만약 초기 우선 순위보다 더 큰 값 */
			cur->priority = top_pri_donator->priority;
		}
	}
}

bool cmp_donator_priority (const struct list_elem *new, const struct list_elem *existing, void *aux)
{
	struct thread *new_donator = list_entry(new, struct thread, d_elem);
	struct thread *existing_donator = list_entry(existing, struct thread, d_elem);

	return new_donator->priority > existing_donator->priority ? 1 : 0;
}