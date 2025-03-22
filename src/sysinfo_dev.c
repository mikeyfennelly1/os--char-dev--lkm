/**
 * sysinfo_dev.c
 * 
 * This file handles file operations on the /dev node for the
 * sysinfo device, and other miscellaneous methods, relevant to
 * those operations.
 * 
 * @author Mikey Fennelly
 */

// headers to include
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/time.h>
#include <linux/errno.h>
#include <linux/mutex.h>

// headers from this application
#include "./procfs.h"
#include "job.h"

// ioctl definitions
#define SET_CIT_CPU 1
#define SET_CIT_MEMORY 2
#define SET_CIT_DISK 3

// device definitions
#define DEVICE_NAME "sysinfo"

// device constants
static dev_t dev_num;
static struct cdev sysinfo_cdev;
static struct class *sysinfo_dev_class;
static ktime_t start_time;
static int times_read = 0;
static bool device_open false;
static DEFINE_MUTEX(device_mutex);

// function prototypes
int __init sysinfo_cdev_init(void);
void __exit sysinfo_cdev_exit(void);
int get_times_read(void);
int get_time_since_loading_ns(void);
ssize_t sysinfo_read(struct file *filp, char __user *user_buffer, size_t count, loff_t *f_pos);

/**
 * @brief function to run when device is opened.
 * 
 * @param inode - pointer to the inode for this device.
 * 
 * @return integer status code - 0 on success, non-zero value 
 *         relevant to error otherwise
 */
static int sysinfo_open(struct inode *inode, struct file *fp)
{
    mutex_lock(&device_mutex);

    if (device_open)
    {
        // unlock the device_mutex if the device has not been reset to closed
        mutex_unlock(&device_mutex); 
        printk(KERN_WARNING "sysinfo: device already in use\n");
        return -EBUSY; // return device busy error
    }

    device_open = true;
    mutex_unlock(&device_mutex);

    printk(KERN_INFO "Device %s opened\n", DEVICE_NAME);
    return 0;
}

/**
 * @brief release the device from the userspace application.
 * 
 * @param inode - pointer to this device's inode in kernel space.
 * @param filep - release the device from the userspace application.
 * 
 * @return integer status code - 0 on success, non-zero value 
 *         relevant to error otherwise.
 */
static int sysinfo_release(struct inode *inode, struct file *filep)
{
    printk(KERN_INFO "release\n");
    return 0;
}

/**
 * @brief function to handle /dev node read.
 * 
 * @param filp - pointer to the current device file.
 * @param user_buffer - pointer in buffer in userspace to write data to on read.
 * @param count - maxumum number of bytes the function is to write to userspace.
 * @param f_pos - pointer to current position in the file.
 * 
 * @return the amount of bytes read by this device read.
 */
ssize_t sysinfo_read(struct file *filp, char __user *user_buffer, size_t count, loff_t *f_pos)
{
    times_read++; // increment the times_read counter

    // declare the number of bytes to copy to userspace
    // and a counter (bytes_copied).
    ssize_t bytes_to_copy, bytes_copied;

    // get the current job.
    Job* current_job = get_current_job();
    if (current_job == NULL)
    {
        printk(KERN_WARNING "current_job pointer is NULL\n");
        return -EFAULT;
    }

    // use the current_job to retrieve sysinfo for current moment in time.
    // store job data in character buffer.
    char* current_job_data = run_job(current_job);
    if (current_job_data == NULL)
    {
        printk(KERN_WARNING "current_job_data pointer is null\n");
        return -EFAULT;
    }
    if (strlen(current_job_data) <= 0)
    {
        printk(KERN_INFO "sysinfo device retrieved no data\n");
        return -EAGAIN;
    }

    if (*f_pos >= message_len)
        return 0;

    bytes_to_copy = min(count, (size_t)(message_len - *f_pos));

    // Copy the retrieved data to user space
    bytes_copied = copy_to_user(user_buffer, current_job_data + *f_pos, bytes_to_copy);
    if (bytes_copied)
        return -EFAULT;

    // increment the file position by bytes_to_copy
    // (i.e. the number of bytes read) and thus where 
    // this read stopped.
    *f_pos += bytes_to_copy;

    return bytes_to_copy;
}

/**
 * @brief get the number of times the /dev node has beed read
 * 
 * @return number of times the device has been read.
 */
int get_times_read(void) 
{
    return times_read;
}

/**
 * @brief get the time since loading the device in nanoseconds
 * 
 * @return number of nanoseconds since device loading
 */
int get_time_since_loading_ns(void)
{
    // get the time after this function is called
    ktime_t current_time = ktime_get();
    // calculate the delta since module load
    int time_diff = ktime_to_ns(current_time) - ktime_to_ns(start_time); 
    
    return time_diff;
}

/**
 * @brief ioctl handler, used to toggle between sysinfo modes.
 * 
 * @param file - pointer to this device structure in kernel space.
 * @param cmd - represents the type of ioctl operation to perform.
 * 
 * @return long status code - 0 on success, non-zero value 
 *         relevant to error otherwise.
 */
static long sysinfo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    switch (cmd)
    {
    case SET_CIT_CPU:
        if (set_current_info_type(CPU) < 0)
        {
            pr_info(KERN_WARNING "Failed to set current_info_type to CPU\n");
            return -EFAULT;
        }
        pr_info("current_info_type set to CPU");
        break;
    case SET_CIT_MEMORY:
        if (set_current_info_type(MEMORY) < 0)
        {
            pr_info(KERN_WARNING "Failed to set current_info_type to MEMORY\n");
            return -EFAULT;
        }
        pr_info(KERN_INFO "current_info_type set to MEMORY");
        break;
    case SET_CIT_DISK:
        if (set_current_info_type(DISK) < 0)
        {
            pr_info(KERN_WARNING "Failed to set current_info_type to DISK\n");
            return -EFAULT;
        }
        pr_info(KERN_INFO "current_info_type set to DISK");
        break;
    default:
        return -EINVAL;
    }

    return 0;
};

/* file operations available on the file */
static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = sysinfo_open,
    .release = sysinfo_release,
    .unlocked_ioctl = sysinfo_ioctl,
    .read = sysinfo_read
};

/**
 * @brief handler for event of device being loaded into kernel space.
 * 
 * @return integer status code - 0 on success, non-zero value 
 *         relevant to error otherwise.
 */
int __init sysinfo_cdev_init(void)
{
    // set the start time variable
    start_time = ktime_get();

    // variable to store return values from functions
    int err_ret;

    // allocate a character device in kernel space
    err_ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (err_ret < 0)
    {
        printk(KERN_WARNING "Failed to allocate major\n");
        return -EFAULT;
    }
    printk(KERN_INFO "Allocated Major: %d, Minor: %d\n", MAJOR(dev_num), MINOR(dev_num));

    // initialize char device
    cdev_init(&sysinfo_cdev, &fops);
    sysinfo_cdev.owner = THIS_MODULE;

    // add dev to kernel
    err_ret = cdev_add(&sysinfo_cdev, dev_num, 1);
    if (err_ret < 0)
    {
        pr_err("Failed to add cdev %s\n", err_ret);

        // unregister the character device
        unregister_chrdev_region(dev_num, 1);

        return err_ret;
    }

    sysinfo_dev_class = class_create(DEVICE_NAME);
    if (IS_ERR(sysinfo_dev_class))
    {
        pr_err("Failed to create sysinfo_dev_class\n");
        // delete the sysinfo device
        cdev_del(&sysinfo_cdev); 
        // unregister the character device by major/minor
        unregister_chrdev_region(dev_num, 1);
        
        return PTR_ERR(sysinfo_dev_class);
    }

    // create a device node in /dev directory
    if (device_create(sysinfo_dev_class, NULL, dev_num, NULL, DEVICE_NAME) == NULL)
    {
        pr_err("Failed to create device\n");
        // destroy the device class
        class_destroy(sysinfo_dev_class);
        // delete the character device
        cdev_del(&sysinfo_cdev);
        // unregister character device via major/minor
        unregister_chrdev_region(dev_num, 1);

        return -EFAULT;
    }

    // create the /proc fs on module init
    char_device_proc_init();

    printk(KERN_INFO "Sysinfo char dev initialized\n");
    return 0;
};

/**
 * @brief handler for event of devive being unloaded from kernel
 * 
 * Tears down device and device class and removed the prok file.
 * Prints to kernel logs before returning.
 */
void __exit sysinfo_cdev_exit(void)
{
    // remove the device from the kernel
    int major;
    major = MAJOR(dev_num);
    device_destroy(sysinfo_dev_class, MKDEV(major, 0)); // destroy device
    class_destroy(sysinfo_dev_class); // destroy device class
    cdev_del(&sysinfo_cdev); // delete character device
    unregister_chrdev_region(MKDEV(major, 0), 1); // unregister the major and minor number of device from kernel

    char_device_proc_exit();
    
    printk(KERN_INFO "Module unloaded\n");
    return;
}

// register the init function for the device
module_init(sysinfo_cdev_init);
// register the exit function for the device
module_exit(sysinfo_cdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Sarah McDonagh");
MODULE_AUTHOR("Danny Quinn");
MODULE_AUTHOR("Mikey Fennelly");
MODULE_DESCRIPTION("Sysinfo kernel module");