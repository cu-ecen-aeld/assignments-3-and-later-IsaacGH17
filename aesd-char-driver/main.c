/**
 * @file aesdchar.c
 * @brief Functions and data related to the AESD char driver implementation
 *
 * Based on the implementation of the "scull" device driver, found in
 * Linux Device Drivers example code.
 *
 * @author Dan Walkes
 * @date 2019-10-22
 * @copyright Copyright (c) 2019
 *
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/printk.h>
#include <linux/types.h>
#include <linux/cdev.h>
#include <linux/fs.h> // file_operations
#include <linux/string.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/uaccess.h>
#include <linux/version.h> // LINUX_VERSION_CODE para el guard de class_create
#include "aesdchar.h"
int aesd_major =   0; // use dynamic major
int aesd_minor =   0;

static struct class *aesd_class = NULL;

MODULE_AUTHOR("IsaacGH"); /** TODO: fill in your name **/
MODULE_DESCRIPTION("AESD char driver");
MODULE_LICENSE("Dual BSD/GPL");

struct aesd_dev aesd_device;

static int aesd_open(struct inode *inode, struct file *filp)
{
    PDEBUG("open");
    struct aesd_dev *dev;
    dev = container_of(inode->i_cdev, struct aesd_dev, cdev);
    filp->private_data = dev; 
    return 0;
}

static int aesd_release(struct inode *inode, struct file *filp)
{
    PDEBUG("release");
    return 0;
}

static ssize_t aesd_read(struct file *filp, char __user *buf, size_t count, loff_t *f_pos){
    ssize_t retval = 0;
    struct aesd_buffer_entry *entry = NULL;
    size_t entry_offset = 0;
    size_t bytes_to_copy = 0;
    PDEBUG("read %zu bytes with offset %lld",count,*f_pos);
    struct aesd_dev *dev = filp->private_data;
    if(mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    entry = aesd_circular_buffer_find_entry_offset_for_fpos(&dev->buffer, *f_pos, &entry_offset);
    if(entry == NULL){
        mutex_unlock(&dev->lock);
        return 0;
    }
    bytes_to_copy = entry->size - entry_offset;
    if (bytes_to_copy > count) {
        bytes_to_copy = count;
    }
    if (copy_to_user(buf, entry->buffptr + entry_offset, bytes_to_copy)) {
        retval = -EFAULT;
    } else {
        *f_pos += bytes_to_copy;
        retval = bytes_to_copy;
    }
    mutex_unlock(&dev->lock);
        return retval;
}

static ssize_t aesd_write(struct file *filp, const char __user *buf, size_t count,
                loff_t *f_pos)
{
    ssize_t retval = -ENOMEM;
    char *kbuf;
    char *newline;
    size_t uncopied_bytes;
    size_t new_size;
    PDEBUG("write %zu bytes with offset %lld",count,*f_pos);
     struct aesd_dev *dev = filp->private_data;
    if(mutex_lock_interruptible(&dev->lock))
        return -ERESTARTSYS;
    kbuf = kmalloc(count, GFP_KERNEL);
    if (!kbuf) {
        mutex_unlock(&dev->lock);
        return -ENOMEM;
    } 
    uncopied_bytes = copy_from_user(kbuf, buf, count);
    if (uncopied_bytes != 0) {
        kfree(kbuf);
        mutex_unlock(&dev->lock);
        return -EFAULT;
    }
    new_size = dev->entry.size + count;
    char *new_buffptr = krealloc(dev->entry.buffptr, new_size, GFP_KERNEL);
    if (!new_buffptr) {
        kfree(kbuf);
        mutex_unlock(&dev->lock);
        return -ENOMEM;
    }
    memcpy(new_buffptr + dev->entry.size, kbuf, count);
    dev->entry.buffptr = new_buffptr;
    dev->entry.size = new_size;
    kfree(kbuf);
    newline = memchr(dev->entry.buffptr, '\n',new_size);
    if (newline) {
        /* When the circular buffer is full, adding a new entry overwrites the
         * oldest one.  This driver owns that memory, so free it before the
         * add_entry call (add_entry returns void by design). */
        if (dev->buffer.full &&
            dev->buffer.entry[dev->buffer.out_offs].buffptr != NULL) {
            kfree((char *)dev->buffer.entry[dev->buffer.out_offs].buffptr);
        }
        aesd_circular_buffer_add_entry(&dev->buffer, &dev->entry);
        dev->entry.buffptr = NULL;
        dev->entry.size = 0;
    }
    retval = count;
    mutex_unlock(&dev->lock);
    return retval;
}
struct file_operations aesd_fops = {
    .owner =    THIS_MODULE,
    .read =     aesd_read,
    .write =    aesd_write,
    .open =     aesd_open,
    .release =  aesd_release,
};

static int aesd_setup_cdev(struct aesd_dev *dev)
{
    int err, devno = MKDEV(aesd_major, aesd_minor);

    cdev_init(&dev->cdev, &aesd_fops);
    dev->cdev.owner = THIS_MODULE;
    dev->cdev.ops = &aesd_fops;
    err = cdev_add (&dev->cdev, devno, 1);
    if (err) {
        printk(KERN_ERR "Error %d adding aesd cdev", err);
    }
    return err;
}



static int aesd_init_module(void)
{
    dev_t dev = 0;
    int result;
    result = alloc_chrdev_region(&dev, aesd_minor, 1,
            "aesdchar");
    aesd_major = MAJOR(dev);
    if (result < 0) {
        printk(KERN_WARNING "Can't get major %d\n", aesd_major);
        return result;
    }
    memset(&aesd_device,0,sizeof(struct aesd_dev));

    mutex_init(&aesd_device.lock);
    aesd_circular_buffer_init(&aesd_device.buffer);
    result = aesd_setup_cdev(&aesd_device);

    if (result) {
        unregister_chrdev_region(dev, 1);
        return result;
    }

    /* Crear /dev/aesdchar vía devtmpfs/udev: sin class_create el nodo
     * no existe en sistemas donde nadie hace mknod (p.ej. Yocto).
     * La firma de class_create cambió en el kernel 6.4. */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,4,0)
    aesd_class = class_create("aesdchar");
#else
    aesd_class = class_create(THIS_MODULE, "aesdchar");
#endif
    if (IS_ERR(aesd_class)) {
        result = PTR_ERR(aesd_class);
        printk(KERN_ERR "Error %d creating aesd class\n", result);
        cdev_del(&aesd_device.cdev);
        unregister_chrdev_region(dev, 1);
        return result;
    }
    device_create(aesd_class, NULL, dev, NULL, "aesdchar");

    return result;
}

static void aesd_cleanup_module(void)
{
   dev_t devno = MKDEV(aesd_major, aesd_minor);
    struct aesd_buffer_entry *entry = NULL;
    uint8_t index = 0;
    if (aesd_class) {
        device_destroy(aesd_class, devno);
        class_destroy(aesd_class);
        aesd_class = NULL;
    }
    cdev_del(&aesd_device.cdev);
    AESD_CIRCULAR_BUFFER_FOREACH(entry, &aesd_device.buffer, index) {
        if (entry->buffptr != NULL) {
            kfree((char *)entry->buffptr);
            entry->buffptr = NULL;
        }
    }
    if (aesd_device.entry.buffptr != NULL) {
        kfree((char *)aesd_device.entry.buffptr);
        aesd_device.entry.buffptr = NULL;
    }
    mutex_destroy(&aesd_device.lock);   
    unregister_chrdev_region(devno, 1);
}



module_init(aesd_init_module);
module_exit(aesd_cleanup_module);
