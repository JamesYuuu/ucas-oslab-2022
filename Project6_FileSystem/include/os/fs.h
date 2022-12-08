#ifndef __INCLUDE_OS_FS_H__
#define __INCLUDE_OS_FS_H__

#include <type.h>
#include <os/mm.h>
#include <os/task.h>

/* macros of file system */
#define SUPERBLOCK_MAGIC 0x20221205
#define NUM_FDESCS 16

// file_system start from 512mb

#define FS_START         (512 * 1024 * 1024 / SECTOR_SIZE)       // start from 2^20 sector
#define SECTOR_MAP_START (FS_START + 1)                          
#define INODE_MAP_START  (SECTOR_MAP_START + SECTOR_MAP_SIZE)
#define INODE_START      (INODE_MAP_START + INODE_MAP_SIZE)
#define DATA_START       (INODE_START + INODE_SECTOR_NUM)
 
#define SECTOR_NUM       (512 * 1024 * 1024 / SECTOR_SIZE)        // 512MB = 2^20 * 512B

#define INODE_SIZE       128                                     // inode size = 128 bytes
#define INODE_NUM        (SECTOR_SIZE * 8)                       // inode number = 4096 
#define INODE_SECTOR_NUM (INODE_NUM * INODE_SIZE / SECTOR_SIZE)  // inode sectors = 1k

#define SECTOR_MAP_SIZE  (SECTOR_NUM / SECTOR_SIZE / 8)          // 256 sectors for blockmap
#define INODE_MAP_SIZE   1                                       // 1 sectors for inodemap

#define DATA_NUM         (SECTOR_NUM - (INODE_SECTOR_NUM + INODE_MAP_SIZE + SECTOR_MAP_SIZE) )

#define DENTRY_SIZE      32

#define min(a,b)    ((a) < (b) ? (a) : (b))
#define max(a,b)    ((a) > (b) ? (a) : (b))

/* data structures of file system */
typedef struct superblock_t{
    // TODO [P6-task1]: Implement the data structure of superblock
    uint64_t magic;

    uint64_t fs_start;
    uint64_t fs_sector_num;

    uint64_t sector_map_start;
    uint64_t sector_map_size;

    uint64_t inode_map_start;
    uint64_t inode_map_size;

    uint64_t inode_start;
    uint64_t inode_free;
    uint64_t inode_num;
    uint64_t inode_sector_num;

    uint64_t data_start;
    uint64_t data_free;
    uint64_t data_num;

    uint64_t sector_size;
    uint64_t inode_size;
    uint64_t dentry_size;

    uint64_t root_ino;

    char padding[512 - 136];
} superblock_t;

typedef enum dentry_type{
    NODE_NULL,
    NODE_DIR,
    NODE_FILE,
} dentry_type_t;

typedef struct dentry_t{
    // TODO [P6-task1]: Implement the data structure of directory entry
    uint32_t ino;
    dentry_type_t type;
    char name[32 - 4 - 4];
} dentry_t;

typedef enum ino_type{
    INO_DIR,
    INO_FILE,
} ino_type_t;

typedef enum ino_access{
    INO_RDONLY,
    INO_WRONLY,
    INO_RDWR,
} ino_access_t;

typedef struct inode_t{ 
    // TODO [P6-task1]: Implement the data structure of inode
    uint32_t ino;
    ino_type_t type;
    ino_access_t access;
    uint32_t nlinks;
    union{
        uint32_t file_size;
        uint32_t file_num;
    };
    uint32_t used_size;
    
    uint32_t create_time;
    uint32_t modify_time;

    uint32_t direct_index[1 + 5];
    uint32_t indirect_index1[3];
    uint32_t indirect_index2[2];
    uint32_t indirect_index3;

    char padding[128 - 80];

} inode_t;

typedef struct fdesc_t{
    // TODO [P6-task2]: Implement the data structure of file descriptor
    uint32_t inode_id;
    
} fdesc_t;

/* modes of do_fopen */
#define O_RDONLY 1  /* read only open */
#define O_WRONLY 2  /* write only open */
#define O_RDWR   3  /* read/write open */

/* whence of do_lseek */
#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

/* fs function declarations */
extern int do_mkfs(void);
extern int do_statfs(void);
extern int do_cd(char *path);
extern int do_mkdir(char *path);
extern int do_rmdir(char *path);
extern int do_ls(char *path, int option);
extern int do_touch(char *path);
extern int do_cat(char *path);
extern int do_fopen(char *path, int mode);
extern int do_fread(int fd, char *buff, int length);
extern int do_fwrite(int fd, char *buff, int length);
extern int do_fclose(int fd);
extern int do_ln(char *src_path, char *dst_path);
extern int do_rm(char *path);
extern int do_lseek(int fd, int offset, int whence);

#endif