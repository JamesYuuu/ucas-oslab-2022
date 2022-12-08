#include <os/string.h>
#include <os/fs.h>
#include <common.h>
#include <os/time.h>

static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];

static char padding[SECTOR_SIZE];

uint32_t current_ino;

void sd_write_offset(uint64_t mem_address, unsigned block_id, unsigned offset,unsigned len)
{
    bzero(padding, SECTOR_SIZE);
    memcpy(padding + offset, (void*)mem_address,len);
    sd_write(kva2pa(padding), 1, block_id);
}

void sd_read_offset(uint64_t mem_address, unsigned block_id, unsigned offset,unsigned len)
{
    sd_read(kva2pa(padding), 1, block_id);
    memcpy((void*)mem_address, padding + offset, len);
}

int alloc_block()
{
    for (int i=0;i<superblock.block_map_size;i++)
    {
        sd_read(kva2pa(padding), 1, superblock.block_map_start+i);
        for (int j=0;j<SECTOR_SIZE;j++)
        {
            if (padding[j] != 0xff)
            {
                for (int k=0;k<8;k++)
                {
                    if ((padding[j] & (1<<k)) == 0)
                    {
                        padding[j] |= (1<<k);
                        sd_write(kva2pa(padding), 1, superblock.block_map_start+i);
                        superblock.data_free--;
                        return i*SECTOR_SIZE*8+j*8+k + superblock.data_start;
                    }
                }
            }
        }
    }
}

int alloc_inode()
{
    for (int i=0;i<superblock.inode_map_size;i++)
    {
        sd_read(kva2pa(padding), 1, superblock.inode_map_start+i);
        for (int j=0;j<SECTOR_SIZE;j++)
        {
            if (padding[j] != 0xff)
            {
                for (int k=0;k<8;k++)
                {
                    if ((padding[j] & (1<<k)) == 0)
                    {
                        padding[j] |= (1<<k);
                        sd_write(kva2pa(padding), 1, superblock.inode_map_start+i);
                        superblock.inode_free--;
                        return i*SECTOR_SIZE*8+j*8+k;
                    }
                }
            }
        }
    }
}

void init_superblock(void)
{
    superblock.magic            = SUPERBLOCK_MAGIC;
    superblock.fs_start         = FS_START;
    superblock.fs_block_num     = BLOCK_NUM;
    superblock.block_map_size   = BLOCK_MAP_SIZE;
    superblock.block_map_start  = BLOCK_MAP_START;
    superblock.inode_map_size   = INODE_MAP_SIZE;
    superblock.inode_map_start  = INODE_MAP_START;
    superblock.inode_start      = INODE_START;
    superblock.inode_free       = INODE_NUM;
    superblock.inode_num        = INODE_NUM;
    superblock.inode_sector_num = INODE_SECTOR_NUM;
    superblock.data_start       = DATA_START;
    superblock.data_free        = DATA_NUM;
    superblock.data_num         = DATA_NUM;
    superblock.block_size       = BLOCK_SIZE;
    superblock.inode_size       = INODE_SIZE;
    superblock.dentry_size      = DENTRY_SIZE;
    superblock.root_ino         = 0;
    bzero(superblock.padding, sizeof(superblock.padding));
    sd_write(kva2pa(&superblock), 1 , FS_START);
}

inode_t init_inode_dir(void)
{
    inode_t inode;
    inode.ino      = alloc_inode();
    inode.type     = INO_DIR;
    inode.access   = INO_RDWR;
    inode.nlinks   = 1;
    inode.file_num = 2;        // . and ..
    inode.used_size = 2 * superblock.dentry_size;
    inode.create_time = inode.modify_time = get_timer();
    inode.direct_index[0] = alloc_block();
    bzero(inode.padding, sizeof(inode.padding));
    return inode;
}

void reflush_inode(inode_t *inode)
{
    sd_write_offset(inode, superblock.inode_start + inode->ino / 4 ,(inode->ino%4)*superblock.inode_size,superblock.inode_size);
}

void init_dentry(inode_t *inode,uint32_t parent_ino)
{
    dentry_t dir_entry[2];
    dir_entry[0].ino = inode->ino;
    dir_entry[0].type = INO_DIR;
    strcpy(dir_entry[0].name, ".");

    dir_entry[1].ino = parent_ino;
    dir_entry[1].type = INO_DIR;
    strcpy(dir_entry[1].name, "..");

    sd_write_offset(dir_entry, inode->direct_index[0], 0, 2 * superblock.dentry_size);
}

void init_root_dir(void)
{
    // init inode
    inode_t root_inode = init_inode_dir();
    reflush_inode(&root_inode);
    // init root dentry
    init_dentry(&root_inode,root_inode.ino);

    current_ino = root_inode.ino;
}

int do_mkfs(void)
{
    // TODO [P6-task1]: Implement do_mkfs
    printk("[FS] Start Initialize filesystem!\n");
    printk("[FS] Setting superblock...\n");

    init_superblock();

    printk("magic: 0x%x\n",superblock.magic);
    printk("num_blocks: %d, start_sector: %d\n",superblock.fs_block_num,superblock.fs_start);
    printk("block map offset: 1 (%d)\n",superblock.block_map_size);
    printk("inode map offset: %d (%d)\n",superblock.inode_map_start-superblock.fs_start,superblock.inode_map_size);
    printk("inode offset: %d (%d)\n",superblock.inode_start-superblock.fs_start,superblock.inode_sector_num);
    printk("data offset: %d (%d)\n",superblock.data_start-superblock.fs_start,superblock.data_num * 8);
    printk("inode entry size: %dB, dentry size: %dB\n",superblock.inode_size,superblock.dentry_size);

    printk("[FS]: Setting block map...\n");
    bzero(padding, SECTOR_SIZE);
    for (uint32_t i = 0; i < superblock.block_map_size; i++)
        sd_write(kva2pa(padding), 1, superblock.block_map_start + i);

    printk("[FS]: Setting inode map...\n");
    bzero(padding, SECTOR_SIZE);
    for (uint32_t i = 0; i < superblock.inode_map_size; i++)
        sd_write(kva2pa(padding), 1, superblock.inode_map_start + i);

    printk("[FS]: Setting inode...\n");
    init_root_dir();

    printk("[FS]: Initialize filesystem finished!\n");

    return 0;  // do_mkfs succeeds
}

int do_statfs(void)
{
    // TODO [P6-task1]: Implement do_statfs
    printk("magic: 0x%x (KFS)\n",superblock.magic);
    printk("used sector :%d/%d, start sector: %d (0x%x)\n",superblock.fs_block_num-superblock.data_free,superblock.fs_block_num,superblock.fs_start,superblock.fs_start*512);
    printk("inode map offset: %d, occupied sector: %d, used : %d/%d\n",superblock.inode_map_start-superblock.fs_start,superblock.inode_map_size,superblock.inode_num-superblock.inode_free,superblock.inode_num);
    printk("inode offset: %d, occupied sector: %d, used : %d/%d\n",superblock.inode_start-superblock.fs_start,superblock.inode_sector_num,superblock.inode_num-superblock.inode_free,superblock.inode_num);
    printk("data offset: %d, occupied sector: %d, used : %d/%d\n",superblock.data_start-superblock.fs_start,superblock.data_num * 8,superblock.data_num-superblock.data_free,superblock.data_num);
    printk("inode entry size: %dB, dentry size: %dB\n",superblock.inode_size,superblock.dentry_size);
    return 0;  // do_statfs succeeds
}

inode_t get_inode(uint32_t ino)
{
    inode_t inode;
    // 1 sector = 4 inode
    sd_read_offset(&inode, superblock.inode_start + ino / 4, (ino%4)*superblock.inode_size, superblock.inode_size);
    return inode;
}

uint32_t get_father_ino(uint32_t ino)
{
    inode_t inode = get_inode(ino);
    dentry_t dentry;
    sd_read_offset(&dentry, inode.direct_index[0], superblock.dentry_size, superblock.dentry_size);
    return dentry.ino;
}

int check_exist(char *path, inode_t *father_node)
{
    // 1 sector = 16 dentry
    for (int i=0;i<father_node->file_num/16+1;i++)
    {
        sd_read(kva2pa(padding),1,father_node->direct_index[i]);
        dentry_t *dir = (dentry_t *)padding;
        for (int j=0;j<min(father_node->file_num-i*16,16);j++)
            if (strcmp(dir[j].name,path)==0) return 1;
    }
}


int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd

    return 0;  // do_cd succeeds
}

int do_mkdir(char *path)
{
    // TODO [P6-task1]: Implement do_mkdir
    uint32_t father_ino = get_father_ino(current_ino);
    inode_t father_inode = get_inode(father_ino);    
    if (check_exist(path,&father_inode)) return 1; // dir already exist
    
    inode_t dir_inode = init_inode_dir();


    return 0;  // do_mkdir succeeds
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir

    return 0;  // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core

    return 0;  // do_ls succeeds
}

int do_touch(char *path)
{
    // TODO [P6-task2]: Implement do_touch

    return 0;  // do_touch succeeds
}

int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat

    return 0;  // do_cat succeeds
}

int do_fopen(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_fopen

    return 0;  // return the id of file descriptor
}

int do_fread(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fread

    return 0;  // return the length of trully read data
}

int do_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fwrite

    return 0;  // return the length of trully written data
}

int do_fclose(int fd)
{
    // TODO [P6-task2]: Implement do_fclose

    return 0;  // do_fclose succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln

    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm

    return 0;  // do_rm succeeds 
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek

    return 0;  // the resulting offset location from the beginning of the file
}

void init_file_system(void)
{
    sd_read(kva2pa(&superblock),1,FS_START);
    if (superblock.magic == SUPERBLOCK_MAGIC) return;
    do_mkfs();
    return;    
}