#include "disk_emu.h"
#include "sfs_api.h"

//reset the content of the block
void release_block(void* block)
{
  for (int i = 0; i < BLOCK_SIZE; i++) {
    *(int*)(block + i) = 0;
  }
}

//initialize the super block
void make_sb() {
    super_block.magic_num = 0xACBD0005;
    super_block.block_size = BLOCK_SIZE;
    super_block.system_size = BLOCK_SIZE * BLOCK_NUM;
    super_block.inode_table_length = INODE_BLOCK_NUM;
    super_block.root_dir = 0;
  
}

//initialize inode table
void make_inode_table() {
    for (int i = 0; i < INODE_NUM; i++) {
        inode_table[i].link_cnt = 0;
        inode_table[i].size = 0;
        inode_table[i].indirect_pointer = 0;
    }
}

//initialize root directory
void make_root_dir() {
    strcpy(root.filename, ".");
    root.inode_num = 0;
    root.visited = 0;
    root.available = 0;
    directory[0] = root;
    inode_table[0].link_cnt = 1;
    inode_table[0].size = sizeof(root);


}

//find the next available inode 
int get_free_inode() {
    for (int i = 0; i < INODE_NUM; i++) {
        if (inode_table[i].link_cnt == 0) {

            return i;
        }
    }
    return -1;
}

//find the next free file descriptor
int get_free_fd() {
    for (int i = 0; i < INODE_NUM; i++) {
        if (fd_table[i].inode_num == 0) {
            return i;
        }
    }
    return -1;
}

//get the next free block
int get_free_block() {
    for (int i = 0; i < BLOCK_NUM; i++)
    {
        if (free_bit_map[i] == 1) {
            free_bit_map[i] = 0;
            write_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);
            return i;
        }
       
    }
  
    return -1;
}

//find the file descriptor of this inode
int get_fd(int inode) {
    if (inode == -1) {
        return -1;
    }
    for (int i = 0; i < INODE_NUM; i++) {
        if (fd_table[i].inode_num == inode) {
            return i;
        }
    }

    return -1;
}

// void write_inode_table() {
//     write_blocks(1, INODE_BLOCK_NUM, inode_table);
// }

void mksfs(int fresh) {
    if (fresh)
    {
        init_fresh_disk(FILENAME, BLOCK_SIZE, BLOCK_NUM);
        
        make_sb();
        write_blocks(0, 1, &super_block);

        make_inode_table();
        write_blocks(1, INODE_BLOCK_NUM, inode_table);
        
        make_root_dir();
        write_blocks(1 + INODE_BLOCK_NUM + FREE_BIT_MAP_BLOCK_NUM, DIR_BLOCK_NUM, directory);

        //set up the free bit map
        for (int i = 0; i < BLOCK_NUM; i++) {
            if (i<1+INODE_BLOCK_NUM+FREE_BIT_MAP_BLOCK_NUM+DIR_BLOCK_NUM) {
                free_bit_map[i] = 0;
            }
            else {
                free_bit_map[i] = 1;
            }
        }

        write_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);
    }

    else {
        init_disk(FILENAME, BLOCK_SIZE, BLOCK_NUM);
        read_blocks(0, 1, &super_block);

        read_blocks(1, INODE_BLOCK_NUM, inode_table);
       
        read_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);
        
        read_blocks(1 + INODE_BLOCK_NUM + FREE_BIT_MAP_BLOCK_NUM, DIR_BLOCK_NUM, directory);
    }
}

int sfs_getnextfilename(char *fname) {
    for (int i = 0; i < INODE_NUM; i++) {
        if (!directory[i].visited && directory[i].inode_num > 0) {
            strcpy(fname, directory[i].filename);
            directory[i].visited = 1;//set the visited flag to true
            write_blocks(1 + INODE_BLOCK_NUM + FREE_BIT_MAP_BLOCK_NUM, DIR_BLOCK_NUM, directory);

            return 1;
        }
    }
    for (int i = 0; i < INODE_NUM; i++) {
        directory[i].visited = 0;
        write_blocks(1 + INODE_BLOCK_NUM + FREE_BIT_MAP_BLOCK_NUM, DIR_BLOCK_NUM, directory);
    }

    return 0;
}

int sfs_getfilesize(const char* path) {
    int inode_num = -1;
    for (int i = 0; i < INODE_NUM; i++)
    {
        if (strcmp(path, directory[i].filename) == 0) {
            inode_num = directory[i].inode_num;
        }
    }
    if (inode_num < 0) {
        return -1;
    }

    return inode_table[inode_num].size;
}

int sfs_fopen(char *name) {
    int inode = -1;
    if (strlen(name) > MAX_FILE_NAME) {
        return -1;
    }

    //find the file
    for (int i = 0; i < INODE_NUM; i++)
    {
        if (strcmp(name, directory[i].filename) == 0) {
            inode = directory[i].inode_num;
            break;
        }
    }
    //If the file is not found, we make a new file
    if (inode < 0) {
        inode = get_free_inode();
        if (inode < 0) {
            return -1;
        }
        file_t new_file;
        strncpy(new_file.filename, name, MAX_FILE_NAME);
        new_file.available = 0;
        new_file.visited = 0;
        new_file.inode_num = inode;
        inode_table[inode].link_cnt = 1;
        inode_table[inode].size = 0;
        directory[inode] = new_file;
        //update the directory
        write_blocks(1 + INODE_BLOCK_NUM + FREE_BIT_MAP_BLOCK_NUM, DIR_BLOCK_NUM, directory);
    }
    int fd = -1;
    //find the file descriptor of this file
    for (int i = 0; i < INODE_NUM; i++) {
        if (fd_table[i].inode_num == inode) {
            fd = i;
        }
    }

    //if the file descriptor not found, give it a new file descriptor
    if (fd < 0)
    {
        fd = get_free_fd();
        if (fd < 0) {
            return -1;
        }
        fd_table[fd].inode_num = inode;
        fd_table[fd].read_write_pointer = inode_table[inode].size;
    }
    // free_bit_map[inode] = 1;//update free bit map
    write_blocks(1, INODE_BLOCK_NUM, inode_table);
    write_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);
    return fd;
}

int sfs_fclose(int fileID) {
    if (fileID >= INODE_NUM) {
        return -1;
    }
    if (fd_table[fileID].inode_num > 0) {
        fd_table[fileID].inode_num = 0;
        fd_table[fileID].read_write_pointer = 0;
        return 0;
    }
    return -1;
}

int get_block(inode_t* inode, int block_index) {
    int block_on_disk = -1;
    //direct pointer
    if (block_index<12) {
        if (inode->direct_pointers[block_index] != 0) {
            block_on_disk = inode->direct_pointers[block_index];
            return block_on_disk;
        }
        //create a new block
        else {
            block_on_disk = get_free_block();
            
            if (block_on_disk < 0)
            {
                return -1;
            }
            inode->direct_pointers[block_index] = block_on_disk;
            write_blocks(1, INODE_BLOCK_NUM, inode_table);
            return block_on_disk;
        }

    }

    //if it exceeds the largest size a file can hold
    else if (block_index >= MAX_BLOCKS_PER_INODE) {
        return -1;
    }

    //indirect pointer
    else {

        //if we have an indirect pointer
        if (inode->indirect_pointer != 0)
        {
            int* indirect_block = (int *)malloc(BLOCK_SIZE);
            release_block(indirect_block);
            int indirect_block_pointer_index = block_index - 12;
            read_blocks(inode->indirect_pointer, 1, indirect_block);
            if (indirect_block[indirect_block_pointer_index] != 0) {
                block_on_disk = indirect_block[indirect_block_pointer_index];
                return block_on_disk;
            }
            //create a new block
            else {
                block_on_disk = get_free_block();
                if (block_on_disk < 0) {
                    return -1;
                }
                indirect_block[indirect_block_pointer_index] = block_on_disk;
                write_blocks(inode->indirect_pointer, 1, indirect_block);
                return block_on_disk;
            }
            free(indirect_block);
        }
        //if indirect pointer is not initialize yet
        else {
            int indirect_block_on_disk = get_free_block();//find a new free block
            if (indirect_block_on_disk < 0) {
                return -1;
            }
            inode->indirect_pointer = indirect_block_on_disk;//indirect pointer ponits to this block
            write_blocks(1, INODE_BLOCK_NUM, inode_table);//update inode table on disk

            int* indirect_block = (int *)malloc(BLOCK_SIZE);
            release_block(indirect_block);

            block_on_disk = get_free_block(); //find a new block on disk
            if (block_on_disk < 0) {
                return -1;
            }
            indirect_block[0] = block_on_disk; //first pointer points to the new block
            write_blocks(inode->indirect_pointer, 1, indirect_block);
            free_bit_map[inode->indirect_pointer] = 0; 
            write_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);//update free bit map
            free(indirect_block);
        }
    }

    return block_on_disk;
}

int sfs_fwrite(int fileID, const char* buf, int length) {

    if (fileID > INODE_NUM) {
        return 0;
    }
    int char_written = 0;
   

    if (fd_table[fileID].inode_num != 0) {
        fd_t *fd = &fd_table[fileID];
        inode_t* inode = &inode_table[fd->inode_num];
        int block_index = fd->read_write_pointer / BLOCK_SIZE;
        void *buffer = (void *)malloc(BLOCK_SIZE);
        while (char_written != length)
        {
            //find the position we want to write
            int block_on_disk = get_block(inode, block_index);

            if (block_on_disk < 0) {
                
                inode->size = MAX_BLOCKS_PER_INODE * 1024;
                
                write_blocks(1, INODE_BLOCK_NUM, inode_table);
                write_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);
                return -1;
            }

            read_blocks(block_on_disk, 1, buffer);
            int position_in_block = fd->read_write_pointer % BLOCK_SIZE;

            //write the current block before its filled
            for (int i = position_in_block; i < BLOCK_SIZE; i++) {

                memcpy(buffer + i, buf, 1);
                char_written+=1;
                buf+=1;
                fd->read_write_pointer+=1;
                if (char_written == length) {
                    break;
                }

            }
            
            
            int err = write_blocks(block_on_disk, 1, buffer);
            if (err < 0) {
                break;
            }
            free_bit_map[block_on_disk] = 0;
            block_index+=1;
        }

        //update the read write pointer
        if (fd->read_write_pointer > inode->size)  {
            inode->size = fd->read_write_pointer;
        }
        
        free(buffer);
        write_blocks(1, INODE_BLOCK_NUM, inode_table);
        write_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);

    }
    return char_written;
}
int get_block_read(inode_t *inode, int block_index) {
    int block_on_disk = -1;
    if (block_index < 0) {
        return -1;
    }

    //find the block direct pointer points to
    if (block_index < 12)
    {
        if (inode->direct_pointers[block_index] != 0) {
            block_on_disk = inode->direct_pointers[block_index];
       
            return block_on_disk;
        }
        else {
      
            return block_on_disk;
        }
    }
 

    //find the block indirect pointer points to
    else {
        if (inode->indirect_pointer != 0) {
            int* indirect_block;
            indirect_block = (int *)malloc(BLOCK_SIZE);
            int indirect_block_pointer_index = block_index - 12;
            read_blocks(inode->indirect_pointer, 1, indirect_block);
            if (indirect_block[indirect_block_pointer_index] != 0) {
                block_on_disk = indirect_block[indirect_block_pointer_index];
             
                return block_on_disk;
            }
            return -1;
        }
        else {
        
            return block_on_disk;
        }
        
        
    }

 
}



int sfs_fread(int fileID, char *buf, int length) {
    if (fileID > INODE_NUM) {
        return 0;
    }
    int char_read = 0;
    if (fd_table[fileID].inode_num > 0)
    {
        fd_t *fd = &fd_table[fileID];
        inode_t* inode = &inode_table[fd->inode_num];
        
        

        int block_index = fd->read_write_pointer / BLOCK_SIZE;
        
        void *buffer = (void *)malloc(BLOCK_SIZE);
        while (length > 0)
        {
            
            int block_on_disk = get_block_read(inode, block_index);
            if (block_on_disk < 0) {
       
                break;
            }

            read_blocks(block_on_disk, 1, buffer);
            int position_in_block = fd->read_write_pointer % BLOCK_SIZE;
            for (int i = position_in_block; i < BLOCK_SIZE; i++) {
                if (fd->read_write_pointer == inode->size) {
                    break;
                }
                memcpy(buf, buffer + i, 1);
                // if (strcmp(buffer+i, "\0") == 0) {
                //     fd->read_write_pointer+=1;
                //     buf += 1;
                // }
                char_read+=1;
                buf+=1;
                fd->read_write_pointer+=1;

                length-=1;
               
                if(length == 0) {
                    break;
                }
            }
            block_index+=1;
       
        }
    
        free(buffer);
        return char_read;
    }

    return 0;
}




int sfs_fseek(int fileID, int loc) {

    if (fd_table[fileID].inode_num != 0) {
        int inode_num = fd_table[fileID].inode_num;
        if (inode_table[inode_num].size <= loc) {
            return -1;
        }
        fd_table[fileID].read_write_pointer = loc;
        return 0;
    }
    return -1;
}

int sfs_remove(char *file) {
    if (strlen(file) > MAX_FILE_NAME) {
        return -1;
    }

    int inode_num = -1;
  
    for (int i = 0; i < INODE_NUM; i++)
    {
        if (strcmp(directory[i].filename, file) == 0) {
            inode_num = directory[i].inode_num;
            break;
        }
    }
    if (inode_num < 0) {
        return -1;
    }
    inode_t* inode = &inode_table[inode_num];


    int* empty_block =(int*)malloc(BLOCK_SIZE);
    // for (int i = 0; i < BLOCK_SIZE; i++) {
    //     *(int*)(empty_block + i) = 0;
    // }

    // int num_blocks = inode->size / BLOCK_SIZE + 1;
    // for (int i = 0; i < num_blocks; i++) {
    //     int block_index = get_block_read(inode, i);
    //     if (block_index < 0) {
    //         continue;
    //     }
    //     write_blocks(block_index, 1, (void*)empty_block);
    //     free_bit_map[i] = 0;
    // }
    // free(empty_block);
    // write_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);

    for (int i = 0; i < 12; i++) {
        if (inode->direct_pointers[i] <= 0) {
            continue;
        }

        else {
            write_blocks(inode->direct_pointers[i], 1, empty_block);
            free_bit_map[inode->direct_pointers[i]] = 1;
            write_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);
        }
    }

    int* indirect_block = (int*)malloc(BLOCK_SIZE);
    if (inode->indirect_pointer > 0)
    {
        read_blocks(inode->indirect_pointer, 1, indirect_block);
        for (int i = 0; i < BLOCK_SIZE / sizeof(int *); i++) {
            if (indirect_block[i]<= 0) {
                continue;
            }

            else {
                write_blocks(indirect_block[i], 1, empty_block);
                free_bit_map[indirect_block[i]] = 1;
                write_blocks(1 + INODE_BLOCK_NUM, FREE_BIT_MAP_BLOCK_NUM, free_bit_map);
            }
        }
    }

    strcpy(directory[inode_num].filename, "");
    for (int i = 0; i < MAX_FILE_NAME; i++) {
        directory[inode_num].filename[i] = 0;
    }
    directory[inode_num].available = 1;
    directory[inode_num].inode_num = 0;
    inode->size = 0;
    inode->link_cnt = 0;
    write_blocks(1, INODE_BLOCK_NUM, inode_table);
    write_blocks(1 + INODE_BLOCK_NUM + FREE_BIT_MAP_BLOCK_NUM, DIR_BLOCK_NUM, directory);

    for (int i = 0; i < INODE_NUM; i++)
    {
        if(fd_table[i].inode_num == inode_num) {
            sfs_fclose(i);
        }
    }

    
    
    return 0;
    
}