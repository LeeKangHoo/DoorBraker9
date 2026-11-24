#pragma once 
#include <linux/types.h>


enum log_flag{
    CBPF = 1,
    DUP2,
    EXECVE,
};

void event_send(enum log_flag flag, pid_t tgid);

void log_exit(void);

