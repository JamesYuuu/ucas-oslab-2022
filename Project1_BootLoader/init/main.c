#include <common.h>
#include <asm.h>
#include <os/bios.h>
#include <os/task.h>
#include <os/string.h>
#include <os/loader.h>
#include <type.h>

#define VERSION_BUF 50

int version = 2; // version must between 0 and 9
char buf[VERSION_BUF];

// Task info array
task_info_t tasks[TASK_MAXNUM];

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
        else if (input==13)         // when input is enter
            bios_putstr("\n\r");  
        else
            bios_putchar(input);
    }
    */

    // TODO: Load tasks by either task id [p1-task3] or task name [p1-task4],
    //   and then execute them.
    // The code below load tasks by task id for [p1-task3]
    int input,task_id;
    long current_address;
    task_id=0;
    while (1)
    {
        input=bios_getchar();
        if (input==-1) 
            continue;
        else if (input==13)         // when input is enter
        {
            bios_putstr("\n\r");
            if (task_id>=1 && task_id<=4)
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
    


    // Infinite while loop, where CPU stays in a low-power state (QAQQQQQQQQQQQ)
    while (1)
    {
        asm volatile("wfi");
    }

    return 0;
}
