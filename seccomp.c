#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/semaphore.h>
#include <linux/list.h>
#include <linux/wait.h>
#include <linux/eventpoll.h>
#include <linux/types.h>
#include "seccomp.h"


static bool should_sleep_killable(struct seccomp_filter *match,
				  struct seccomp_knotif *n)
{
	return match->wait_killable_recv && n->state == SECCOMP_NOTIFY_SENT;
}


static inline struct seccomp_knotif* find_notification(struct seccomp_filter *filter, u64 id)
{
	struct seccomp_knotif *cur;

	lockdep_assert_held(&filter->notify_lock);

	list_for_each_entry(cur, &filter->notif->notifications, list) {
		if (cur->id == id)
			return cur;
	}

	return NULL;
}


long seccomp_notify_recv_original(struct seccomp_filter *filter, struct seccomp_notif* buf){
   struct seccomp_knotif *knotif = NULL, *cur;
   struct seccomp_notif notif;

   ssize_t ret;

	ret = down_interruptible(&filter->notif->request);
	if (ret < 0)
      printk("down_interruptible() failed");
		return ret;

	mutex_lock(&filter->notify_lock);
	list_for_each_entry(cur, &filter->notif->notifications, list) {
		if (cur->state == SECCOMP_NOTIFY_INIT) {
			knotif = cur;
			break;
		}
	}

   if (!knotif) {
		ret = -ENOENT;
      printk("list_for_each_entry() failed");
		goto out;
	}

	notif.id = knotif->id;
	notif.pid = task_pid_vnr(knotif->task);
	notif.data = *(knotif->data);

	knotif->state = SECCOMP_NOTIFY_SENT;
	wake_up_poll(&filter->wqh, EPOLLOUT | EPOLLWRNORM);
	ret = 0;

out:
	mutex_unlock(&filter->notify_lock);

	if (ret == 0 && memcpy(buf, &notif, sizeof(notif))) {
		ret = -EFAULT;

		/*
		 * Userspace screwed up. To make sure that we keep this
		 * notification alive, let's reset it back to INIT. It
		 * may have died when we released the lock, so we need to make
		 * sure it's still around.
		 */
		mutex_lock(&filter->notify_lock);
		knotif = find_notification(filter, notif.id);
		if (knotif) {
			/* Reset the process to make sure it's not stuck */
			if (should_sleep_killable(filter, knotif))
				complete(&knotif->ready);
			knotif->state = SECCOMP_NOTIFY_INIT;
			up(&filter->notif->request);
		}
		mutex_unlock(&filter->notify_lock);
	}

	return ret;

}
