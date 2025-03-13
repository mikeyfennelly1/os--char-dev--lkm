#include <stdlib.h>
#include <stdio.h>
#include <json-c/json.h>
#include <string.h>

#define INITIAL_CAPACITY 16
#define GROWTH_FACTOR 2

typedef struct {
    char *data;         // The buffer
    ssize_t size;       // the current size of the buffer
    ssize_t capacity;   // allocated capacity of the buffer
} DynamicJobBuffer;

/**
 * Initialize a DynamicJobBuffer.
 */
DynamicJobBuffer* init_job_buffer(void)
{
    DynamicJobBuffer *b = malloc(sizeof(DynamicJobBuffer));
    b->capacity = INITIAL_CAPACITY;
    b->size = 0;
    b->data = (char *)malloc(b->capacity);
    if (!b->data)
    {
        printf("Could not increase size of backing array in job buffer\n");
        exit(1);
    }
    b->data[0] = '\0';
    return b;
}

void resize_job_buffer(DynamicJobBuffer *b, size_t new_capacity)
{
    char *new_data = realloc(b->data, new_capacity);
    if (!new_data)
    {
        printf("Memory allocation failed\n");
        exit(1);
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
    free(b->data);
    b->data = NULL;
    b->size = 0;
    b-> capacity = 0;
}

/**
 * Return type for job function in snake case.
 * key_value_pair.key - the name of the metric (e.g. cpu_speed_hz)
 * key_value_pair.value - the value of that metric
 */
typedef struct key_value_pair {
    char* key;
    char* value;
} key_value_pair;

/**
 * The smallest unit of a Job.
 * Consists of a get_kvp function pointer and a pointer to the
 * next Step in the Job.
 */
typedef struct Step {
    // function to run the step
    key_value_pair (*get_kvp)(void);

    // pointer to the next step in the job
    struct Step* next;
} Step;

/**
 * A Job is composed of a title and a list steps.
 */
typedef struct Job {
    // title for the job
    char* job_title;
    
    // The 'head' step is the first step to 
    // run in the job
    Step* head;
} Job;

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
    step = (Step*) malloc(sizeof(Step));
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
    // malloc new Job.
    Job* job = (Job*)malloc(sizeof(Job));
    // if failure in malloc return NULL.
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
    Job job_to_add_to;
    job_to_add_to = *job;

    // initialize cur as job.head
    Step cur = (*job_to_add_to.head);
    while (cur.next)       // until cur.next == NULL...
        cur = (*cur.next); // ... walk the job.

    // create the step to append
    Step* p_step_to_add = step_init(get_kvp_func);
    cur.next = p_step_to_add;
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

    struct json_object *root = json_object_new_object();

    Step* cur = j->head;
    while (cur != NULL)
    {
        key_value_pair cur_kvp = cur->get_kvp();
        json_object_object_add(root, cur_kvp.key, json_object_new_string(cur_kvp.value));
        cur = cur->next;
    }

    DynamicJobBuffer* target_buf = init_job_buffer();
    append_to_job_buffer(target_buf, json_object_to_json_string_ext(root, 0));

    // Clean up the JSON object after converting it to a string
    json_object_put(root);  // This frees the JSON object memory

    return target_buf->data;
}