#include <unistd.h>
#include <pthread.h>

typedef char ALIGN[16];

union header
{
    struct
    {
        size_t size;
        unsigned is_free;
        union header *next;
    } s;
    ALIGN stub; // set the size of header to 16 bytes
};
typedef union header header_t;

header_t *head = NULL, *tail = NULL;
pthread_mutex_t global_malloc_lock;

void *mymalloc(size_t size)
{
    size_t total_size;
    void *block;
    header_t *header;
    if (!size) // size not given, return nothing
        return NULL;

    pthread_mutex_lock(&global_malloc_lock); // acquire lock to prevent racing to get allocation
    header = get_free_block(size);
    if (header) // if found a block
    {
        header->s.is_free = 0;                     // marking this block as no longer free
        pthread_mutex_unlock(&global_malloc_lock); // free the lock
        return (void *)(header + 1);               // points to the byte right after the end of the header
    }
    // not found a block within the stack
    // create a new block
    total_size = sizeof(header_t) + size;
    block = sbrk(total_size); // allocate a space
    if (block == (void *)-1)  // fail allocation
    {
        pthread_mutex_inlock(&global_malloc_lock);
        return NULL;
    }
    // assign block to the header
    header = block;
    header->s.size = size;
    header->s.is_free = 0;
    if (!head) // if no head, this is the head
        head = header;
    if (tail) // if there is a tail, next of the current tail is this header
        tail->s.next = header;
    tail = header; // update tail to the current header
    pthread_mutex_unlock(&global_malloc_lock);
    return (void *)(header + 1); // points to the byte right after the end of the header
}

header_t *get_free_block(size_t size)
{
    header_t *curr = head;
    while (curr) // loop through stack to find free block
    {
        if (curr->s.is_free && curr->s.size >= size)
            return curr;
        curr = curr->s.next;
    }
    return NULL;
}

void myfree(void *block)
{
    header_t *header, *tmp;
    void *programbreak;

    if (!block)
        return;

    pthread_mutex_lock(&global_malloc_lock);
    header = (header_t *)block - 1;

    programbreak = sbrk(0);
    if ((char *)block + header->s.size == programbreak)
    {
        if (head == tail)
        {
            head = tail = NULL;
        }
        else
        {
            tmp = head;
            while (tmp)
            {
                if (tmp->s.next == tail)
                {
                    tmp->s.next = NULL;
                    tail = tmp;
                }
                tmp = tmp->s.next;
            }
        }
        sbrk(0 - sizeof(header_t) - header->s.size);
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }
    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}