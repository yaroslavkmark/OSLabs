    #include <stdlib.h>
    #include <unistd.h>
    #include <string.h>
    #include <sys/mman.h>

    typedef struct Block{
        int size;
        int free;
        struct Block *left,*right;
    } Block;

    typedef struct Allocator {
        Block *root;
        void *memory;
        int total_size, offset;
    } Allocator;

    int is_extent_of_two(unsigned int n) {
        return (n > 0) && ((n & (n - 1)) == 0);
    }

    Block *create_node(Allocator *allocator, int size) {
        if (allocator->offset + sizeof(Block) > allocator->total_size) {
            return NULL;
        }

        Block *node = (Block *)((char *)allocator->memory + allocator->offset);
        allocator->offset += sizeof(Block);
        node->size = size;
        node->free = 1;
        node->left = node->right = NULL;
        return node;
    }

    Allocator *allocator_create(void *mem, size_t size) {
        if (!is_extent_of_two(size)) {
            const char msg[] = "ERROR: allocator initialize a extent of two\n";
            write(STDERR_FILENO, msg, sizeof(msg) - 1);
            return NULL;
        }

        Allocator *allocator = (Allocator *)mem;
        allocator->memory = (char *)mem + sizeof(Allocator);
        allocator->total_size = size - sizeof(Allocator);
        allocator->offset = 0;

        allocator->root = create_node(allocator, size);
        if (!allocator->root) {
            return NULL;
        }

        return allocator;
    }

    void split_node(Allocator *allocator, Block *node) {
        int newSize = node->size / 2;

        node->left = create_node(allocator, newSize);
        node->right = create_node(allocator, newSize);
    }

    Block *allocate_block(Allocator *allocator, Block *node, int size) {
        if (node == NULL || node->size < size || !node->free) {
            return NULL;
        }

        if (node->size == size) {
            node->free = 0;
            return (void *)node;
        }

        if (node->left == NULL) {
            split_node(allocator, node);
        }

        void *allocated = allocate_block(allocator, node->left, size);
        if (allocated == NULL) {
            allocated = allocate_block(allocator, node->right, size);
        }

        node->free = (node->left && node->left->free) || (node->right && node->right->free);
        return allocated;
    }

    void *my_malloc(Allocator *allocator, size_t size) {
        if (allocator == NULL || size <= 0) {
            return NULL;
        }

        while (!is_extent_of_two(size)) {
            size++;
        }

        return allocate_block(allocator, allocator->root, size);
    }

    void my_free(Allocator *allocator, void *ptr) {
        if (allocator == NULL || ptr == NULL) {
            return;
        }
        Block *node = (Block *)ptr;
        if (node == NULL) {
            return;
        }

        node->free = 1;

        if (node->left != NULL && node->left->free && node->right->free) {
            my_free(allocator, node->left);
            my_free(allocator, node->right);
            node->left = node->right = NULL;
        }
    }

    void allocator_destroy(Allocator *allocator) {
        if (!allocator){
            return;
        }

        my_free(allocator, allocator->root);
        if (munmap((void *)allocator, allocator->total_size + sizeof(Allocator)) == 1) {
            exit(EXIT_FAILURE);
        }
    }