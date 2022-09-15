# Project1 BootLoader

## 任务1：第一个引导块的制作

在bootblock.S中，通过寄存器a0,a1,a2...传递参数，调用BIOS API，实现在屏幕上打印字符串。

```asm
    li a0,BIOS_PUTSTR
    la a1,msg
    call bios_func_entry
```

## 任务2：开始制作镜像文件

在bootblock.S中，将kernel拷贝至内存中，kernel的大小存放在os_size_loc所在地址中。

```asm
    li a0,BIOS_SDREAD
    la a1,kernel        //mem_address
    la a4,os_size_loc
    lh a2,(a4)          //num_of_blocks
    li a3,1             //block_id
    call bios_func_entry
```

跳转到kernel的main函数的入口地址，执行kernel。

```asm
    la a0,kernel
    jr a0
```

在head.S文件中，清空bss段，设置栈帧，为kernel的运行做好准备。其中bss的起始地址和结束地址均通过链接器脚本riscv.lds传递。

```asm
    la s1,__bss_start
    la s2,__BSS_END__
    bge s1,s2,end
loop:
    sw zero,(s1)
    addi s1,s1,4
    blt s1,s2,loop
end:
    li sp,KERNEL_STACK
    call main
```

在main函数中实现回显，通过bios_getchar()可以返回当前输入符的ascii码，换行的时候需要额外进行处理，由于读入的是换行符，但回显的时候需要打印换行符和回车符，使光标回到行首。

```c
    int input;
    while (1)
    {
        input=bios_getchar();
        if (input==-1) 
            continue;
        else if (input=='\n')         // when input is enter
            bios_putstr("\n\r");  
        else
            bios_putchar(input);
    }
```
### 任务3：加载并选择启动多个用户程序之一

在createimage.c中，将kernel和用户程序拷贝至镜像文件中，为了方便后续kernel加载用户程序，默认用户程序和kernel都占据镜像文件中15个block的大小，空出的部分用0进行填充，可利用提供的write_padding函数进行处理。

```c
if (strcmp(*files, "bootblock") == 0) write_padding(img, &phyaddr, SECTOR_SIZE);
    else write_padding(img, &phyaddr, (1+fidx*BLOCK_NUM)*SECTOR_SIZE);
```

在loader.c中，根据task_id，将对应的用户程序所在的15个block拷贝到内存对应的entry_point处，并将entry_point返回main函数，通过main函数调用对应的用户程序入口。

```c
uint64_t load_task_img(int task_id)
{
    unsigned int current_address=TASK_MEM_BASE+TASK_SIZE*(task_id-1);
    unsigned int block_id=1+task_id*BLOCK_NUM; 
    bios_sdread(current_address,BLOCK_NUM,block_id);
    return (uint64_t)current_address;
}
```

在main函数中，通过task_id的值，调用load_task_img函数，将对应的用户程序拷贝到内存中，并将entry_point返回，通过entry_point跳转到用户程序的入口。

```c
    int input,task_id;
    long current_address;
    task_id=0;
    while (1)
    {
        input=bios_getchar();
        if (input==-1) 
            continue;
        else if (input=='\n')         // when input is enter
        {
            bios_putstr("\n\r");
            if (task_id>=1 && task_id<=task_num) //make sure the task_id is valid 
            {
                current_address=load_task_img(task_id);
                (*(void(*)())current_address)();    //execute a pointer point to a function address
            }
            else bios_putstr("Invalid task id!\n\r");
            task_id=0;
        }
        else
        {
            bios_putchar(input);
            task_id=task_id*10+input-'0';
        }
    }
```

在crt0.S中为用户程序函数准备C的运行时环境，即清空bss段，设置栈帧。并在用户程序完成后，回收栈帧，返回main函数。

```asm
    // clear bss segment
    la s1,__bss_start
    la s2,__BSS_END__
    bge s1,s2,end
loop:
    sw zero,(s1)
    addi s1,s1,4
    blt s1,s2,loop
end:
    // set up stack pointer and enter the main function
    addi sp,sp,-16
    sw ra,12(sp)
    call main
    // recover stack pointer and return to main function
    lw ra,12(sp)
    addi sp,sp,16
    ret
```

## 任务4：镜像文件压缩

设计task_info_t结构体，用于存储用户程序的相关信息，包括用户程序的名字，用户程序的入口地址，用户程序在镜像文件中的起始位置和结束位置。

```c
typedef struct {
    char task_name[NAME_MAXNUM];
    uint64_t entry_point;
    uint32_t start_addr;
    uint32_t end_addr;
} task_info_t;
```

利用用户程序在镜像文件中的起始位置和结束位置，可以计算出用户程序的大小，用户程序在镜像文件的起始block、在起始block中的起始位置offset和占据的block数。利用这些信息，可以将用户程序从镜像文件中读取出来，存储到内存中的对应位置，接着再将用户程序整体向前移动offset个字节，使得用户程序的起始位置与entry_point对齐。

修改loader.c中的load_task_img函数如下。

```c
uint64_t load_task_img(char *taskname, int task_num)
{
    for (int i=0;i<task_num;i++)
    {
        if (strcmp(taskname,tasks[i].task_name)==0)
        {   
            int offset=tasks[i].start_addr%512;
            int block_num=(tasks[i].end_addr-1)/512+1-tasks[i].start_addr/512;
            int block_id=tasks[i].start_addr/512;
            int task_size=tasks[i].end_addr-tasks[i].start_addr;
            bios_sdread(tasks[i].entry_point,block_num,block_id);
           
            // The code below copy the app to the original place if needed
            uint8_t *dest=(uint8_t*)tasks[i].entry_point;
            uint8_t *src=(uint8_t*)(tasks[i].entry_point+offset);
            memcpy(dest,src,task_size);
            return tasks[i].entry_point;     
        }
    }
    return -1;
}
```

修改main函数，改为根据task_name来调用load_task_img函数，并添加相关异常处理，例如task_name名称不正确，task_name名字过长等错误。

```c
    char task_name[NAME_MAXNUM];
    int input,len;
    unsigned long current_address;
    while(1)
    {
        input=bios_getchar();
        if (input==-1) 
            continue;
        else if (input=='\n')
        {
            if (len==0)
            {
                bios_putstr("Please input task name\n\r");
                continue;
            }
            bios_putstr("\n\r");
            current_address=load_task_img(task_name,task_num);
            if (current_address==-1)
                bios_putstr("Invalid task name!\n\r");
            else 
                (*(void(*)())current_address)();
            for (int i=0;i<=len;i++)
                task_name[i]=0;
            len=0;
        }
        else 
        {
            bios_putchar(input);
            task_name[len]=input;
            len++;
            if (len>=NAME_MAXNUM)
            {
                bios_putstr("Task name too long!\n\r");
                for (int i=0;i<=len;i++)
                    task_name[i]=0;
                len=0;
            }
        }
    }
```

修改createimage.c中的write_img_info函数，将app info块写在所有的用户程序之后，并将app info块的相关信息，包括task的总数，app_info块起始的block和起始位置，app_info块总共占据的block数目；并保存在bootblock块中的固定位置，便于kernel读取。

```c
static void write_img_info(int nbytes_kern, task_info_t *taskinfo,
                           short tasknum, FILE * img , int info_address)
{
    fseek(img,OS_SIZE_LOC-16,SEEK_SET);
    // write app_info before OS_SIZE_LOC for 16 bytes info
    int block_id=info_address/512;
    int offset=info_address%512;
    int block_num=(info_address+sizeof(task_info_t)*tasknum-1)/512+1-block_id;
    fwrite(&tasknum,sizeof(int),1,img);
    fwrite(&block_id,sizeof(int),1,img);
    fwrite(&offset,sizeof(int),1,img);
    fwrite(&block_num,sizeof(int),1,img);   
    // write os_size
    int os_size=(nbytes_kern-1)/512+1;
    fwrite(&os_size,2,1,img);   
    // write app_info
    fseek(img,0,SEEK_END);
    for (int i=0;i<tasknum;i++)
        fwrite(&taskinfo[i],sizeof(task_info_t),1,img);
}
```

相对应的，kernel通过init_task_info函数，读取bootblock块中app_info的信息，并初始化tasks数组。

```c
static void init_task_info(void)
{
    // NOTE: You need to get some related arguments from bootblock first
    bios_sdread(TASK_ADDRESS, 1, 0);
    task_num=*((int*)(TASK_ADDRESS+512-20));
    int block_id=*((int*)(TASK_ADDRESS+512-16));
    int offset=*((int*)(TASK_ADDRESS+512-12));
    int block_num=*((int*)(TASK_ADDRESS+512-8));
    // Init tasks
    bios_sdread(TASK_ADDRESS, block_num, block_id);
    for (int i=0;i<task_num;i++)
    {
        tasks[i]=*((task_info_t*)(unsigned long)(TASK_ADDRESS+offset));
        offset+=sizeof(task_info_t);
    }
}
```
## 任务5：批处理运行多个用户程序

批处理运行用户程序，是在任务4的基础上继续，由用户输入task_name修改为kernel读取保存好的批处理文件，自动根据批处理文件中的task_name来运行用户程序，修改对应的main函数如下。

```c
    char task_name[NAME_MAXNUM];
    int len;
    unsigned long current_address;
    bios_putstr("\n\rStart executing bat file!\n\r");
    while (bat_size-->=0)
    {
        if (*bat_file=='\n')
        {
            if (len==0)
            {
                bios_putstr("Please input task name\n\r");
                continue;
            }
            current_address=load_task_img(task_name,task_num);
            if (current_address==-1)
                bios_putstr("Invalid task name!\n\r");
            else 
            {
                bios_putstr("Executing task ");
                bios_putstr(task_name);
                bios_putstr("...\n\r");    
                (*(void(*)())current_address)();
                bios_putstr("\n\r");
            }
            for (int i=0;i<=len;i++)
                task_name[i]=0;
            len=0;            
        }
        else
        {
            task_name[len]=*bat_file;
            len++;
            if (len>=NAME_MAXNUM)
            {
                bios_putstr("Task name too long!\n\r");
                bios_putstr(task_name);
                for (int i=0;i<=len;i++)
                    task_name[i]=0;
                len=0;
            }
        }
        bat_file++;
    }
    bios_putstr("Finish executing bat file!\n\r");
```

在createimage时，将批处理文件的内容写入app info块的后面，并将批处理文件的大小写在bootblock块的固定位置，便于kernel读取，修改对应的write_img_info函数，添加如下代码。

```c
    // write bat_info if needed for [p1-task5]
    FILE *fp=fopen(BAT_FILE,"r");
    if (fp!=NULL)
    {
        fseek(fp,0,SEEK_END);
        long bat_size=ftell(fp)+1;  //add '\n' at the end of bat file
        fseek(img,OS_SIZE_LOC-24,SEEK_SET);
        fwrite(&bat_size,sizeof(long),1,img);
    }
    else 
        fseek(img,OS_SIZE_LOC-16,SEEK_SET);
```
新增加write_bat_info函数，将批处理文件的内容写入app info块的后面。

```c
// writing bat_info for [p1-task5]
static void write_bat_info(FILE * img)
{
    FILE *fp=fopen(BAT_FILE,"r");
    char c;
    while ((c=fgetc(fp))!=EOF)
        fputc(c,img);
    fputc('\n',img);
}
```

相应的修改kernel中的init_task_info函数，读取批处理文件的大小，并将批处理文件的开头地址保存在bat_file指针中。

```c
static void init_task_info(void)
{
    bios_sdread(TASK_ADDRESS, 1, 0);
    task_num=*((int*)(TASK_ADDRESS+512-20));
    int block_id=*((int*)(TASK_ADDRESS+512-16));
    int offset=*((int*)(TASK_ADDRESS+512-12));
    int block_num=*((int*)(TASK_ADDRESS+512-8));

    // get bat_size for [p1-task5]
    bat_size=*((long*)(TASK_ADDRESS+512-28));

    bios_sdread(TASK_ADDRESS, block_num, block_id);
    for (int i=0;i<task_num;i++)
    {
        tasks[i]=*((task_info_t*)(unsigned long)(TASK_ADDRESS+offset));
        offset+=sizeof(task_info_t);
    }

    // init bat_file for [p1-task5]
    bat_file=(char*)(unsigned long)(TASK_ADDRESS+offset);
}
```
