#undef __KERNEL__
#define __KERNEL__
#undef MODULE
#define MODULE
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/string.h>
#include <linux/errno.h>
#include "message_slot.h" /* our custom definitios of IOCTL operations */

MODULE_LICENSE("GPL");

#define SUCCESS 0
#define DEVICE_RANGE_NAME "message_slot"
#define DEVICE_FILE_NAME "message_slot"

/* Clarification: I was inspired by the template from home recitation #6 */

/** ================== MODULE STRUCTURE =========================== **/

struct slot { /* allows an array of message slots for the driver */
    struct channel* channels;
};

struct channel { /* allows a linked list of channels for a slot, in which the message is saved */
    unsigned long id;
    ssize_t message_len;
    char* message;
    struct channel* next;
};

static struct slot* slots[257] = {NULL}; /* message slots (257 even though max is 256 channels bc wasn't sure if 256 counts) */


/** ================== DEVICE FUNCTIONS =========================== **/

static int device_open(struct inode* inode,
                        struct file*  file)
{
    int minor_number;
    struct slot* curr_slot;

    minor_number = iminor(inode);
    if (slots[minor_number] == NULL){ /* slot doesn't exist yet, so we should create a new slot */
        curr_slot = (struct slot*)kmalloc(sizeof(struct slot), GFP_KERNEL);
        if (curr_slot == NULL){ /* allocation failed */
            return -ENOMEM;
        }
        curr_slot->channels = NULL;
        slots[minor_number] = curr_slot;
    }

    return SUCCESS; /* if slot exists then there is no need to do anything */
}


static long device_ioctl(struct file* file,
                            unsigned int ioctl_command_id,
                            unsigned long ioctl_param)
{
    if ((ioctl_command_id == MSG_SLOT_CHANNEL) && (ioctl_param > 0)){
        file->private_data = (void*) ioctl_param; /* set channel id */
        return SUCCESS;
    }
    else{
        return -EINVAL;
    }
}


static ssize_t device_write(struct file* file,
                            const char __user* buffer,
                            size_t length,
                            loff_t* offset)
{
    ssize_t i;
    int minor_number;
    unsigned long channel_id;
    struct channel* curr_channel;
    struct channel* new_channel = NULL;
    char* tmp = NULL;
    char* message;

    if (!access_ok(buffer, length) || buffer == NULL){ /* validating user's buffer */
        return -EFAULT; /* bad address */
    }

    if (file->private_data == NULL){ /* no channel set for this fd */
        return -EINVAL;
    }

    if (length == 0 || length > 128){ /* message is either empty or too long */
        return -EMSGSIZE;
    }

    minor_number = iminor(file->f_inode);
    channel_id = (unsigned long)(file->private_data);
    curr_channel = (slots[minor_number])->channels;

    while ((curr_channel != NULL) && (curr_channel->id != channel_id)){
        curr_channel = curr_channel->next; /* look for the specified channel */
    }
    if (curr_channel == NULL){ /* search was not successful -> specified channel doesn't exist.
                                       that means that we should create the channel */
        new_channel = (struct channel*)kmalloc(sizeof(struct channel), GFP_KERNEL);
        if (new_channel == NULL){ /* allocation failed */
            return -ENOMEM;
        }
        new_channel->id = channel_id;
        new_channel->next = slots[minor_number]->channels;
        curr_channel = new_channel;
    }
    if (curr_channel->message != NULL){ /* there's an old message to override */
        tmp = curr_channel->message;
    }

    message = (char*)kmalloc(length * sizeof(char), GFP_KERNEL); /* allocate space for new message */
    if (message == NULL){ /* allocation failed */
        return -ENOMEM;
    }
    for (i = 0; i < length; i++){
        if (get_user(message[i], &buffer[i]) < 0){ /* copy message from user's buffer */
            return -EFAULT;
        }
    }
    curr_channel->message = message;
    curr_channel->message_len = length; /* update message length */

    if (new_channel != NULL){ /* we need to update new channel now that write was successful */
        slots[minor_number]->channels = new_channel;
    }

    if (tmp != NULL){ /* there was an old message that now can be freed after write was successful */
        kfree(tmp);
    }

    return i; /* return the number of input characters used. should always be length */
}


static ssize_t device_read(struct file* file,
                            char __user* buffer,
                            size_t length,
                            loff_t* offset)
{
    ssize_t i;
    int minor_number;
    unsigned long channel_id;
    struct channel* curr_channel;
    ssize_t message_len;
    char* message;

    if (!access_ok(buffer, length) || buffer == NULL){ /* validating user's buffer */
        return -EFAULT; /* bad address */
    }

    if (file->private_data == NULL){ /* no channel set for this fd */
        return -EINVAL;
    }

    minor_number = iminor(file->f_inode);
    channel_id = (unsigned long)(file->private_data);
    curr_channel = (slots[minor_number])->channels;

    while ((curr_channel != NULL) && (curr_channel->id != channel_id)){
        curr_channel = curr_channel->next; /* look for the specified channel */
    }
    if (curr_channel == NULL){ /* search was not successful -> specified channel doesn't exist.
                                   that means that there is no message in this channel */
        return -EWOULDBLOCK;
    }
    else{ /* channel found */
        message_len = curr_channel->message_len;
        if (length < message_len){ /* buffer doesn't have enough space for the whole message */
            return -ENOSPC;
        }

        message = curr_channel->message;
        for (i = 0; i < message_len; i++){
            if (put_user(message[i], &buffer[i]) < 0){ /* copy message into user's buffer */
                return -EFAULT;
            }
        }

        return i; /* return the number of characters read. should always be message_len */
    }
}


/** ==================== DEVICE SETUP ============================= **/

/* this structure will hold the functions to be called
    when a process does something to the device we created */
struct file_operations fops = {
        .owner	  = THIS_MODULE,
        .read           = device_read,
        .write          = device_write,
        .open           = device_open,
        .unlocked_ioctl = device_ioctl,
};


/* loader -> initialize the module - register the character device */
static int __init message_slot_init(void)
{
    int rc = -1;

    rc = register_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME, &fops); /* register driver capabilities */
    if (rc < 0){ /* there was an error registering the device */
        printk(KERN_ERR "%s registration failed for %d\n", DEVICE_FILE_NAME, MAJOR_NUM);
        return rc;
    }

    return SUCCESS;
}


/* unloader -> cleanup the module - unregister the character device and free all allocated memory */
static void __exit message_slot_cleanup(void)
{
    int i;
    struct slot* curr_slot;
    struct channel* curr_channel;
    struct channel* tmp;

    for (i = 0; i < 257; i++){
        curr_slot = slots[i];
        if (curr_slot != NULL){
            curr_channel = curr_slot->channels;
            while (curr_channel != NULL){ /* free all messages and all channels of a slot */
                kfree(curr_channel->message);
                tmp = curr_channel->next;
                kfree(curr_channel);
                curr_channel = tmp;
            }
            kfree(curr_slot); /* free slot */
        }
    }

    unregister_chrdev(MAJOR_NUM, DEVICE_RANGE_NAME); /* unregister device */
}

module_init(message_slot_init);
module_exit(message_slot_cleanup);

/** as for memory allocation complexity, there are at most 256 allocations for slots, and for each slot,
 * there is an allocation for each channel used and for each channel, an allocation for a message in the
 * size of the message. So, during the module's run, the total memory that is allocated is S+C+C*M where
 * S <= 256 * C * M if C >= 1 (since a channel is only allocated if it has been written to), and M >= 1,
 * since an empty message is not written, and so S+C+C*M <= 256*C*M+C*M+C*M = 258*C*M = O(C*M) (if C = 0
 * then in the worst case all possible slots will be allocated, which is 256, and 256 = O(1) **/
