#ifndef SFS_API_H
#define SFS_API_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef FILENAME
#define FILENAME "sfs.file"
#endif

#ifndef BLOCK_SIZE
#define BLOCK_SIZE 1024
#endif

#ifndef BLOCK_NUM
#define BLOCK_NUM 100000
#endif

#ifndef INODE_NUM
#define INODE_NUM 1000
#endif

#ifndef INODE_BLOCK_NUM
#define INODE_BLOCK_NUM (INODE_NUM * sizeof(inode_t)) / BLOCK_SIZE + 1 //If size of an inode is smaller than block size, we want it to occupy a whole block
#endif

#ifndef DIR_BLOCK_NUM
#define DIR_BLOCK_NUM (INODE_NUM * sizeof(file_t)) / BLOCK_SIZE + 1
#endif


#ifndef MAX_FILE_NAME
#define MAX_FILE_NAME 32
#endif

#ifndef FREE_BIT_MAP_BLOCK_NUM
#define FREE_BIT_MAP_BLOCK_NUM (BLOCK_NUM / BLOCK_SIZE) + 1
#endif

#define MAX_BLOCKS_PER_INODE (12 + 1024 / sizeof(int))


// You can add more into this file.
typedef struct{
    uint64_t magic_num; 
    int block_size;
    int system_size;
    int inode_table_length;
    int root_dir;
} superblock_t;

typedef struct{
    int link_cnt;
    int size;
    int direct_pointers[12];
    int indirect_pointer;

} inode_t;

typedef struct{
    char filename[MAX_FILE_NAME];
    int visited;
    int available;
    int inode_num;
} file_t;

typedef struct{
    int inode_num;
    int read_write_pointer;
} fd_t;

superblock_t super_block;
inode_t inode_table[INODE_NUM];
file_t root;
file_t directory[INODE_NUM];
fd_t fd_table[INODE_NUM];
int free_bit_map[BLOCK_NUM];

void mksfs(int);

int sfs_getnextfilename(char*);

int sfs_getfilesize(const char*);

int sfs_fopen(char*);

int sfs_fclose(int);

int sfs_fwrite(int, const char*, int);

int sfs_fread(int, char*, int);

int sfs_fseek(int, int);

int sfs_remove(char*);

#endif
