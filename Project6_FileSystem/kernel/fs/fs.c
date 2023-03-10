#include <os/string.h>
#include <os/fs.h>
#include <common.h>
#include <os/time.h>
#include <printk.h>

static superblock_t superblock;
static fdesc_t fdesc_array[NUM_FDESCS];

static char padding[SECTOR_SIZE];
static char dir_buffer[SECTOR_SIZE];
static char file_buffer[SECTOR_SIZE];

uint32_t current_ino;

int do_mkfs(void)
{
    // TODO [P6-task1]: Implement do_mkfs
    printk("[FS] Start Initialize filesystem!\n");
    printk("[FS] Setting superblock...\n");

    init_superblock();

    printk("magic: 0x%x\n",superblock.magic);
    printk("num_blocks: %d, start_sector: %d\n",superblock.fs_sector_num,superblock.fs_start);
    printk("sector map offset: 1 (%d)\n",superblock.sector_map_size);
    printk("inode map offset: %d (%d)\n",superblock.inode_map_start-superblock.fs_start,superblock.inode_map_size);
    printk("inode offset: %d (%d)\n",superblock.inode_start-superblock.fs_start,superblock.inode_sector_num);
    printk("data offset: %d (%d)\n",superblock.data_start-superblock.fs_start,superblock.data_num);
    printk("inode entry size: %dB, dentry size: %dB\n",superblock.inode_size,superblock.dentry_size);

    printk("[FS]: Setting block map...\n");
    bzero(padding, SECTOR_SIZE);
    for (uint32_t i = 0; i < superblock.sector_map_size; i++)
        sd_write(kva2pa(padding), 1, superblock.sector_map_start + i);

    printk("[FS]: Setting inode map...\n");
    for (uint32_t i = 0; i < superblock.inode_map_size; i++)
        sd_write(kva2pa(padding), 1, superblock.inode_map_start + i);

    printk("[FS]: Setting inode...\n");
    init_root_dir();

    printk("[FS]: Initialize filesystem finished!\n");

    superblock.root_ino = current_ino;

    return 0;  // do_mkfs succeeds
}

int do_statfs(void)
{
    sd_write(kva2pa(&superblock), 1 , FS_START);
    // TODO [P6-task1]: Implement do_statfs
    printk("magic: 0x%x (KFS)\n",superblock.magic);
    printk("used sector: %d/%d, start sector: %d (0x%x)\n",superblock.fs_sector_num-superblock.data_free,superblock.fs_sector_num,superblock.fs_start,superblock.fs_start*512);
    printk("inode map offset: %d, occupied sector: %d, used : %d/%d\n",superblock.inode_map_start-superblock.fs_start,superblock.inode_map_size,superblock.inode_num-superblock.inode_free,superblock.inode_num);
    printk("inode offset: %d, occupied sector: %d, used : %d/%d\n",superblock.inode_start-superblock.fs_start,superblock.inode_sector_num,superblock.inode_num-superblock.inode_free,superblock.inode_num);
    printk("data offset: %d, occupied sector: %d, used : %d/%d\n",superblock.data_start-superblock.fs_start,superblock.data_num,superblock.data_num-superblock.data_free,superblock.data_num);
    printk("inode entry size: %dB, dentry size: %dB\n",superblock.inode_size,superblock.dentry_size);
    return 0;  // do_statfs succeeds
}

int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd
    uint32_t ino_num = get_final_ino(path,current_ino,0);
    if (ino_num == -1) return -1;
        else current_ino = ino_num;
    return 0;  // do_cd succeeds
}

int do_mkdir(char *path)
{
    // TODO [P6-task1]: Implement do_mkdir
    inode_t father_inode = get_inode(current_ino);    
    if (get_son_inode(path,&father_inode)!=-1) return -1; // dir already exist
    // init son dir
    inode_t dir_inode = init_inode(INO_DIR);
    init_dentry(&dir_inode,current_ino);
    // set father dir
    set_father_dir(&father_inode,path,&dir_inode);
    return 0;  // do_mkdir succeeds
}

int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir
    inode_t father_inode = get_inode(current_ino);
    // remove son dir
    return remove_son_dir(&father_inode,path,INO_DIR);
    //return 0;  // do_rmdir succeeds
}

int do_ls(char *path, int option)
{
    // TODO [P6-task1]: Implement do_ls
    // Note: argument 'option' serves for 'ls -l' in A-core
    // 1 sector = 16 dentry
    uint32_t ino_num = get_final_ino(path,current_ino,0);
    if (ino_num == -1) return -1;
    inode_t final_node = get_inode(ino_num);
    for (int i=0;i<final_node.file_num/16+1;i++)
    {
        sd_read(kva2pa(dir_buffer),1,final_node.direct_index[i]);
        dentry_t *dir = (dentry_t *)dir_buffer;
        for (int j=0;j<min(final_node.file_num-i*16,16);j++)
        {
            if (option==1)
            {
                inode_t temp_node = get_inode(dir[j].ino);
                if (temp_node.type == INO_DIR) printk("d");
                    else printk("-");
                if (temp_node.access == INO_RDONLY) printk("r- ");
                    else if (temp_node.access == INO_WRONLY) printk("-w ");
                        else if (temp_node.access == INO_RDWR) printk("rw ");
                printk("%d %d %d ",temp_node.nlinks,temp_node.used_size,temp_node.ino);  
                print_timer(temp_node.create_time);
                print_timer(temp_node.modify_time);
            }
            printk("%s ",dir[j].name);
            if (option==1)
            {
                if (dir[j].type==INO_LINK) printk("-> %s",dir[j].dst_name);
                printk("\n");
            }
        }
    }
    if (option==0) printk("\n");
    return 0;  // do_ls succeeds
}

int do_touch(char *path)
{
    // TODO [P6-task2]: Implement do_touch
    inode_t father_inode = get_inode(current_ino);
    if (get_son_inode(path,&father_inode)!=-1) return -1; // file already exist
    // init son file
    inode_t file_inode = init_inode(INO_FILE);
    set_father_dir(&father_inode,path,&file_inode);
    return 0;  // do_touch succeeds
}

int do_cat(char *path)
{
    // TODO [P6-task2]: Implement do_cat
    // cat don't support big file
    inode_t father_inode = get_inode(current_ino);
    int son_ino = get_son_inode(path,&father_inode); 
    if (son_ino == -1) return -1;                        // file not exist
    // print son file
    inode_t son_inode = get_inode(son_ino);
    for (int i=0;i<son_inode.file_size/SECTOR_SIZE;i++)
    {
        sd_read(kva2pa(file_buffer),1,son_inode.direct_index[i]);
        printk("%s",file_buffer);
    }
    if (son_inode.file_size%SECTOR_SIZE!=0)
    {
        sd_read(kva2pa(file_buffer),1,son_inode.direct_index[son_inode.file_size/SECTOR_SIZE]);
        for (int i=0;i<son_inode.file_size%SECTOR_SIZE;i++)
            printk("%c",file_buffer[i]);
    }

    if (file_buffer[son_inode.file_size%SECTOR_SIZE-1]!='\n') printk("\n");
    return 0;  // do_cat succeeds
}

int do_fopen(char *path, int mode)
{
    // TODO [P6-task2]: Implement do_fopen
    inode_t father_inode = get_inode(current_ino);
    int son_ino = get_son_inode(path,&father_inode);
    if (son_ino == -1) return -1;                        // file not exist
    // get a spare fd
    int spare_fd = -1;
    for (int i=0;i<NUM_FDESCS;i++)
    {
        if (fdesc_array[i].is_used == FREE)
        {
            spare_fd = i;
            break;
        }
    }
    if (spare_fd == -1) return 2;                       // no spare fd
    // init fd
    fdesc_array[spare_fd].is_used = USED;
    fdesc_array[spare_fd].ino     = son_ino;
    fdesc_array[spare_fd].mode    = mode;
    fdesc_array[spare_fd].offset  = 0;

    return spare_fd;  // return the id of file descriptor
}

int do_fread(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fread
    if (fdesc_array[fd].is_used == FREE) return -1;      // fd not exist
    if (fdesc_array[fd].mode == O_WRONLY) return -2;     // fd is not readable
    
    inode_t file = get_inode(fdesc_array[fd].ino);
    int real_length = min(length,file.file_size - fdesc_array[fd].offset);
    int start_sector = fdesc_array[fd].offset/SECTOR_SIZE;
    int end_sector = (fdesc_array[fd].offset+real_length)/SECTOR_SIZE;

    int offset_buff, offset_file_buff, copy_length;

    for (int i=start_sector;i<=end_sector;i++)
    {
        unsigned int block_id = get_block_id(&file,i);
        sd_read(kva2pa(file_buffer),1,block_id);
        offset_buff      = (i==start_sector) ? 0 : (i-start_sector) * SECTOR_SIZE - fdesc_array[fd].offset % SECTOR_SIZE;
        offset_file_buff = (i==start_sector) ? fdesc_array[fd].offset % SECTOR_SIZE : 0;
        copy_length      = (i==end_sector) ? real_length - offset_buff : SECTOR_SIZE - offset_file_buff;
        memcpy(buff+offset_buff,file_buffer+offset_file_buff,copy_length);
    }

    fdesc_array[fd].offset += real_length;

    return real_length;  // return the length of trully read data
}

int do_fwrite(int fd, char *buff, int length)
{
    // TODO [P6-task2]: Implement do_fwrite
    if (fdesc_array[fd].is_used == FREE) return -1;      // fd not exist
    if (fdesc_array[fd].mode == O_RDONLY) return -2;     // fd is not writable

    inode_t file = get_inode(fdesc_array[fd].ino);
    int start_sector = fdesc_array[fd].offset/SECTOR_SIZE;
    int end_sector = (fdesc_array[fd].offset+length)/SECTOR_SIZE;

    int offset_buff, offset_file_buff, copy_length;

    for (int i=start_sector;i<=end_sector;i++)
    {
        unsigned int block_id = get_block_id(&file,i);
        offset_buff      = (i==start_sector) ? 0 : (i-start_sector) * SECTOR_SIZE - fdesc_array[fd].offset % SECTOR_SIZE;
        offset_file_buff = (i==start_sector) ? fdesc_array[fd].offset % SECTOR_SIZE : 0;
        copy_length      = (i==end_sector) ? length - offset_buff : SECTOR_SIZE - offset_file_buff;
        memcpy(file_buffer+offset_file_buff,buff+offset_buff,copy_length);
        sd_write(kva2pa(file_buffer),1,block_id);
    }

    file.file_size +=length;
    file.used_size +=length;
    file.modify_time = get_timer();
    fdesc_array[fd].offset += length;

    reflush_inode(&file);

    return length;  // return the length of trully written data
}

int do_fclose(int fd)
{
    // TODO [P6-task2]: Implement do_fclose
    fdesc_array[fd].is_used = FREE;
    return 0;  // do_fclose succeeds
}

int do_ln(char *src_path, char *dst_path)
{
    // TODO [P6-task2]: Implement do_ln
    uint32_t src_inode = get_final_ino(src_path,current_ino,0);
    uint32_t dst_inode = get_final_ino(dst_path,current_ino,1);
    inode_t src_file = get_inode(src_inode);
    inode_t dst_directory = get_inode(dst_inode);
    // set new dentry
    dentry_t new_dentry;
    new_dentry.ino = src_inode;
    new_dentry.type = INO_LINK;
    strcpy(new_dentry.name,dst_path);
    strcpy(new_dentry.dst_name,src_path);
    // set destination directory
    if (dst_directory.file_num%16==0) dst_directory.direct_index[dst_directory.file_num/16] = alloc_sector();
    sd_write_offset(&new_dentry,dst_directory.direct_index[dst_directory.file_num/16],dst_directory.file_num%16*superblock.dentry_size,superblock.dentry_size);
    dst_directory.file_num++;
    dst_directory.modify_time = get_timer();
    dst_directory.nlinks++;
    dst_directory.used_size++;
    // set source file
    src_file.nlinks++;
    // reflush inode
    reflush_inode(&src_file);
    reflush_inode(&dst_directory);
    return 0;  // do_ln succeeds 
}

int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm
    inode_t father_node = get_inode(current_ino);
    int file_result = remove_son_dir(&father_node,path,INO_FILE);  // do_rm succeeds 
    int link_result = remove_son_dir(&father_node,path,INO_LINK);  // do_rm succeeds
    return link_result == -1 && file_result == -1 ? -1 : 0;
}

int do_lseek(int fd, int offset, int whence)
{
    // TODO [P6-task2]: Implement do_lseek
    if (fdesc_array[fd].is_used == FREE) return -1;      // fd not exist
    
    if (whence==SEEK_SET) fdesc_array[fd].offset = max(0,offset);
        else if (whence==SEEK_CUR) fdesc_array[fd].offset = max(0,fdesc_array[fd].offset+offset);
            else if (whence==SEEK_END) fdesc_array[fd].offset = max(0,get_inode(fdesc_array[fd].ino).file_size+offset);
                else return -2;  // whence not valid

    return fdesc_array[fd].offset;  // the resulting offset location from the beginning of the file
}

// sd_read and sd_write offset
void sd_write_offset(uint64_t mem_address, unsigned block_id, unsigned offset,unsigned len)
{
    sd_read(kva2pa(padding), 1 ,block_id);
    memcpy(padding + offset, (void*)mem_address,len);
    sd_write(kva2pa(padding), 1, block_id);
}
void sd_read_offset(uint64_t mem_address, unsigned block_id, unsigned offset,unsigned len)
{
    sd_read(kva2pa(padding), 1, block_id);
    memcpy((void*)mem_address, padding + offset, len);
}

// alloc new sector or inode
uint32_t alloc_sector()
{
    for (int i=0;i<superblock.sector_map_size;i++)
    {
        sd_read(kva2pa(padding), 1, superblock.sector_map_start+i);
        for (int j=0;j<SECTOR_SIZE;j++)
            if (padding[j] != 0xff)
            {
                for (int k=0;k<8;k++)
                    if ((padding[j] & (1<<k)) == 0)
                    {
                        padding[j] |= (1<<k);
                        sd_write(kva2pa(padding), 1, superblock.sector_map_start+i);
                        superblock.data_free--;
                        return i * SECTOR_SIZE * 8 + j * 8 + k + superblock.data_start;
                    }
            }
    }
    return 0;
}
uint32_t alloc_inode()
{
    for (int i=0;i<superblock.inode_map_size;i++)
    {
        sd_read(kva2pa(padding), 1, superblock.inode_map_start+i);
        for (int j=0;j<SECTOR_SIZE;j++)
            if (padding[j] != 0xff)
            {
                for (int k=0;k<8;k++)
                    if ((padding[j] & (1<<k)) == 0)
                    {
                        padding[j] |= (1<<k);
                        sd_write(kva2pa(padding), 1, superblock.inode_map_start+i);
                        superblock.inode_free--;
                        return i * SECTOR_SIZE * 8 + j * 8 + k;
                    }
            }
    }
    return 0;
}
void release_inode(uint32_t ino)
{
    sd_read(kva2pa(padding),1,superblock.inode_map_start+ino/(8*SECTOR_SIZE));
    padding[ino%(8*SECTOR_SIZE)/8] &= ~(1<<(ino%8));
    sd_write(kva2pa(padding),1,superblock.inode_map_start+ino/(8*SECTOR_SIZE));
}
void release_sector(uint32_t block_id)
{
    sd_read(kva2pa(padding),1,superblock.sector_map_start+block_id/(8*SECTOR_SIZE));
    padding[block_id%(8*SECTOR_SIZE)/8] &= ~(1<<(block_id%8));
    sd_write(kva2pa(padding),1,superblock.sector_map_start+block_id/(8*SECTOR_SIZE));
}

// init different things
void init_file_system(void)
{
    sd_read(kva2pa(&superblock),1,FS_START);
    if (superblock.magic == SUPERBLOCK_MAGIC) current_ino = superblock.root_ino;
        else do_mkfs();
    return;
}
void init_superblock(void)
{
    superblock.magic            = SUPERBLOCK_MAGIC;
    superblock.fs_start         = FS_START;
    superblock.fs_sector_num    = SECTOR_NUM;
    superblock.sector_map_size  = SECTOR_MAP_SIZE;
    superblock.sector_map_start = SECTOR_MAP_START;
    superblock.inode_map_size   = INODE_MAP_SIZE;
    superblock.inode_map_start  = INODE_MAP_START;
    superblock.inode_start      = INODE_START;
    superblock.inode_free       = INODE_NUM;
    superblock.inode_num        = INODE_NUM;
    superblock.inode_sector_num = INODE_SECTOR_NUM;
    superblock.data_start       = DATA_START;
    superblock.data_free        = DATA_NUM;
    superblock.data_num         = DATA_NUM;
    superblock.sector_size      = SECTOR_SIZE;
    superblock.inode_size       = INODE_SIZE;
    superblock.dentry_size      = DENTRY_SIZE;
    superblock.root_ino         = 0;
    sd_write(kva2pa(&superblock), 1 , FS_START);
}
inode_t init_inode(ino_type_t inode_type)
{
    inode_t inode;
    bzero(&inode, superblock.inode_size);
    inode.ino      = alloc_inode();
    inode.type     = inode_type;
    inode.access   = INO_RDWR;
    inode.nlinks   = 1;
    if (inode_type == INO_DIR) 
    {
        inode.file_num = 2;        // . and ..
        inode.used_size = 2 * superblock.dentry_size;
    }
    else 
    {
        inode.file_size = 0;
        inode.used_size = 0;
    }
    inode.create_time = inode.modify_time = get_timer();
    inode.direct_index[0] = alloc_sector();
    reflush_inode(&inode);
    return inode;
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
    inode_t root_inode = init_inode(INO_DIR);
    // init root dentry
    init_dentry(&root_inode,root_inode.ino);
    current_ino = root_inode.ino;
}

// inode related
inode_t get_inode(uint32_t ino)
{
    inode_t inode;
    // 1 sector = 4 inode
    sd_read_offset(&inode, superblock.inode_start + ino / 4, (ino%4)*superblock.inode_size, superblock.inode_size);
    return inode;
}
void reflush_inode(inode_t *inode)
{
    sd_write_offset(inode, superblock.inode_start + inode->ino / 4 ,(inode->ino%4)*superblock.inode_size,superblock.inode_size);
}
uint32_t get_father_ino(uint32_t ino)
{
    inode_t inode = get_inode(ino);
    dentry_t dentry;
    sd_read_offset(&dentry, inode.direct_index[0], superblock.dentry_size, superblock.dentry_size);
    return dentry.ino;
}
int get_son_inode(char *path, inode_t *father_node)
{
    // 1 sector = 16 dentry
    for (int i=0;i<father_node->file_num/16+1;i++)
    {
        sd_read(kva2pa(dir_buffer),1,father_node->direct_index[i]);
        dentry_t *dir = (dentry_t *)dir_buffer;
        for (int j=0;j<min(father_node->file_num-i*16,16);j++)
            if (strcmp(dir[j].name,path)==0) return dir[j].ino;
    }
    return -1;
}
uint32_t get_final_ino(char *path, uint32_t ino,uint32_t offset)
{
    // seperate path
    char argv[10][20];
    int i,start,argc;
    int len = strlen(path);
    i=start=argc=0;
    bzero(argv,200);
    while (i<len)
    {
        if (path[i] == '/')
        {
            strncpy(argv[argc],path+start,i-start);
            start = i+1;
            argc++;
        }
        i++;
    }
    strncpy(argv[argc++],path+start,i-start);
    if (len==0) argc--;
    // use for ln
    strcpy(path,argv[argc-1]);
    argc-=offset;
    // get final inode
    int ino_num = ino;
    inode_t temp_node = get_inode(ino_num);
    for (i=0;i<argc;i++)
    {
        ino_num = get_son_inode(argv[i],&temp_node);
        if (ino_num == -1) return -1;
        temp_node = get_inode(ino_num);
    }
    return ino_num;
}

// set father dir
void set_father_dir(inode_t *father_node, char *name, inode_t *son_inode)
{
    dentry_t dentry;
    dentry.ino = son_inode->ino;
    dentry.type = son_inode->type;
    strcpy(dentry.name, name);
    if (father_node->file_num%16==0) father_node->direct_index[father_node->file_num/16] = alloc_sector();
    sd_write_offset(&dentry, father_node->direct_index[father_node->file_num/16], (father_node->file_num%16)*superblock.dentry_size, superblock.dentry_size);
    father_node->file_num++;
    father_node->modify_time = get_timer();
    father_node->nlinks++;
    father_node->used_size += superblock.dentry_size ;
    reflush_inode(father_node);
}
int remove_son_dir(inode_t *father_node, char *name, ino_type_t type)
{
    int index_i = -1;
    int index_j;
    // 1 sector = 16 dentry
    for (int i=0;i<father_node->file_num/16+1;i++)
    {
        sd_read(kva2pa(dir_buffer),1,father_node->direct_index[i]);
        dentry_t *dir = (dentry_t *)dir_buffer;
        for (int j=0;j<min(father_node->file_num-i*16,16);j++)
            if (strcmp(dir[j].name,name)==0)
            { 
                if (dir[j].type != type) return -1;
                father_node->file_num--;
                father_node->modify_time = get_timer();
                father_node->nlinks--;
                father_node->used_size -= superblock.dentry_size;
                reflush_inode(father_node);
                index_i = i;
                index_j = j;
                inode_t son_node = get_inode(dir[j].ino);
                son_node.nlinks--;
                if (type == INO_DIR || type == INO_FILE && son_node.nlinks==0) release_inode(dir[j].ino);
                    else reflush_inode(&son_node);
                break;
            }
        if (index_i!=-1) break;
    }
    if (index_i==-1) return -1;
    // move last dentry to index;
    sd_read(kva2pa(dir_buffer),1,father_node->direct_index[(father_node->file_num+1)/16]);
    dentry_t *dir = (dentry_t *)dir_buffer;
    sd_write_offset(dir+(father_node->file_num+1)%16-1, father_node->direct_index[index_i], index_j*superblock.dentry_size, superblock.dentry_size);
}

// print time for ls
void print_timer(uint32_t time)
{
    uint32_t hour = time / 3600;
    uint32_t min = (time % 3600) / 60;
    uint32_t sec = time % 60;
    printk("%dh:%dm:%ds ", hour, min, sec);
}

// for big file
unsigned int get_block_id(inode_t* file, int sector_id)
{
    /*
        uint32_t direct_index[1 + 5];  // 3KB
        // 512B = 128 int
        uint32_t indirect_index1[3];   // represent 3*128 direct_index          3*64KB
        uint32_t indirect_index2[2];   // represent 2*128*128 direct_index      2*8MB
        uint32_t indirect_index3;      // represent 128*128*128 direct_index    1GB
    */

    // currently we only support 8MB big file, so we use indirect_index2
    unsigned int block_id=0;
    if (sector_id<6) 
    {
        block_id=file->direct_index[sector_id];
        if (block_id==0) block_id=file->direct_index[sector_id]=alloc_sector();
    }
    else if (sector_id<6+3*128)
    {
        int indirect1_num = (sector_id-6)/128;
        int indirect1_offset = (sector_id-6)%128;
        if (file->indirect_index1[indirect1_num]==0) 
        {
            file->indirect_index1[indirect1_num]=alloc_sector();
            block_id = alloc_sector();
            sd_write_offset(&block_id, file->indirect_index1[indirect1_num], indirect1_offset*4, 4);
        }
        else 
        {
            sd_read_offset(&block_id, file->indirect_index1[indirect1_num], indirect1_offset*4, 4);
            if (block_id==0) 
            {
                block_id = alloc_sector();
                sd_write_offset(&block_id, file->indirect_index1[indirect1_num], indirect1_offset*4, 4);
            }
        }
    }
    else
    {
        int indirect2_num = (sector_id-6-3*128)/128/128;
        int indirect2_offset = (sector_id-6-3*128)%(128*128)/128;
        int indirect1_offset = (sector_id-6-3*128)%128;
        if (file->indirect_index2[indirect2_num]==0)
        {
            file->indirect_index2[indirect2_num]=alloc_sector();
            unsigned int indirect1_id = alloc_sector();
            sd_write_offset(&indirect1_id, file->indirect_index2[indirect2_num], indirect2_offset*4, 4);
            block_id = alloc_sector();
            sd_write_offset(&block_id, indirect1_id, indirect1_offset*4, 4);
        }
        else
        {
            unsigned int indirect1_id;
            sd_read_offset(&indirect1_id, file->indirect_index2[indirect2_num], indirect2_offset*4, 4);
            if (indirect1_id==0)
            {
                indirect1_id = alloc_sector();
                sd_write_offset(&indirect1_id, file->indirect_index2[indirect2_num], indirect2_offset*4, 4);
                block_id = alloc_sector();
                sd_write_offset(&block_id, indirect1_id, indirect1_offset*4, 4);
            }
            else
            {
                sd_read_offset(&block_id, indirect1_id, indirect1_offset*4, 4);
                if (block_id==0)
                {
                    block_id = alloc_sector();
                    sd_write_offset(&block_id, indirect1_id, indirect1_offset*4, 4);
                }
            }
        }
    }
    reflush_inode(file);
    return block_id;
}