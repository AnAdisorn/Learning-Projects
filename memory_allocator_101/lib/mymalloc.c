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

    if (!block) // no block, do nothing
        return;

    pthread_mutex_lock(&global_malloc_lock);
    // get the header of the block
    // by getting a pointer that is behind the block by a distance equalling the size of the header.
    header = (header_t *)block - 1;

    programbreak = sbrk(0);
    // check if the block to be freed is at the end of the heap
    if ((char *)block + header->s.size == programbreak)
    {
        if (head == tail)
        {
            head = tail = NULL;
        }
        else // while loop to set block to NULL, from head to tail
        {
            tmp = head;
            while (tmp)
            {
                if (tmp->s.next == tail) // next one is tail
                {
                    tmp->s.next = NULL; // set next to NULL, this will break while loop in the next loop
                    tail = tmp;         // this tmp block is the new tail
                }
                tmp = tmp->s.next; // move to next
            }
        }
        sbrk(0 - sizeof(header_t) - header->s.size); // release the memory
        pthread_mutex_unlock(&global_malloc_lock);
        return;
    }
    header->s.is_free = 1;
    pthread_mutex_unlock(&global_malloc_lock);
}

// allocate memory for an array of num elements of nsize bytes each and returns a pointer to the allocated memory
void *mycalloc(size_t num, size_t nsize)
{
    size_t size;
    void *block;
    if (!num || !nsize)
        return NULL;
    size = num * nsize;
    // check mul overflow
    if (nsize != size / num)
        return NULL;
    block = mymalloc(size);
    if (!block)
        return NULL;
    memset(block, 0, size); // clear the allocated memory to all zero
    return block;
}

// changes the size of the given memory block to the given size
void *myrealloc(void *block, size_t size)
{
    header_t *header;
    void *ret;
    if (!block || !size)
        return malloc(size);
    header = (header_t *)block - 1;
    if (header->s.size >= size)
        return block;
    ret = malloc(size);
    if (ret)
    {

        memcpy(ret, block, header->s.size); // relocate contents to the new bigger block
        free(block);                        // free the old memory block
    }
    return ret;
}