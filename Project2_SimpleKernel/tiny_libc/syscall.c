#include <syscall.h>
#include <stdint.h>

static const long IGNORE = 0L;

static long invoke_syscall(long sysno, long arg0, long arg1, long arg2,
                           long arg3, long arg4)
{
    /* TODO: [p2-task3] implement invoke_syscall via inline assembly */
    long result;
    asm volatile(
        "mv a7,%[sysno];\
        mv a0,%[arg0];\
        mv a1,%[arg1];\
        mv a2,%[arg2];\
        mv a3,%[arg3];\
        mv a4,%[arg4];\
        ecall;"
        :[a7]"=r"(result)
        :[sysno]"r"(sysno),
        [arg0]"r"(arg0),
        [arg1]"r"(arg1),
        [arg2]"r"(arg2),
        [arg3]"r"(arg3),
        [arg4]"r"(arg4)
        :"a0","a1","a2","a3","a4","a7"
    );
    return result;
}

void sys_yield(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_yield */
    invoke_syscall(SYSCALL_YIELD,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_move_cursor(int x, int y)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_move_cursor */
    invoke_syscall(SYSCALL_CURSOR,x,y,IGNORE,IGNORE,IGNORE);
}

void sys_write(char *buff)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_write */
    invoke_syscall(SYSCALL_WRITE,(long)buff,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_reflush(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_reflush */
    invoke_syscall(SYSCALL_REFLUSH,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

int sys_mutex_init(int key)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_init */
    return invoke_syscall(SYSCALL_LOCK_INIT,key,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_mutex_acquire(int mutex_idx)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_acquire */
    invoke_syscall(SYSCALL_LOCK_ACQ,mutex_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_mutex_release(int mutex_idx)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_mutex_release */
    invoke_syscall(SYSCALL_LOCK_RELEASE,mutex_idx,IGNORE,IGNORE,IGNORE,IGNORE);
}

long sys_get_timebase(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_timebase */
    return invoke_syscall(SYSCALL_GET_TIMEBASE,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

long sys_get_tick(void)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_get_tick */
    return invoke_syscall(SYSCALL_GET_TICK,IGNORE,IGNORE,IGNORE,IGNORE,IGNORE);
}

void sys_sleep(uint32_t time)
{
    /* TODO: [p2-task3] call invoke_syscall to implement sys_sleep */
    invoke_syscall(SYSCALL_SLEEP,time,IGNORE,IGNORE,IGNORE,IGNORE);
}