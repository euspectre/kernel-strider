/* State identificators for misc device */

#ifndef MISCDEV_MODEL_H
#define MISCDEV_MODEL_H

#include <linux/miscdevice.h>

/* 
 * Signal-wait id for misc device object.
 * 
 * Only pre- identificator exists, post- identificator is not needed:
 * no one signal it, because file operations may be executed after
 * device unregistered.
 */

static inline unsigned long MISCDEV_MODEL_STATE_PRE_REGISTERED(
	struct miscdevice* misc)
{
	return (unsigned long)&misc->fops;
}

#endif /* CDEV_MISCDEV_H */