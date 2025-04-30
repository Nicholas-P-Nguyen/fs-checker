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

typedef struct {
    superblock *sb;
    void *file_image;
    dinode *inodes;
} filecheck;

int valid_inode(u16 inode_t) {
    return inode_t == 0 || inode_t == T_DIR || inode_t == T_FILE || inode_t == T_DEV;
}

// checking bitmap, if inode allocated, but marked free, return false, else true
int check_inode_bitmap(u32 data_block, filecheck fc) {
    u8 *bitmap = (u8 *)(fc.file_image + (BSIZE * BMAPSTART));
    u32 byte_index = data_block / 8;
    u32 bit_offset = data_block % 8;
    u8 byte = bitmap[byte_index];
    return (byte & (1 << bit_offset)) != 0;
}

void valid_direct_block(dinode *inode, filecheck fc) {
    for (int i = 0; i < NDIRECT; i++) {
        u32 data_block = inode->addrs[i];
        if (data_block == 0) {
            continue;
        }
        if (data_block < DATASTART || data_block >= fc.sb->size) {
            fprintf(stderr, "ERROR: bad direct address in inode.\n");
            exit(1);
        }

        if (!check_inode_bitmap(data_block, fc)) {
            fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
            exit(1);
        }

    }
    return;
}

void valid_indirect_block(u32 *indirect_block, filecheck fc) {
    for (int i = 0; i < NINDIRECT; i++) {
        u32 data_block = indirect_block[i];
        if (data_block == 0) {
            continue;
        }
        if (data_block < DATASTART || data_block >= fc.sb->size) {
            fprintf(stderr, "ERROR: bad indirect address in inode.\n");
            exit(1);
        }

        if (!check_inode_bitmap(data_block, fc)) {
            fprintf(stderr, "ERROR: address used by inode but marked free in bitmap.\n");
            exit(1);
        }
    }
    return;
}

int check_root(dinode *root, void *file_img) {
    if (root->type == 0 || root->type != T_DIR) {
        return 0;
    }

    dirent *root_dir = (dirent *)(file_img + (root->addrs[0] * BSIZE));
    
    for (int i = 0; i < BSIZE / sizeof(dirent); i++) {
        // parent directory
        if (strcmp(root_dir[i].name, "..") == 0) {
            if (root_dir[i].inum != 1) {
                return 0;
            }
        }
    }
    return 1;
}

void check_directory_format(dirent *dir_entry, u32 dir_inode) {
    int seen_curr = 0;
    int seen_parent = 0;
    for (int i = 0; i < BSIZE / sizeof(dirent); i++) {
        // current directory
        if (strncmp(dir_entry[i].name, ".", DIRSIZ) == 0) {
            seen_curr = 1;
            if (dir_entry[i].inum != dir_inode) {
                fprintf(stderr, "ERROR: directory not properly formatted.\n");
                exit(1);
            }
        }
        // parent directory
        if (strncmp(dir_entry[i].name, "..", DIRSIZ) == 0) {
            seen_parent = 1;
        }
    }

    if (!seen_curr || !seen_parent) {
        fprintf(stderr, "ERROR: directory not properly formatted.\n");
        exit(1);
    }
    return;
}

void check_inodes(filecheck fc) {
    for (int i = 0; i < fc.sb->ninodes; i++) {
        dinode *inode = &fc.inodes[i];
        if (!valid_inode(inode->type)) {
            fprintf(stderr, "ERROR: bad inode.\n");
            exit(1);
        }
        // unallocated inode
        if (inode->type == 0) {
            continue;
        }
    
        // checking direct block
        valid_direct_block(inode, fc);

        // checking indirect block
        u32 indirect_block_num = inode->addrs[NDIRECT];
        u32 *indirect_block = (u32 *)(fc.file_image + (BSIZE * indirect_block_num));
        valid_indirect_block(indirect_block, fc);

        // checking directory format
        if (inode->type == T_DIR) {
            if (inode->addrs[0] == 0) {
                continue;
            }
            dirent *dir_entry = (dirent *)(fc.file_image + (inode->addrs[0] * BSIZE));
            check_directory_format(dir_entry, i);
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

    filecheck fc;
    fc.file_image = file_img;
    fc.sb = sb;
    fc.inodes = inodes;
    
    dinode *root = &inodes[1];
    if (!check_root(root, file_img)) {
        fprintf(stderr, "ERROR: root directory does not exist.\n");
        exit(1);
    }

    check_inodes(fc);




    // Didn't detect a problem, don't print anything.
    munmap(file_img, st.st_size);
    close(fd);
    exit(0);
}

