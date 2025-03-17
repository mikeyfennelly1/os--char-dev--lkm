#include <linux/kernel.h>
#include <linux/module.h>
#include "cpu.h"
#include "memory.h"
#include "disk.h"

// definitions for backing array for job
#define INITIAL_CAPACITY 16
#define GROWTH_FACTOR 2

// definitions for info types
#define CPU 1
#define MEMORY 2
#define DISK 3

static int current_info_type = 1;

typedef struct {
    char *data;         // The buffer
    ssize_t size;       // the current size of the buffer
    ssize_t capacity;   // allocated capacity of the buffer
} DynamicJobBuffer;

DynamicJobBuffer* init_job_buffer(void);
void resize_job_buffer(DynamicJobBuffer *b, size_t new_capacity);
void append_to_job_buffer(DynamicJobBuffer *b, const char* text);
void free_job_buffer(DynamicJobBuffer *b);
Step* step_init(key_value_pair (*get_kvp)(void));

/**
 * Initialize a DynamicJobBuffer.
 */
DynamicJobBuffer* init_job_buffer(void)
{
    DynamicJobBuffer *b = kmalloc(sizeof(DynamicJobBuffer), GFP_KERNEL);
    b->capacity = INITIAL_CAPACITY;
    b->size = 0;
    b->data = (char *)kmalloc(b->capacity, GFP_KERNEL);
    if (!b->data)
    {
        pr_err("Could not increase size of backing array in job buffer\n");
        return NULL;
    }
    b->data[0] = '\0';
    return b;
}

void resize_job_buffer(DynamicJobBuffer *b, size_t new_capacity)
{
    char *new_data = krealloc(b->data, new_capacity, GFP_KERNEL);
    if (!new_data)
    {
        pr_err("Memory allocation failed resizing job buffer\n");
        return;
    }
    b->data = new_data;
    b->capacity = new_capacity;
}

void append_to_job_buffer(DynamicJobBuffer *b, const char* text)
{
    size_t text_len = strlen(text);

    if (b->size + text_len + 1 > b->capacity)
    {
        size_t required_capacity = b->size + text_len + 1;
        size_t new_capacity = b->capacity * GROWTH_FACTOR;
        while (new_capacity < required_capacity)
        {
            new_capacity *= GROWTH_FACTOR;
        }
        resize_job_buffer(b, new_capacity);
    }

    memcpy(b->data + b->size, text, text_len);
    b->data[b->size + text_len] = '\0';
    b->size += text_len;
}

void free_job_buffer(DynamicJobBuffer *b)
{
    kfree(b->data);
    b->data = NULL;
    b->size = 0;
    b-> capacity = 0;
}

/**
 * Initialize a step with Step.next==NULL
 *
 * @param get_kvp the get_kvp function to initialize the step.
 * @return Step* - pointer to the initialized step.
 * @return NULL if step init failed.
 */
Step* step_init(key_value_pair (*get_kvp)(void))
{
    Step* step;
    step = (Step*) kmalloc(sizeof(Step), GFP_KERNEL);
    if (step == NULL)
        return NULL;
    step->next = NULL;
    step->get_kvp = get_kvp;
    return step;
}

/**
 * Initialize a job, by title and first function
 * to run in the job.
 *
 * @param title The title of the Job to initialize.
 * @param head_func the function for the first step in the Job.
 * @return a pointer to the initialized Job,
 *         NULL if job_init failed
 */
Job* job_init(char* title, key_value_pair (*head_func)(void))
{
    // kmalloc new Job.
    Job* job = (Job*)kmalloc(sizeof(Job), GFP_KERNEL);
    // if failure in kmalloc return NULL.
    if (job == NULL)
        return NULL;

    // set title of job.
    job->job_title = title;

    // initialize step for head_step of job.
    Step* head_step = step_init(head_func);
    if (head_step == NULL)
        return NULL;

    // set job.head to head_step.
    job->head=head_step;

    job->step_count = 1;
    return job;
}

/**
 * Add a step to the job.
 * 
 * @param job - the job to add the function pointer to.
 * @param get_kvp_func - the function for the step to add
 */
void append_step_to_job(Job* job, key_value_pair (*get_kvp_func)(void))
{
    // initialize cur as job.head
    Step* cur = job->head;

    while (cur->next != NULL)       // until cur.next is a NULL pointer
    {
        printk("Walking job list. Current KVP key: %s\n", cur->get_kvp().key);
        cur = cur->next;            // ... walk the job.
    }
    
    if (get_kvp_func == NULL)
    {
        printk("get_kvp_func == NULL\n");
        return;
    }

    // create the step to append
    Step* p_step_to_add = step_init(get_kvp_func);
    if (p_step_to_add == NULL)
    {
        printk(KERN_ERR "Pointer to step to add is NULL\n");
        return;
    }
    cur->next = p_step_to_add;

    if (cur->next == NULL)
    {
        printk("End of the list reached.\n");
        printk("kvp_of cur (expected to be last) is: %s:%s.\n", cur->get_kvp().key, cur->get_kvp().value);
    }


    job->step_count++;
    return;
}

/**
 * Runs the job (gets the key-value information for each step)
 * and writes the contents to a buffer 'target_buf'.
 *
 * @param j - pointer to the job to run.
 * @return string buffer that contains job data in key-value form.
 *
 * WARNING: It is the responsibility of the caller to free
 * the memory of the returned buffer.
 */
char* run_job(Job* j)
{
    if (j == NULL)
    {
        return NULL;
    }

    DynamicJobBuffer* target_buf = init_job_buffer();

    int run_count = 0;

    Step* cur = j->head;
    while (cur != NULL)
    {
        key_value_pair cur_kvp = cur->get_kvp();
        append_to_job_buffer(target_buf, cur_kvp.key);
        append_to_job_buffer(target_buf, cur_kvp.value);
        run_count++;
        cur = cur->next;
    }

    printk("run_job, target_buf->data: \n %s\n", target_buf->data);
    printk("run_count: %d\n", run_count);

    return target_buf->data;
}

/**
 * Get a pointer to the job for the current_info_type.
 * 
 * @return pointer to the job for current_info_type.
 */
Job* get_current_job(void)
{
    Job* current_job;
    
    switch (current_info_type)
    {
        case CPU:
            current_job = get_cpu_job();
            break;
        case MEMORY:
            current_job = get_memory_job();
            break;
        case DISK:
            current_job = get_disk_job();
            break;
        default:
            current_job = get_cpu_job();
    }
    return current_job;
}

/**
 * Set the value of the current info type to 1 of
 * 3 values:
 * 
 * 1. CPU
 * 2. MEMORY
 * 3. DISK
 * 
 * @return 0 if success, -1 on error.
 */
int set_current_info_type(int value)
{
    if (value <=3 && value >=1)
    {
        current_info_type = value;
        return 0;
    }
    else
    {
        return -1;
    }
}

MODULE_LICENSE("GPL");