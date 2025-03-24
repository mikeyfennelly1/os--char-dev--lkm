/**
 * procfs.c
 * 
 * Functions for the /proc filesystem for this device.
 * 
 * @author Mikey Fennelly
 */

#include <linux/proc_fs.h>                  // functions for /proc file interaction
#include <linux/module.h>                   // functions for kernel module development
#include <linux/seq_file.h>                 // sequential file functions
#include <linux/kernel.h>                   // kernel development headerss
#include <linux/fs.h>                       // file system interaction headers
#include <linux/types.h>                    // types for file offsets, sizes etc

#include "sysinfo_dev.h"
#include "job.h"

#define PROC_FILE_NAME "sysinfo"
#define EOF 0
#define PROCFS_MAX_SIZE 256

ssize_t sysinfo_proc_read(struct file *file, char *user_buffer, size_t available_bytes,  loff_t *offset);
int append_to_proc(struct seq_file *m, void *v);
int my_proc_open(struct inode *inode, struct file *file);
int __init char_device_proc_init(void);
void __exit char_device_proc_exit(void);

/**
 * @brief called when userspace file reads /proc/sysinfo
 * 
 * @param file - pointer to /proc file struct in kernel space
 * @param user_buffer - buffer which the function data will be copied
 * @param available_bytes - number of bytes the reader is allowed to read
 * @param offset - offset for start of read within the file.
 *                 This is passed by the VFS layer whenever the function is called. 
 *                 Persists in the fd managed by the kernel.
 * 
 * @return size of the read in bytes
 */
ssize_t
sysinfo_proc_read(struct file *file,
                  char __user *user_buffer,
                  size_t available_bytes,
                  loff_t *offset)
{
    char* copy_target = "Something to print to read_buf\n";
    char read_buf[PROCFS_MAX_SIZE];

    // when the offset value is greater then the length of the
    // string to copy, EOF condition has been met.
    if (*offset >= strlen(copy_target))
    {
        printk("EOF condition reached\n");
        return EOF;
    }

    // write to read_buf
    // store count of bytes written in 'n'
    int n = snprintf(read_buf, PROCFS_MAX_SIZE, copy_target);
    if (n <= 0)
    {
        pr_err("Could not write data to read_buf\n");
        return -EFAULT;
    }
    int bytes_written = n;

    // copy n bytes from read_buf, starting at offset position within read_buf
    int ret = copy_to_user(user_buffer, &read_buf + *offset, n);
    if (ret != 0)
    {
        pr_err("An error occurred copying internal buffer in read() to user space buffer\n");
        return -EFAULT;
    }

    // update the offset to track read progress
    *offset += bytes_written;

    return bytes_written;
};

// structure to hold information about the proc file
struct proc_dir_entry *proc_entry;

static const struct proc_ops proc_ops = {
    .proc_read = sysinfo_proc_read,
    .proc_open = my_proc_open,
};

/**
 * @brief function to initialize /proc file
 * 
 * Creates a proc entry in kernel space. Sets permissions 
 * and operations on that file.
 * 
 * @return int status code
 */
int
__init
char_device_proc_init(void)
{
    // Create the proc file at /proc/PROC_FILE_NAME.
    // Pass NULL as pointer for parent proc directory, as this is a child file of /proc
    proc_entry = proc_create(PROC_FILE_NAME, 0666, NULL, &proc_ops);
    if (!proc_entry) {
        pr_err("Failed to create /proc/%s\n", PROC_FILE_NAME);
        // entry will fail if system runs out of memory
        return -ENOMEM;
    }

    pr_info("/proc/%s created\n", PROC_FILE_NAME);
    return 0;
};

/**
 * @brief function to remove /proc file
 */
void
__exit
char_device_proc_exit(void)
{
    // Remove the proc file
    proc_remove(proc_entry);
    pr_info("/proc/%s removed\n", PROC_FILE_NAME);
};

int
append_to_proc(struct seq_file *m,
               void *v)
{
    seq_printf(m, "initial content\n");
    seq_printf(m, "more data\n");
    return 0;
};

/**
 * @brief function to run when proc file is opened.
 * 
 * @param inode - pointer to proc file inode.
 * @param file - function to call on open of proc file.
 * 
 * @return status code
 */
int
my_proc_open(struct inode *inode,
             struct file *file)
{
    return single_open(file, append_to_proc, NULL);
}

MODULE_LICENSE("GPL");