/**
 * Sysinfo character device
 * 
 * A kernel module to provide system information.
 * 
 * @author Sarah McDonagh, Danny Quinn, Mikey Fennelly
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/file.h>
#include <linux/uaccess.h>
#include <linux/fs.h>
#include <linux/kernel.h>
#include <linux/cdev.h>
#include <linux/device.h>
#include <linux/version.h>
#include <linux/proc_fs.h>

#define MY_IOCTL_MAGIC 'M'
#define MY_IOCTL_CMD_1 _IO(MY_IOCTL_MAGIC, 1)
#define MY_IOCTL_CMD_2 _IOR(MY_IOCTL_MAGIC, 2, int)
#define MY_IOCTL_CMD_3 _IOW(MY_IOCTL_MAGIC, 3, int)

#define DEVICE_NAME "sysinfo_cdev"

static dev_t dev_num;
static struct cdev sysinfo_cdev;
static struct class *sysinfo_dev_class;

static int sysinfo_open(struct inode *inode, struct file *fp)
{
    printk(KERN_INFO "Device %s opened\n", DEVICE_NAME);
    return 0;
}

static int sysinfo_release(struct inode *inode, struct file *filep)
{
    printk(KERN_INFO "release\n");
    return 0;
}

static long sysinfo_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int value;

    switch (cmd)
    {
        case MY_IOCTL_CMD_1:
            pr_info("IOCTL command 1 executed\n");
            break;
        case MY_IOCTL_CMD_2:
            value = 1234;
            if (copy_to_user((int __user *)arg, &value, sizeof(value)))
            {
                return -EFAULT;
            }
            pr_info("IOCTL command 2 executed, received: %d\n", value);
            break;
        case MY_IOCTL_CMD_3:
            if (copy_from_user(&value, (int __user*)arg, sizeof(value)))
            {
                return -EFAULT;
            }
            pr_info("IOCTL command 3 executed, received%d\n", value);
            break;
        
        default:
            return -EINVAL;
    }

    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = sysinfo_open,
    .release = sysinfo_release,
    .unlocked_ioctl = sysinfo_ioctl
};

static struct proc_dir_entry *proc_entry;

static int __init sysinfo_cdev_init(void)
{
    int ret;
    
    ret = alloc_chrdev_region(&dev_num, 0, 1, DEVICE_NAME);
    if (ret < 0)
    {
        printk(KERN_INFO "Failed to allocate major\n");
        return ret;
    }
    printk(KERN_INFO "Allocated Major: %d, Minor: %d\n", MAJOR(dev_num), MINOR(dev_num));

    // initialize char device
    cdev_init(&sysinfo_cdev, &fops);
    sysinfo_cdev.owner = THIS_MODULE;

    // add dev to kernel
    ret = cdev_add(&sysinfo_cdev, dev_num, 1);
    if (ret < 0)
    {
        pr_err("Failed to add cdev\n");
        unregister_chrdev_region(dev_num, 1);
        return ret;
    }

    sysinfo_dev_class = class_create(DEVICE_NAME);
    if (IS_ERR(sysinfo_dev_class))
    {
        printk(KERN_INFO "Failed to create sysinfo_dev_class\n");
        cdev_del(&sysinfo_cdev);
        unregister_chrdev_region(dev_num, 1);
        return PTR_ERR(sysinfo_dev_class);
    }

    // create a device node in /dev directory
    if (device_create(sysinfo_dev_class, NULL, dev_num, NULL, DEVICE_NAME) == NULL)
    {
        printk(KERN_INFO "Failed to create device\n");
        class_destroy(sysinfo_dev_class);
        cdev_del(&sysinfo_cdev);
        unregister_chrdev_region(dev_num, 1);
        return -1;
    }

    proc_entry = proc_create("my_proc_file", 0666, NULL, &proc_fops);
    if (!proc_entry) {
        pr_err("Failed to create proc file\n");
        return -ENOMEM;
    }

    pr_info("/proc/my_proc_file created\n");

    printk(KERN_INFO "Sysinfo char dev initialized\n");
    return 0;
};

static void __exit sysinfo_cdev_exit(void)
{
    static int major;
    major = MAJOR(dev_num);
    device_destroy(sysinfo_dev_class, MKDEV(major, 0));
    class_destroy(sysinfo_dev_class);
    cdev_del(&sysinfo_cdev);
    unregister_chrdev_region(MKDEV(major, 0), 1);
    printk(KERN_INFO "Module unloaded\n");
};

module_init(sysinfo_cdev_init);
module_exit(sysinfo_cdev_exit);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Mikey Fennelly");
MODULE_DESCRIPTION("Sysinfo kernel module");