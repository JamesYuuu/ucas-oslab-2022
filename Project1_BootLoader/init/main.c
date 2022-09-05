#include <common.h>
#include <asm.h>
#include <os/bios.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

#define TASK_ADDRESS 0x52100000

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];
int task_num; //task_num for [p1-task4]

// bat_file info for [p1-task5]
long bat_size;
char *bat_file;

static int bss_check(void)
{
    for (int i = 0; i < VERSION_BUF; ++i)
    {
        if (buf[i] != 0)
        {
            return 0;
        }
    }
    return 1;
}

static void init_bios(void)
{
    volatile long (*(*jmptab))() = (volatile long (*(*))())BIOS_JMPTAB_BASE;

    jmptab[CONSOLE_PUTSTR]  = (long (*)())port_write;
    jmptab[CONSOLE_PUTCHAR] = (long (*)())port_write_ch;
    jmptab[CONSOLE_GETCHAR] = (long (*)())port_read_ch;
    jmptab[SD_READ]         = (long (*)())sd_read;
}

static void init_task_info(void)
{
    // TODO: [p1-task4] Init 'tasks' array via reading app-info sector
    // NOTE: You need to get some related arguments from bootblock first
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

int main(void)
{
    // Check whether .bss section is set to zero
    int check = bss_check();

    // Init jump table provided by BIOS (ΦωΦ)
    init_bios();

    // Init task information (〃'▽'〃)
    init_task_info();

    // Output 'Hello OS!', bss check result and OS version
    char output_str[] = "bss check: _ version: _\n\r";
    char output_val[2] = {0};
    int i, output_val_pos = 0;

    output_val[0] = check ? 't' : 'f';
    output_val[1] = version + '0';
    for (i = 0; i < sizeof(output_str); ++i)
    {
        buf[i] = output_str[i];
        if (buf[i] == '_')
        {
            buf[i] = output_val[output_val_pos++];
        }
    }

    bios_putstr("Hello OS!\n\r");
    bios_putstr(buf);

    // Add read and print support for [p1-task2]
    // If there is no input, bios_getchar() will return -1
    // The code below outputs what it reads
    /*
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
    */

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    // The code below load tasks by task id for [p1-task3]
    /*
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
            if (task_id>=1 && task_id<=task_num)
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
    */
    /*
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
    */

    // running bat file for [p1-task5]
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

    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
