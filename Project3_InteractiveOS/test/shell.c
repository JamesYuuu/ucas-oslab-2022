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

#define SHELL_BEGIN 20
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
    }

    return 0;
}

void init_buffer()
{
    len=0;
    bzero(command_buffer,BUFFER_MAX);
    for (int i = 0; i < argc; ++i)
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
        sys_clear();
        sys_move_cursor(0, SHELL_BEGIN);
        printf("------------------- COMMAND -------------------\n");
        return;
    }

    // unknown command
    printf("Error: Unknown command: %s\n",argv[0]);
    return;
}