/*
* chardev.c: Creates a read-only char device that says how many times
* you have read from the dev file
*/

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/irq.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/kthread.h>
#include <linux/poll.h>
#include <linux/file.h>
#include <linux/tee_drv.h>
#include <linux/tee.h>
#include <linux/seccomp.h>
#include "tee_client_api_extensions.h"
#include "tee_client_api.h"
#include "seccomp.h"
//#include "teec_trace.h"


/* Prototypes - this would normally go in a .h file */
static int device_open(struct inode *, struct file *);
static int device_release(struct inode *, struct file *);
static ssize_t device_read(struct file *, char __user *, size_t, loff_t *);
static ssize_t device_write(struct file *, const char __user *, size_t,
loff_t *);
static long device_ioctl(struct file *, unsigned int, unsigned long);

#define SUCCESS 0
#define DEVICE_NAME "teemonitordev" /* Dev name as it appears in /proc/devices */
#define BUF_LEN 80 /* Max length of the message from the device */
#define SET_SESSION 3
#define SET_NOTIFYFD 4
#define INVOKE 5
#define TA_HELLO_WORLD_CMD_INC_VALUE 0

/* Global variables are declared as static, so are global within the file. */

static int major; /* major number assigned to our device driver */

enum {
CDEV_NOT_USED = 0,
CDEV_EXCLUSIVE_OPEN = 1,
};

/* Is device open? Used to prevent multiple access to device */
static atomic_t already_open = ATOMIC_INIT(CDEV_NOT_USED);

static char msg[BUF_LEN + 1]; /* The msg the device will give when asked */

//static struct task_struct *k;

static TEEC_Session session;
static int notifyfd;

static struct class *cls;

static struct file_operations chardev_fops = {
.read = device_read,
.write = device_write,
.open = device_open,
.unlocked_ioctl = device_ioctl,
.compat_ioctl   = device_ioctl,
.release = device_release,
};

static int __init teemonitordev_init(void)
 {
 major = register_chrdev(0, DEVICE_NAME, &chardev_fops);

 if (major < 0) {
 pr_alert("Registering char device failed with %d\n", major);
 return major;
 }

 pr_info("I was assigned major number %d.\n", major);

 cls = class_create(THIS_MODULE, DEVICE_NAME);
 device_create(cls, NULL, MKDEV(major, 0), NULL, DEVICE_NAME);

 pr_info("Device created on /dev/%s\n", DEVICE_NAME);

 return SUCCESS;
 }

 static void __exit teemonitordev_exit(void)
 {
 device_destroy(cls, MKDEV(major, 0));
 class_destroy(cls);

 /* Unregister the device */
 unregister_chrdev(major, DEVICE_NAME);
 }




static int notification(void *arg){

	struct fd f; 
   struct seccomp_notif *req;
   //struct seccomp_notif_resp *resp;
   struct seccomp_filter *filter;
   long rc;

   f = fdget(notifyfd);
	if(!f.file){
		printk(KERN_INFO "notifyfd: Bad file number \n");
      return -1;
	}
   memset(&req, 0, sizeof(req));
   filter = f.file->private_data;
   rc = seccomp_notify_recv_original(filter, req);
	if (rc) {
		printk(KERN_INFO "SECCOMP_IOCTL_NOTIF_RECV: error\n");
      return -1;
	}
   printk("nr:%d\n",req->data.nr);

   //invoke_command(NULL);

   //resp->id = req->id;
   //resp->id = req->id;
   //resp->flags = 0;
   
   //if(req->data->nr){
   // resp->flags = SECCOMP_USER_NOTIF_FLAG_CONTINUE;
   //}else{
   //resp->error = -EPERM;
   //}

   //if(vfs_ioctl(f.file, SECCOMP_IOCTL_NOTIF_SEND, resp)==-1){
      //printk(KERN_INFO "SECCOMP_IOCTL_NOTIF_SEND: error\n")
   //}

   return 0;

}

static int invoke_command(void *arg)
{
   TEEC_Result res;
	TEEC_Operation op;
	uint32_t err_origin;

	memset(&op, 0, sizeof(op));
	op.paramTypes = TEEC_PARAM_TYPES(TEEC_VALUE_INOUT, TEEC_NONE,
					 TEEC_NONE, TEEC_NONE);
	op.params[0].value.a = 42;
	printk(KERN_INFO "Invoking TA to increment %d\n", op.params[0].value.a);
	res = TEEC_InvokeCommand(&session, TA_HELLO_WORLD_CMD_INC_VALUE, &op,
				 &err_origin);
   if(res!=TEEC_SUCCESS){
      printk(KERN_INFO "TEEC_InvokeCommand failed\n");
      return -1;
   }

   return 0;
}

//static int invoke_command_from_thread(void *arg){

   ////k = kthread_create(invoke_command, NULL, "kthreaddd");
   //if(!invoke_command(NULL)){
      //printk(KERN_INFO "invoke_command failed\n");
   //}
   //printk(KERN_INFO "[%s] wake up as kthread\n", k->comm);
   //wake_up_process(k);

   //return 0;

//}

 static int device_open(struct inode *inode, struct file *file)
 {
   static int counter = 0;
 
   if (atomic_cmpxchg(&already_open, CDEV_NOT_USED, CDEV_EXCLUSIVE_OPEN))
   return -EBUSY;

   sprintf(msg, "I already told you %d times Hello world!\n", counter++);
   try_module_get(THIS_MODULE);

   return SUCCESS;
 }

 /* Called when a process closes the device file. */
 static int device_release(struct inode *inode, struct file *file)
 {
   /* We're now ready for our next caller */
   atomic_set(&already_open, CDEV_NOT_USED);

   /* Decrement the usage count, or else once you opened the file, you will
   * never get rid of the module.
   */
   module_put(THIS_MODULE);

    return SUCCESS;
 }

 /* Called when a process, which already opened the dev file, attempts to
 * read from it.
 */
 static ssize_t device_read(struct file *filp, /* see include/linux/fs.h */
 char __user *buffer, /* buffer to fill with data */
 size_t length, /* length of the buffer */
 loff_t *offset)
 {
 /* Number of bytes actually written to the buffer */
 int bytes_read = 0;
 const char *msg_ptr = msg;

 if (!*(msg_ptr + *offset)) { /* we are at the end of message */
 *offset = 0; /* reset the offset */
 return 0; /* signify end of file */
 }

 msg_ptr += *offset;

 /* Actually put the data into the buffer */
 while (length && *msg_ptr) {
 /* The buffer is in the user data segment, not the kernel
 * segment so "*" assignment won't work. We have to use
 * put_user which copies data from the kernel data segment to
 * the user data segment.
 */
 put_user(*(msg_ptr++), buffer++);
 length--;
 bytes_read++;
 }

 *offset += bytes_read;

 /* Most read functions return the number of bytes put into the buffer. */
 return bytes_read;
 }

 /* Called when a process writes to dev file: echo "hi" > /dev/hello */
 static ssize_t device_write(struct file *filp, const char __user *buff,
 size_t len, loff_t *off)
 {
 pr_alert("Sorry, this operation is not supported.\n");
 return -EINVAL;
 }

static long device_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
   printk("device_ioctl\n");

   switch (cmd) {
   case SET_SESSION:
      printk("SET_Session\n");
      printk("session from kernel:%ld\n",arg);
      if(copy_from_user(&session,(TEEC_Session*)arg,sizeof(session))){
            return -EFAULT;
      }
      
      break;
   case SET_NOTIFYFD:
      if(copy_from_user(&notifyfd,(int*)arg,sizeof(notifyfd))){
            return -EFAULT;
      }
      break;
   case INVOKE:
      printk("INVOKE\n");
      notification(NULL);
      break;
   default:
      printk(KERN_WARNING "unsupported command %d\n", cmd);
      return -EFAULT;
   }
   return 0;
}


 module_init(teemonitordev_init);
 module_exit(teemonitordev_exit);

 MODULE_LICENSE("GPL");
