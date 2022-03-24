#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include <math.h>

#include "disk.h"
#include "fs.h"

#define FAT_BLOCK_SIZE 2048

struct Superblock
{
    char signature[8];
    uint16_t total_blk_count;
    uint16_t rdir_blk;
    uint16_t data_blk;
    uint16_t data_blk_count;
    uint8_t fat_blk_count;
    uint8_t padding[4079];
} __attribute__((__packed__));

struct FAT
{
    uint16_t *arr;
    int fat_avaible;
} __attribute__((__packed__));

struct RootDirBlock
{
    char filename[FS_FILENAME_LEN];
    uint32_t file_size;
    uint16_t first_dataBlock_index;
    uint8_t padding[10];
} __attribute__((__packed__));

struct RootDir
{
    struct RootDirBlock arr[FS_FILE_MAX_COUNT];
    int root_avaible;
} __attribute__((__packed__));

struct FDTableBlock
{
    int rootDir_index;
    size_t offset;
} __attribute__((__packed__));

struct FDTable
{
    struct FDTableBlock arr[FS_OPEN_MAX_COUNT];
    int fd_count;
} __attribute__((__packed__));

struct Superblock superblock;
struct FAT fat;
struct RootDir rootDir;
struct FDTable fdTable;

int mounted = 0;

int fs_mount(const char *diskname)
{
    if (block_disk_open(diskname) != 0)
    {
        /* Could not open diskname */
        return -1;
    }
    block_read(0, &superblock);
    if (strncmp(superblock.signature, "ECS150FS", 8) != 0)
    {
        /* Diskname does not match */
        return -1;
    }
    fat.arr = (uint16_t *)malloc((int)superblock.data_blk_count * sizeof(uint16_t));
    if (fat.arr == NULL)
    {
        /* FAT arr memory not allocated */
        return -1;
    }

    /* For full blocks, just do block read. For last fat block if it is less than 2048 than do bounce buffer. */
    for (int i = 1; i < superblock.rdir_blk; i++)
    {
        if (i == superblock.rdir_blk - 1)
        {
            int last_fat_block_count = superblock.data_blk_count % FAT_BLOCK_SIZE;
            if (last_fat_block_count)
            {
                uint16_t buf[FAT_BLOCK_SIZE];
                block_read(i, &buf);
                memcpy(&fat.arr[FAT_BLOCK_SIZE * (i - 1)], &buf, last_fat_block_count);
                break;
            }
        }
        block_read(i, &fat.arr[FAT_BLOCK_SIZE * (i - 1)]);
    }

    fat.fat_avaible = 0;
    /* get fat availbe  */
    for (int i = 0; i < superblock.data_blk_count; i++)
    {
        if (fat.arr[i] == 0)
        {
            fat.fat_avaible += 1;
        }
    }

    block_read(superblock.rdir_blk, &rootDir.arr);

    /* get rootDir avaible */
    rootDir.root_avaible = 0;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        if (strcmp("", rootDir.arr[i].filename) == 0)
        {
            rootDir.root_avaible += 1;
        }
    }
    /* Initialize FDtable */
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
    {
        fdTable.arr[i].rootDir_index = -1;
    }
    fdTable.fd_count = 0;

    mounted = 1;

    return 0;
}

int fs_umount(void)
{
    if (!mounted)
    {
        /* no FS is currently mounted */
        return -1;
    }
    if (fdTable.fd_count != 0)
    {
        /* There are still open file descriptors */
        return -1;
    }
    if (block_disk_close() != 0)
    {
        /* Could not close diskname */
        return -1;
    }
    free(fat.arr);
    mounted = 0;
    return 0;
}

int fs_info(void)
{
    if (block_disk_count() == -1)
    {
        /* No underlying virtual disk was opened */
        return -1;
    }
    printf("FS Info:\n");
    printf("total_blk_count=%d\n", superblock.total_blk_count);
    printf("fat_blk_count=%d\n", superblock.fat_blk_count);
    printf("rdir_blk=%d\n", superblock.rdir_blk);
    printf("data_blk=%d\n", superblock.data_blk);
    printf("data_blk_count=%d\n", superblock.data_blk_count);
    printf("fat_free_ratio=%d/%d\n", fat.fat_avaible, superblock.data_blk_count);
    printf("rdir_free_ratio=%d/%d\n", rootDir.root_avaible, FS_FILE_MAX_COUNT);
    return 0;
}

int fs_create(const char *filename)
{
    if (!mounted)
    {
        /* No FS is currently mounted */
        return -1;
    }
    if (filename == NULL || strcmp(filename, "") == 0)
    {
        /* filename is invalid */
        return -1;
    }

    /* loop thru root directory to see if filename already exists */
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        if (strcmp(filename, rootDir.arr[i].filename) == 0)
        {
            /* file named @filename already exists */
            return -1;
        }
    }
    if ((int)strlen(filename) > FS_FILENAME_LEN)
    {
        /* string @filename is too long */
        return -1;
    }
    if (rootDir.root_avaible == 0)
    {
        /* root directory already contains FS_FILE_MAX_COUNT files */
        return -1;
    }

    int found_index = -1;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        if (strcmp("", rootDir.arr[i].filename) == 0)
        {
            found_index = i;
            break;
        }
    }

    /* Construct current root directory block */
    struct RootDirBlock curRootDirBlock;
    strcpy(curRootDirBlock.filename, filename);
    curRootDirBlock.file_size = 0;
    curRootDirBlock.first_dataBlock_index = 0xFFFF;
    rootDir.arr[found_index] = curRootDirBlock;
    rootDir.root_avaible -= 1;
    block_write(superblock.rdir_blk, &rootDir.arr);

    return 0;
}

int fs_delete(const char *filename)
{
    if (!mounted)
    {
        /* No FS is currently mounted */
        return -1;
    }
    if (filename == NULL || strcmp(filename, "") == 0)
    {
        /* filename is invalid */
        return -1;
    }
    int found_index = -1;
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
        if (strcmp(filename, rootDir.arr[i].filename) == 0)
        {
            found_index = i;
            break;
        }
    }
    if (found_index == -1)
    {
        /* no file named @filename to delete */
        return -1;
    }

    /* Compare the rootDir dir filename with rootDir index in fd table (opened fd) with current delete filename */
    for (int i = 0; i < fdTable.fd_count; i++)
    {
        if (strcmp(rootDir.arr[fdTable.arr[i].rootDir_index].filename, filename) == 0)
        {
            /* file @filename is currently open */
            return -1;
        }
    }

    /* Delete the file named @filename from the root directory of the mounted file system. */
    struct RootDirBlock emptyRootDirBlock;
    memset(emptyRootDirBlock.filename, 0, sizeof(emptyRootDirBlock.filename));
    emptyRootDirBlock.first_dataBlock_index = 0;
    emptyRootDirBlock.file_size = 0;
    rootDir.arr[found_index] = emptyRootDirBlock;
    rootDir.root_avaible += 1;
    block_write(superblock.rdir_blk, &rootDir.arr);

    return 0;
}

int fs_ls(void)
{
    if (!mounted)
    {
        /* No FS is currently mounted */
        return -1;
    }
    printf("FS Ls:\n");
    for (int i = 0; i < FS_FILE_MAX_COUNT; i++)
    {
         if (strcmp("", rootDir.arr[i].filename) != 0)
        {
             printf("file: %s, size: %d, data_blk: %d\n", rootDir.arr[i].filename, rootDir.arr[i].file_size, rootDir.arr[i].first_dataBlock_index);
        }
    }
    return 0;
}

int fs_open(const char *filename)
{
    if (!mounted)
    {
        /* No FS is currently mounted */
        return -1;
    }
    if (filename == NULL || strcmp(filename, "") == 0)
    {
        /* filename is invalid */
        return -1;
    }

    /* Find filename in root diretionary */
    int found_index = -1;
    for (int i = 0; i < FS_FILE_MAX_COUNT - rootDir.root_avaible; i++)
    {
        if (strcmp(filename, rootDir.arr[i].filename) == 0)
        {
            found_index = i;
            break;
        }
    }
    if (found_index == -1)
    {
        /* no file named @filename to open */
        return -1;
    }
    if (fdTable.fd_count == FS_OPEN_MAX_COUNT)
    {
        /* Already FS_OPEN_MAX_COUNT files currently opened */
        return -1;
    }
    // to do: check if the same file is opened mutiple files, fs_open() does return the distinct file descriptors.
    struct FDTableBlock curFDTableBlock;
    curFDTableBlock.rootDir_index = found_index;
    curFDTableBlock.offset = 0;

    /* Find next avaible fd */
    int next_avaible_fd = -1;
    for (int i = 0; i < FS_OPEN_MAX_COUNT; i++)
    {
        if (fdTable.arr[i].rootDir_index == -1)
        {
            next_avaible_fd = i;
            break;
        }
    }

    /* Set next avaible fd index in fdTable array to current FD Table Block */
    fdTable.arr[next_avaible_fd] = curFDTableBlock;
    fdTable.fd_count += 1;

    return next_avaible_fd;
}

int fdTable_fd_errCheck(int fd)
{
    // fd is out of bound when greater than file max. It can be greter than fd_count since closing will reduce the count but the fd stays in same index in fdtable_arr
    if (fd >= FS_OPEN_MAX_COUNT)
    {
        /* @fd is out of bounds */
        return -1;
    }
    if (fdTable.arr[fd].rootDir_index == -1)
    {
        /* @fd is not currently open */
        return -1;
    }
    return 0;
}
int fs_close(int fd)
{
    if (!mounted)
    {
        /* No FS is currently mounted */
        return -1;
    }
    if (fdTable_fd_errCheck(fd) == -1)
    {
        return -1;
    }
    /* Construct emptyFDTableBlock and set the fd index of fdTable array to this emprty block */
    struct FDTableBlock emptyFDTableBlock;
    emptyFDTableBlock.rootDir_index = -1;
    emptyFDTableBlock.offset = 0;
    fdTable.arr[fd] = emptyFDTableBlock;
    fdTable.fd_count -= 1;

    return 0;
}

int fs_stat(int fd)
{
    if (!mounted)
    {
        /* No FS is currently mounted */
        return -1;
    }
    if (fdTable_fd_errCheck(fd) == -1)
    {
        return -1;
    }
    /* Get current rootDir_index from fdTable.arr and access rootDir[rootDir_index] to get fd's file size */
    return rootDir.arr[fdTable.arr[fd].rootDir_index].file_size;
}

int fs_lseek(int fd, size_t offset)
{
    if (!mounted)
    {
        /* No FS is currently mounted */
        return -1;
    }
    if (fdTable_fd_errCheck(fd) == -1)
    {
        return -1;
    }
    if ((int)offset > fs_stat(fd))
    {
        /* @offset is larger than the current file size */
        return -1;
    }
    fdTable.arr[fd].offset = offset;
    // to do: test this -> Call fs_lseek(fd, fs_stat(fd)) to append to a file
    return 0;
}

/* Times needed to traverse is calculated by the bytes divided by block size */
int get_traverse_times(size_t currentBytes)
{
    return currentBytes / BLOCK_SIZE;
}

int fs_write(int fd, void *buf, size_t count)
{   
    if (!mounted)
    {
        /* No FS is currently mounted */
        return -1;
    }
    if (fdTable_fd_errCheck(fd) == -1)
    {
        return -1;
    }
    if (buf == NULL)
    {
        /* @buf is NULL */
        return -1;
    }
    
    /* Get desired variables for traversing the FAT array */
    uint16_t offset = fdTable.arr[fd].offset;
    uint16_t rootDir_index = fdTable.arr[fd].rootDir_index;
    size_t allocated_size = ceil(rootDir.arr[rootDir_index].file_size / BLOCK_SIZE) * BLOCK_SIZE;
    size_t endingBytes = offset+count;
   
    if( endingBytes > allocated_size){

        int extraBlocks = (count - (offset % BLOCK_SIZE)) / BLOCK_SIZE + ((count - (offset % BLOCK_SIZE))% BLOCK_SIZE > 0 ? 1 : 0);
        int available_data_blocks = 0;
        int fat_block_indices[extraBlocks];

        /* Create an array of needed blocks alongn with FAT array index */
        for (int j = 0; j < superblock.data_blk_count; j++)
        {
            if (fat.arr[j] == 0)
            {
                fat_block_indices[available_data_blocks] = j;
                available_data_blocks++;
            }
            if (available_data_blocks == extraBlocks)
                break;
        }

        int cur_fat_index_cursor = rootDir.arr[rootDir_index].first_dataBlock_index;

        /* If cursor is 0xFFFF, that means the file has just been created, else it is extended write*/
        if(cur_fat_index_cursor == 0xFFFF){
            rootDir.arr[rootDir_index].first_dataBlock_index =  fat_block_indices[0];
            cur_fat_index_cursor = fat_block_indices[0];
            for (int k = 1; k < extraBlocks; k++)
            {
                fat.arr[cur_fat_index_cursor] = fat_block_indices[k];
                cur_fat_index_cursor = fat.arr[cur_fat_index_cursor];
            }
        }else{
             /* Get to the last index before 0xFFFF for extended write */
            while (fat.arr[cur_fat_index_cursor] != 0xFFFF)
            {
                cur_fat_index_cursor = fat.arr[cur_fat_index_cursor];
            }
            for (int k = 0; k < extraBlocks; k++)
            {
                fat.arr[cur_fat_index_cursor] = fat_block_indices[k];
                cur_fat_index_cursor = fat.arr[cur_fat_index_cursor];
            }
        }

        fat.arr[cur_fat_index_cursor] = 0xFFFF;

    }

    if(offset + count > rootDir.arr[rootDir_index].file_size){
        rootDir.arr[rootDir_index].file_size = offset + count ;
    }

    uint16_t cur_fat_index = rootDir.arr[rootDir_index].first_dataBlock_index;

    int cur_block = get_traverse_times(offset);
    size_t shift = 0;
    size_t byte_written = 0;
    size_t displacement = offset % BLOCK_SIZE;

    /* Get to current FAT index by traversing FAT array */
    for (int i = 0; i < cur_block; i++)
    {
        cur_fat_index = fat.arr[cur_fat_index];
    }

    uint16_t cur_index = superblock.data_blk + cur_fat_index;
    uint8_t *buffer = (uint8_t *)buf;
    uint8_t bounce_buffer[BLOCK_SIZE];

    /*loop iteration writes the block one at a time*/
    while(count > 0)
    {
        if (displacement + count > BLOCK_SIZE)
        {
            shift = BLOCK_SIZE - displacement;
        }
        else
        {
            shift = count;
        }
        /* If the shift is block size we can write to that cur_index directly without reading */
        if (shift == BLOCK_SIZE)
        {
            memcpy(&bounce_buffer[displacement], buffer, shift);
            block_write(cur_index, &bounce_buffer);
        }
        else
        {
            block_read(cur_index, &bounce_buffer);
            memcpy(&bounce_buffer[displacement], buffer, shift);
            block_write(cur_index, &bounce_buffer);
        }
        /* Traverse to next FAT index using FAT array and current FAT Index */
        cur_fat_index = fat.arr[cur_fat_index];
        /* Current actual index by FAT array index + starting data block index */
        cur_index = superblock.data_blk + cur_fat_index;
        /*position the buffer to the next block*/
        byte_written += shift;
        buffer += shift;
        /* remove the read amount from the count */
        count -= shift;
        displacement = 0;
    }

    fdTable.arr[fd].offset += byte_written;
    block_write(superblock.rdir_blk, &rootDir.arr);

    return byte_written;
}

size_t min(size_t a, size_t b)
{
    return (a > b) ? b : a;
}

int fs_read(int fd, void *buf, size_t count)
{
    if (!mounted)
    {
        /* No FS is currently mounted */
        return -1;
    }
    if (fdTable_fd_errCheck(fd) == -1)
    {
        return -1;
    }
    if (buf == NULL)
    {
        /* @buf is NULL */
        return -1;
    }

    /* Get desired variables for traversing the FAT array */
    uint16_t offset = fdTable.arr[fd].offset;
    uint16_t rootDir_index = fdTable.arr[fd].rootDir_index;
    uint16_t cur_fat_index = rootDir.arr[rootDir_index].first_dataBlock_index;

    /* Ending bytes should not read beyond file size */
    size_t endBytes = min((offset + count), rootDir.arr[rootDir_index].file_size);

    /* Initailize variables for memcpy from bounce buffer to buf and block read */
    uint8_t *buffer = (uint8_t *)buf;
    size_t buf_size = 0;
    size_t shift = 0;
    size_t displacement = offset % BLOCK_SIZE;
    size_t bounded_count = endBytes - offset;

     /* If the first position is FAT_EOC don't read */
    if(cur_fat_index == 0xFFFF){
        return 0;
    }

    /* Get times need to traverse to current staritng bytes*/
    int cur_block = get_traverse_times(offset);

    /* Get to current FAT index by traversing FAT array */
    for (int i = 0; i < cur_block; i++)
    {
        cur_fat_index = fat.arr[cur_fat_index];
    }

    uint16_t cur_index = superblock.data_blk + cur_fat_index; 
    
    /*loop iteration read the block one at a time until count is 0*/
    while (bounded_count > 0)
    {
        if (displacement + bounded_count > BLOCK_SIZE)
        {
            shift = BLOCK_SIZE - displacement;
        }
        else
        {
            shift = bounded_count;
        }
        /* If the shift is block size we can call block read directly without using bounce buffer */
        if (shift == BLOCK_SIZE)
        {
            block_read(cur_index, &buffer[buf_size]);
        }
        else
        {
            uint8_t bounce_buffer[BLOCK_SIZE];
            block_read(cur_index, &bounce_buffer);
            memcpy(&buffer[buf_size], &bounce_buffer[displacement], shift);
        }

        /* Traverse to next FAT index using FAT array and current FAT Index */
        cur_fat_index = fat.arr[cur_fat_index];
        /* Current actual index by FAT array index + starting data block index */
        cur_index = superblock.data_blk + cur_fat_index;
        /*position the buffer to the next block*/
        buf_size += shift;
        /* remove the read amount from the count */
        bounded_count -= shift;
        displacement = 0;
    }

    return buf_size;
}
