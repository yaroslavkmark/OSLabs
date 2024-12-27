#include <stdlib.h>
#include <math.h>
#include <string.h>
#include <unistd.h>
#include <sys/mman.h>
#include <unistd.h>

#define MAX_BLOCK_SIZE_EXTENT 10  // Максимальный размер блока (2^10 = 1024)
#define MIN_BLOCK_SIZE_EXTENT 0    // Минимальный размер блока (2^0 = 1)
#define NUM_LISTS 11          // Количество списков (от 0 до 10)


typedef struct Block {
    struct Block *next;
    struct Block *prev;
    size_t size;
} Block;

typedef struct Allocator {
    Block* freeLists[NUM_LISTS];
    void *memory;
    size_t total_size;
} Allocator;

int power(int base, int exp) {
    long long result = 1;
    while (exp > 0) {
        if (exp % 2 == 1) {
            result *= base;
        }
        base *= base;
        exp /= 2;
    }
    return result;
}

Allocator* allocator_create(void* mem, size_t size) {

    Allocator* allocator = (Allocator*)mem;
    allocator->total_size = size - sizeof(Allocator);
    allocator->memory = (char*)mem + sizeof(Allocator);

    size_t offset = 0;

    size_t extent = 0;
    for (int i = 0; i < 10; ++i) {
        allocator->freeLists[i] = NULL;
    }
    while (offset + power(2, extent / 10) <= allocator->total_size) {
        Block *block = (Block *)((char *)allocator->memory + offset);
        if (allocator->freeLists[extent / 10] == NULL) {
            block->next = NULL;
            allocator->freeLists[extent / 10] = block;
        } else {
            allocator->freeLists[extent / 10]->prev = block;
        }
        block->next = allocator->freeLists[extent / 10];
        block->size = power(2, extent / 10);
        offset += power(2, extent / 10);
        extent++;
    }

    return allocator;
}

void split_block(Allocator *allocator, Block* block) {
    int index = 0;
    while ((1 << index) < block->size) {
        index++;
    }
    Block *block_copy = (Block *)((char *)allocator->memory + block->size / 2);
    if (allocator->freeLists[index] == NULL) {
        block->next = NULL;
        allocator->freeLists[index] = block;
        allocator->freeLists[index]->prev = block_copy;

    } else {
        allocator->freeLists[index]->prev = block;
        block->next = allocator->freeLists[index];
        allocator->freeLists[index]->prev = block_copy;
        block_copy->next = allocator->freeLists[index];
    }
    block->size = power(2, index);
    block_copy->size = power(2, index);
}

void* my_malloc(Allocator *allocator, size_t size) {
    int index = 0;
    while ((1 << index) < size) {
        index++;
    }

    if (index >= NUM_LISTS) return NULL;

    if (allocator->freeLists[index] != NULL) {
        Block *block = allocator->freeLists[index];
        allocator->freeLists[index] = block->next;
        return block;
    }

    for (int i = 0; i < 10 - index; ++i) {
        if (allocator->freeLists[index + i] != NULL) {
            int j = 0;
            while (i != j) {
                Block *block = allocator->freeLists[index + i - j];
                allocator->freeLists[index + i - j] = block->next;
                split_block(allocator, block);
            }

            break;
        }
    }

    return NULL;
}

void my_free(Allocator *allocator, void *ptr) {

    if (allocator == NULL || ptr == NULL)
    {
        return;
    }

    int index = 0;
    while ((1 << (index + 4)) < ((Block*)ptr)->size) {
        index++;
    }

    allocator->freeLists[index] = ((Block*)ptr)->next;
    ptr = NULL;
}

void allocator_destroy(Allocator *allocator) {
    if (!allocator)
    {
        return;
    }

    if (munmap((void *)allocator, allocator->total_size + sizeof(Allocator)) == 1)
    {
        exit(EXIT_FAILURE);
    }
}

