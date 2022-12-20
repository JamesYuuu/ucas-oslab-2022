# Project 6 File System

## 任务一：物理文件系统的实现

### 1.1 文件系统相关定义

我们的文件系统从磁盘空间的512MB开始，共占据512MB空间，布局如下：

```
File System Layout

      1 sector     superblock
    256 sector     sector map   
      1 sector     inode map
   1024 sector     inode array
1047295 sector     data
```

由此定义相关宏如下，用于保存在superblock中

```c
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
```

`superblock`中基本用于存储相关信息，此外增加部分内容，详细如下：

```c
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
```

`inode`、`dentry_t`、`fdesc_t`相关信息仿照Linux系统设计，展示如下：

```c
typedef enum ino_type{
    INO_DIR,
    INO_FILE,
    INO_LINK,
} ino_type_t;

typedef enum ino_access{
    INO_RDONLY,
    INO_WRONLY,
    INO_RDWR,
} ino_access_t;

typedef struct dentry_t{
    // TODO [P6-task1]: Implement the data structure of directory entry
    uint32_t ino;
    ino_type_t type;
    char name[12];
    char dst_name[12];
} dentry_t;

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
```

### 1.2 `mkfs`命令的实现

首先初始化文件系统的时候，我们先读取`superblock`，如果`magic`满足条件，则无需初始化文件系统，否则我们利用`mkfs`命令初始化文件系统。

```c
// init different things
void init_file_system(void)
{
    sd_read(kva2pa(&superblock),1,FS_START);
    if (superblock.magic == SUPERBLOCK_MAGIC) current_ino = superblock.root_ino;
        else do_mkfs();
    return;
}
```

除去打印相关信息外，`mkfs`命令需要完成以下三件事：
1. 初始化`superblock`，并写入磁盘
2. 将`inode_map`和`sector_map`初始化为全0，写入磁盘
3. 初始化`root`目录，写入磁盘

```c
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
```

其中`init_superblock`函数中，我们把相关宏填充到`superblock`中，然后写入磁盘。

```c
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
```

在初始化`root`目录的时候，我们需要初始化`inode`和`dentry`，其中我们使用的`init_inode`函数和`init_dentry`函数将在之后大量的使用，在之后的代码中不再赘述。

```c
void init_root_dir(void)
{
    // init inode
    inode_t root_inode = init_inode(INO_DIR);
    // init root dentry
    init_dentry(&root_inode,root_inode.ino);
    current_ino = root_inode.ino;
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
```

### 1.3 `statfs`命令的实现

在`statfs`命令的实现中，我们同时刷新`superblock`，用于保证文件系统的完整性

```c
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
```

### 1.4 `cd`命令的实现

我们的`cd`命令需要实现多级索引，因此我们需要简单的分割路径，并获得最后一级目录的`inode`号，我们通过函数`get_final_inode`实现，其中我们反复调用`get_son_inode`函数，该函数用于索引一级目录。

```c
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
```

有了`get_final_inode`函数后，实现`cd`命令就很简单了，我们只需要判断路径是否合法，然后刷新`current_ino`即可。

```c
int do_cd(char *path)
{
    // TODO [P6-task1]: Implement do_cd
    uint32_t ino_num = get_final_ino(path,current_ino,0);
    if (ino_num == -1) return -1;
        else current_ino = ino_num;
    return 0;  // do_cd succeeds
}
```

### 1.5 `mkdir`命令的实现

在`mkdir`命令中，我们需要实现初始化新文件夹，并设置父文件夹。在初始化新文件夹时，我们通过之前使用过的`init_dentry`函数实现；在设置父文件夹时，我们通过`set_father_dir`函数实现，具体要做的就是增加子文件夹的目录索引。

```c
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
```

### 1.6 `rmdir`命令的实现

`rmdir`命令和`mkdir`命令相反，但实现基本类似，我们具体通过`remove_son_dir`函数实现。

```c
int do_rmdir(char *path)
{
    // TODO [P6-task1]: Implement do_rmdir
    inode_t father_inode = get_inode(current_ino);
    // remove son dir
    return remove_son_dir(&father_inode,path,INO_DIR);
    //return 0;  // do_rmdir succeeds
}
```

在填补`dentry`的空缺上，我采用的策略是将最后一个`dentry`填补到空缺的部分，是一个比较简单的策略，但是会导致`dentry`的顺序发生变化，并不影响结果的正确。

```c
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
```

### 1.7 `ls`命令的实现

`ls`的命令需要支持多级索引以及`-l`选项，用于输出详细信息，我们也是利用`get_final_ino`函数得到最终的`inode`，然后遍历`dentry`，输出相关信息。

```c
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
```

## 任务二：文件操作

### 2.1 `touch`指令的实现

`touch`指令和`mkdir`指令几乎相同，只是`touch`指令创建的是文件，而不是目录，因此只需要将`init_inode`中的`INO_DIR`改为`INO_FILE`即可。

```c
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
```

### 2.2 `cat`指令的实现

`cat`指令中，我们只需简单的读取文件并打印相关内容即可，由于我们的输出`buffer`有限，因此我们只考虑直接索引部分的内容。

```c
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
```

### 2.3 `fopen`指令的实现

`fopen`指令为打开一个文件，并返回一个文件描述符，我们需要在找到一个空闲的文件描述符，并将其初始化，然后将其`inode`设置为`path`对应的`inode`、`offset`设置为0。

```c
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
```

### 2.4 `fwrite`指令的实现

`fwrite`指令为向一个文件中写入内容，我们需要先判断文件是否可写，然后根据`offset`和`file_size`判断是否需要分配新的磁盘块，最后将内容写入磁盘。写入后，我们需要刷新`inode`的相关信息。

```c
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
```

注意上面的`get_block_id`函数，用于根据其实块号，返回实际的磁盘块号，如果该磁盘块不存在，则分配一个新的磁盘块。因为目前我们需要实现大文件的支持，即至少实现2级索引，在该函数中，我们需要判断该块出于几级索引，并返回真实的磁盘块号。

```c
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
```

### 2.5 `fread`指令的实现

`fread`指令的实现与`fwrite`指令类似，只是将数据从磁盘读取到内存中，而不是从内存写入到磁盘中。

```c
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
```

### 2.6 `fclose`指令的实现

`fclose`指令的实现与`fopen`指令类似，只需将文件描述符的状态设置为`FREE`即可。

```c
int do_fclose(int fd)
{
    // TODO [P6-task2]: Implement do_fclose
    fdesc_array[fd].is_used = FREE;
    return 0;  // do_fclose succeeds
}
```

## 任务三：S-core 和 A-core 新增的指令

### 3.1 `ln`指令的实现

`ln`指令中，我们需要实现多级索引，仍然使用的是`get_final_inode`函数，此外我们需要新建一个`link`类型的`dentry`，代表是作为硬链接的文件，并将其`inode`设置为目标文件的`inode`

```c
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
```

### 3.2 `rm`指令的实现

`rm`指令中，我们的实现和`rmdir`类似，暂时没有实现回收资源的功能。

```c
int do_rm(char *path)
{
    // TODO [P6-task2]: Implement do_rm
    inode_t father_node = get_inode(current_ino);
    int file_result = remove_son_dir(&father_node,path,INO_FILE);  // do_rm succeeds 
    int link_result = remove_son_dir(&father_node,path,INO_LINK);  // do_rm succeeds
    return link_result == -1 && file_result == -1 ? -1 : 0;
}
```

### 3.3 `ls -l`指令和大文件的支持

这俩要求在之前已经实现，不再赘述。



