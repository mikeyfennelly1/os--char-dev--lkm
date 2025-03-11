#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#define INITIAL_CAPACITY 16
#define GROWTH_FACTOR 2

typedef struct {
    char *data;         // The buffer
    ssize_t size;       // the current size of the buffer
    ssize_t capacity;   // allocated capacity of the buffer
} DynamicJobBuffer;

void init_job_buffer(DynamicJobBuffer *b)
{
    b->capacity = INITIAL_CAPACITY;
    b->size = 0;
    b->data = (char *)kmalloc(b->capacity);
    if (!b->data)
    {
        printk(KERN_ERR "Could not increase size of backing array in job buffer\n");
        exit(1);
    }
    b->data[0] = '\0';
}

void resize_job_buffer(DynamicJobBuffer *b, size_t new_capacity)
{
    char *new_data = (char *)realloc(b->data, new_capacity);
    if (!new_data)
    {
        printk(KERN_ERR "Memory allocation failed\n");
        exit(1);
    }
    b->data = new_data;
    b-> capacity = new_capacity;
}

void append_to_job_buffer(DynamicJobBuffer *b, const char* text)
{
    size_t text_len = strlen(text);

    if (b->size + text_len + 1 > b->capacity)
    {
        size_t new_capacity = b->capacity * GROWTH_FACTOR;
        while (new_capacity < b->size + text_len + 1)
        {
            new_capacity *= GROWTH_FACTOR;
        }
        resize_job_buffer(b, new_capacity);
    }

    strcpy(b->data + b->size, text);
    b->size += text_len;
}

void free_job_buffer(DynamicJobBuffer *b)
{
    kfree(b->data);
    b->data = NULL;
    b->size = 0;
    b-> capacity = 0;
}

int main() {
    DynamicJobBuffer buffer;
    init_job_buffer(&buffer);

    append_to_job_buffer(&buffer, "Hello, ");
    append_to_job_buffer(&buffer, "world!");
    append_to_job_buffer(&buffer, " How are you?");

    printf("Buffer content: %s\n", buffer.data);
    
    free_job_buffer(&buffer);
    return 0;
}