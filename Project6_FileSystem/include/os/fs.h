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

typedef enum ino_type{
    INO_DIR,
    INO_FILE,
    INO_LINK,
} ino_type_t;

typedef struct dentry_t{
    // TODO [P6-task1]: Implement the data structure of directory entry
    uint32_t ino;
    ino_type_t type;
    char name[12];
    char dst_name[12];
} dentry_t;

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

    // 6 * 512B = 3KB
    uint32_t direct_index[1 + 5];  // 3KB
    // 512B = 128 int
    uint32_t indirect_index1[3];   // represent 3*128 direct_index          3*64KB
    uint32_t indirect_index2[2];   // represent 2*128*128 direct_index      2*8MB
    uint32_t indirect_index3;      // represent 128*128*128 direct_index    1GB

    char padding[128 - 80];

} inode_t;

typedef struct fdesc_t{
    // TODO [P6-task2]: Implement the data structure of file descriptor
    uint32_t ino;
    uint32_t mode;
    state_t is_used;
    uint32_t offset;
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

extern void sd_write_offset(uint64_t mem_address, unsigned block_id, unsigned offset,unsigned len);
extern void sd_read_offset(uint64_t mem_address, unsigned block_id, unsigned offset,unsigned len);
extern void reflush_inode(inode_t *inode);

extern uint32_t alloc_sector(void);
extern uint32_t alloc_inode(void);
extern void release_inode(uint32_t ino);
extern void release_sector(uint32_t block_id);

extern void init_superblock(void);
extern inode_t init_inode(ino_type_t inode_type);
extern void init_dentry(inode_t *inode,uint32_t parent_ino);
extern void init_root_dir(void);

extern inode_t get_inode(uint32_t ino);
extern uint32_t get_father_ino(uint32_t ino);
extern int get_son_inode(char *path, inode_t *father_node);
extern uint32_t get_final_ino(char *path, uint32_t ino,uint32_t offset);

extern void set_father_dir(inode_t *father_node, char *name, inode_t *son_inode);
extern int remove_son_dir(inode_t *father_node, char *name,ino_type_t type);

extern void print_timer(uint32_t time);

extern unsigned int get_block_id(inode_t *file, int sector_id);

#endif