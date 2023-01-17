#ifndef SECCOMP_H
#define SECCOMP_H 


enum notify_state {
	SECCOMP_NOTIFY_INIT,
	SECCOMP_NOTIFY_SENT,
	SECCOMP_NOTIFY_REPLIED,
};

struct seccomp_knotif {
	/* The struct pid of the task whose filter triggered the notification */
	struct task_struct *task;

	/* The "cookie" for this request; this is unique for this filter. */
	u64 id;

	/*
	 * The seccomp data. This pointer is valid the entire time this
	 * notification is active, since it comes from __seccomp_filter which
	 * eclipses the entire lifecycle here.
	 */
	const struct seccomp_data *data;

	/*
	 * Notification states. When SECCOMP_RET_USER_NOTIF is returned, a
	 * struct seccomp_knotif is created and starts out in INIT. Once the
	 * handler reads the notification off of an FD, it transitions to SENT.
	 * If a signal is received the state transitions back to INIT and
	 * another message is sent. When the userspace handler replies, state
	 * transitions to REPLIED.
	 */
	enum notify_state state;

	/* The return values, only valid when in SECCOMP_NOTIFY_REPLIED */
	int error;
	long val;
	u32 flags;

	/*
	 * Signals when this has changed states, such as the listener
	 * dying, a new seccomp addfd message, or changing to REPLIED
	 */
	struct completion ready;

	struct list_head list;

	/* outstanding addfd requests */
	struct list_head addfd;
};

struct seccomp_kaddfd {
	struct file *file;
	int fd;
	unsigned int flags;
	__u32 ioctl_flags;

	union {
		bool setfd;
		/* To only be set on reply */
		int ret;
	};
	struct completion completion;
	struct list_head list;
};

struct notification {
	struct semaphore request;
	u64 next_id;
	struct list_head notifications;
};

struct action_cache {
	DECLARE_BITMAP(allow_native, SECCOMP_ARCH_NATIVE_NR);
#ifdef SECCOMP_ARCH_COMPAT
	DECLARE_BITMAP(allow_compat, SECCOMP_ARCH_COMPAT_NR);
#endif
};


struct seccomp_filter {
	refcount_t refs;
	refcount_t users;
	bool log;
	bool wait_killable_recv;
	struct action_cache cache;
	struct seccomp_filter *prev;
	struct bpf_prog *prog;
	struct notification *notif;
	struct mutex notify_lock;
	wait_queue_head_t wqh;
};



long seccomp_notify_recv_original(struct seccomp_filter*, struct seccomp_notif*);

#endif