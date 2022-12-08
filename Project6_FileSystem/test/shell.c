/* * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *            Copyright (C) 2018 Institute of Computing Technology, CAS
 *               Author : Han Shukai (email : hanshukai@ict.ac.cn)
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *                  The shell acts as a task running in user mode.
 *       The main function is to make system calls through the user's output.
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * *
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy of this
 * software and associated documentation files (the "Software"), to deal in the Software
 * without restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * * *  * * * * * * * * * * */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <ctype.h>

#define SHELL_BEGIN 25
#define BUFFER_MAX 80
#define ARG_MAX 10
#define ARG_LEN 20

char command_buffer[BUFFER_MAX];
char argv[ARG_MAX][ARG_LEN];
int len,argc;

void execute_command();
void init_buffer();
void seperate_command();

int main(void)
{
    int input;
    sys_move_cursor(0, SHELL_BEGIN);
    printf("------------------- COMMAND -------------------\n");
    init_buffer();
    while (1)
    {
        // TODO [P3-task1]: call syscall to read UART port
        input = sys_getchar();
        // if there is no input, continue
        if (input == -1) continue;
        // TODO [P3-task1]: parse input
        // note: backspace maybe 8('\b') or 127(delete)
        // if their is a backspace, delete the last character
        if ((input == 8 || input == 127) && (len>0))
        {
            command_buffer[--len]=0;
            printf("\b");
            continue;
        }
        // if the input is enter, execute the command
        if (input == 13 || len>BUFFER_MAX)
        {
            printf("\n");
            execute_command();
            init_buffer();
            continue;
        }

        command_buffer[len++] = input;
        printf("%c",input);

        // TODO [P3-task1]: ps, exec, kill, clear        

        // TODO [P6-task1]: mkfs, statfs, cd, mkdir, rmdir, ls

        // TODO [P6-task2]: touch, cat, ln, ls -l, rm
    }

    return 0;
}

void init_buffer()
{
    len=0;
    bzero(command_buffer,BUFFER_MAX);
    for (int i = 0; i < ARG_MAX; ++i)
        bzero(argv[i],ARG_LEN);
    argc=0;
    printf("> root@UCAS_OS: ");
    return;
}

void seperate_command()
{
    int i,start;
    i=start=0;
    while (i<len)
    {
        if (command_buffer[i] == ' ')
        {
            strncpy(argv[argc],command_buffer+start,i-start);
            start = i+1;
            argc++;
        }
        i++;
    }
    strncpy(argv[argc++],command_buffer+start,i-start);
    return;
}

void execute_command()
{
    seperate_command(command_buffer);
    // command clear
    if (strcmp(argv[0],"clear")==0)
    {
        int clear_line=SHELL_BEGIN;
        if (argc!=1) clear_line=atoi(argv[1]);
        sys_clear(clear_line);
        if (clear_line<=SHELL_BEGIN)
        {
            sys_move_cursor(0, SHELL_BEGIN);
            printf("------------------- COMMAND -------------------\n");
        }
        else
            sys_move_cursor(0, clear_line);
        return;
    }
    // command ps
    if (strcmp(argv[0],"ps")==0)
    {
        sys_ps();
        return;
    }
    // command exec
    if (strcmp(argv[0],"exec")==0)
    {
        // check if waitpid is needed
        int need_wait=1; 
        if (strcmp(argv[argc-1],"&")==0)
        {
            argc--;
            need_wait=0;
        }
        // remove 'exec' from argv
        char *p[argc-1];
        for (int i=1;i<argc;i++)
            p[i-1]=argv[i];
        int pid=sys_exec(argv[1],argc-1,p);
        if (pid!=0) printf("Info: Execute %s successfully, pid=%d\n",argv[1],pid);
            else printf("Error: Execute %s failed\n",argv[1]);
        if (need_wait) sys_waitpid(pid);
        return;
    }
    // command kill
    if (strcmp(argv[0],"kill")==0)
    {
        int pid=atoi(argv[1]);
        if (sys_kill(pid)!=0) printf("Info: Kill process %d successfully\n",pid);
            else printf("Error: Kill process %d failed\n",pid);
        return;
    }
    // command taskset
    if (strcmp(argv[0],"taskset")==0)
    {
        if (strcmp(argv[1],"-p")==0)
        {
            int mask;
            if (strcmp(argv[2],"0x1")==0) mask=1;
                else if (strcmp(argv[2],"0x2")==0) mask=2;
                    else mask=3;
            int pid=atoi(argv[3]);
            if (sys_taskset_p(pid,mask)!=0) printf("Info: Set process %d's mask to %s successfully\n",pid,argv[2]);
                else printf("Error: Set process %d's mask to %s failed\n",pid,argv[2]);
        }
        else
        {
            int mask;
            if (strcmp(argv[1],"0x1")==0) mask=1;
                else if (strcmp(argv[1],"0x2")==0) mask=2;
                    else mask=3;
            char *name = argv[2];
            char *p[argc-2];
            for (int i=2;i<argc;i++)
                p[i-2]=argv[i];
            int pid = sys_taskset(name,argc-2,p,mask);
            if (pid!=0) printf("Info: Execute %s and set it's mask to %s successfully, pid=%d\n",name,argv[1],pid);
                else printf("Error: Execute %s and set it's mask to %s failed\n",name,argv[1]);
        }
        return;
    }
    // command mkfs
    if (strcmp(argv[0],"mkfs")==0)
    {
        if (sys_mkfs()!=0) printf("Error: Make file system failed\n");
        return;
    }
    // command statfs
    if (strcmp(argv[0],"statfs")==0)
    {
        sys_statfs();
        return;
    }
    // command mkdir
    if (strcmp(argv[0],"mkdir")==0)
    {
        if (sys_mkdir(argv[1])!=0) printf("Error: Make directory %s failed\n",argv[1]);
        return;
    }
    // unknown command
    printf("Error: Unknown command: %s\n",argv[0]);
    return;
}