#include <linux/module.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/sched.h>


static int sock_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    int result = (int)regs_return_value(regs);
    char comm[TASK_COMM_LEN];

    get_task_comm(comm, current);

    pr_info_ratelimited("DoorBraker9: sk_attach_filter pid=%d tid=%d comm=%s ret=%d",task_tgid_nr(current), task_pid_nr(current), comm, result);

    return 0;

}

static int sock_entry(struct kretprobe_instance *ri, struct pt_regs *regs){
    struct sock *sk;
    
    sk = (struct sock *)regs_get_kernel_argument(regs,1);

    if (!sk || sk->sk_family != AF_PACKET)
        return 1;

    return 0;
}

struct kretprobe sock_probe = {
    .kp.symbol_name = "sk_attach_filter",
    .entry_handler = sock_entry,
    .handler = sock_ret,
};


static int __init detector_init(void){
    int ret;

    ret = register_kretprobe(&sock_probe);
    if (ret){
        pr_err("DoorBraker9 : probe registration failed. code : %d", ret);
        return ret;
    }

    pr_info("DoorBraker9: loaded\n");
    return 0;
}

static void __exit detector_exit(void){
    unregister_kretprobe(&sock_probe);
    pr_info("DoorBraker9: unloaded\n");
}

module_init(detector_init);
module_exit(detector_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TEST DOORBRAKER9");
