#ifndef CHARDEV_H
#define CHARDEV_H

// #include <linux/ioctl.h>

// The major device number
#define MAJOR_NUM 235

// Set the message of the device driver
#define MSG_SLOT_CHANNEL _IOW(MAJOR_NUM, 0, unsigned long)

#define DEVICE_RANGE_NAME "message_slot"
#define DEVICE_FILE_NAME "message_slot"
#define SUCCESS 0

#endif
