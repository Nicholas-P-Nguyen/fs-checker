#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdbool.h>

#include "xcheck.h"

int valid_inode(u16 inode_t) {
    return inode_t == 0 || inode_t == T_DIR || inode_t == T_FILE || inode_t == T_DEV;
}

int valid_direct_block(superblock *sb ,dinode *inode) {
    int rc = 1;
    for (int i = 0; i < NDIRECT; i++) {
        u32 block_addr = inode->addrs[i];
        if (block_addr == 0) {
            continue;
        }
        if (block_addr < DATASTART || block_addr >= sb->size) {
            rc = 0;
        }

    }
    return rc;
}

void check_inodes(superblock *sb ,dinode *inodes) {
    for (int i = 0; i < sb->ninodes; i++) {
        dinode *inode = &inodes[i]; 
        if (!valid_inode(inode->type)) {
            fprintf(stderr, "ERROR: bad inode.\n");
            exit(1);
        }
        if (inode->type == 0) {
            continue;
        }

        // checking direct block
        if (!valid_direct_block(sb ,inode)) {
            fprintf(stderr, "ERROR: bad direct address in inode.\n");
            exit(1);
        }



    }
    return;

}


int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "Usage: xcheck <file_system_image>\n");
        exit(1);
    }

    int fd;
    struct stat st;

    fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        fprintf(stderr, "image not found.\n");
        close(fd);
        exit(1);
    }

    if (fstat(fd, &st) == -1) {
        perror("Failed to get size of file system image.\n");
        close(fd);
        exit(1);
    }

    void *file_img = mmap(NULL, st.st_size, PROT_READ, MAP_PRIVATE, fd, 0);
    if (file_img == MAP_FAILED) {
        perror("mmap failed\n");
        close(fd);
        exit(1);
    }

    // superblock is in block 1
    superblock *sb = (superblock *) (file_img + BSIZE);
    // inode blocks starts at block 32
    dinode *inodes = (dinode *) (file_img + (BSIZE * INODESTART));
    

    check_inodes(sb, inodes);




    // Didn't detect a problem, don't print anything.
    munmap(file_img, st.st_size);
    close(fd);
    exit(0);
}

