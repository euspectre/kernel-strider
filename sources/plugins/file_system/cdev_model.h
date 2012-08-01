/* State identificators for character device */

#ifndef CDEV_MODEL_H
#define CDEV_MODEL_H

#include <linux/cdev.h>

/* 
 * Signal-wait id for character device object.
 * 
 * Only pre- identificator exists, post- identificator is not needed:
 * no one signal it, because file operations may be executed after
 * device unregistered.
 */

static inline unsigned long CDEV_MODEL_STATE_PRE_REGISTERED(
	struct cdev* dev)
{
	return (unsigned long)&dev->kobj;
}

#endif /* CDEV_MODEL_H */