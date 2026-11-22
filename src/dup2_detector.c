#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <asm/syscall.h>
#include <linux/file.h>
#include <linux/net.h>
#include <net/sock.h>

#include "dup2_detector.h"
#include "yellow_card.h"

struct dup2_data {
    unsigned int oldfd;
    unsigned int newfd;
};

static bool is_network_socket(unsigned int fd){
    struct file *file;
    struct socket *sock;
    bool is_inet = false;

    file = fget(fd);
    if (!file)
        return false;

    sock = sock_from_file(file);
    if (sock && sock->sk) {
        unsigned short family = sock->sk->sk_family;

        is_inet = family == AF_INET || family == AF_INET6;
    }

    fput(file);
    return is_inet;
}


static int dup2_entry(struct kretprobe_instance *ri, struct pt_regs *regs){
    struct dup2_data *data = (void *)ri->data;
    struct pt_regs *syscall_regs;
    unsigned long args[6];

    syscall_regs = (struct pt_regs *)regs_get_kernel_argument(regs, 0);
    syscall_get_arguments(current, syscall_regs, args);
    data->oldfd = (unsigned int)args[0];
    data->newfd = (unsigned int)args[1];

    return 0;

}

static int dup2_ret(struct kretprobe_instance *ri, struct pt_regs *regs){
    struct dup2_data *data = (void *)ri->data;
    int c_pid = task_tgid_nr(current);
    long result = (long)regs_return_value(regs);

    if (result < 0 || data->newfd > 1 || data->oldfd == data->newfd)
        return 0;

    if (check_yellow_card(c_pid) && is_network_socket(data->oldfd)){
        if (mark_yellow_card(c_pid,data->newfd))
            pr_info_ratelimited("DoorBraker9: detected dup2. network socket to %d tgid=%d\n", data->newfd, c_pid);
    }

    // if (result >= 0)
    //     pr_info_ratelimited("DoorBraker9: dup2 oldfd=%u newfd=%ld\n",data->oldfd, result);

    return 0;
}





static struct kretprobe dup2_probe = {
    .kp.symbol_name = "__x64_sys_dup2",
    .entry_handler = dup2_entry,
    .handler = dup2_ret,
    .data_size = sizeof(struct dup2_data),
};

int register_dup2_probe(void)
{
    return register_kretprobe(&dup2_probe);
}

void unregister_dup2_probe(void)
{
    unregister_kretprobe(&dup2_probe);
}
