#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <asm/syscall.h>

#include "rs.h"

struct dup2_data {
    unsigned int oldfd;
};

static int dup2_entry(struct kretprobe_instance *ri, struct pt_regs *regs){
    struct dup2_data *data = (void *)ri->data;
    struct pt_regs *syscall_regs;
    unsigned long args[6];

    syscall_regs = (struct pt_regs *)regs_get_kernel_argument(regs, 0);
    syscall_get_arguments(current, syscall_regs, args);
    data->oldfd = (unsigned int)args[0];

    return 0;

}

static int dup2_ret(struct kretprobe_instance *ri, struct pt_regs *regs){
    struct dup2_data *data = (void *)ri->data;
    long result = (long)regs_return_value(regs);

    if (result >= 0)
        pr_info_ratelimited("DoorBraker9: dup2 oldfd=%u newfd=%ld\n",data->oldfd, result);

    return 0;
}





static struct kretprobe dup2_probe = {
    .kp.symbol_name = "__x64_sys_dup2",
    .entry_handler = dup2_entry,
    .handler = dup2_ret,
    .data_size = sizeof(struct dup2_data),
};

int register_rs_probe(void)
{
    return register_kretprobe(&dup2_probe);
}

void unregister_rs_probe(void)
{
    unregister_kretprobe(&dup2_probe);
}
