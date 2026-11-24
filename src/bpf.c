#include <linux/kernel.h>
#include <linux/kprobes.h>
#include <linux/ptrace.h>
#include <linux/sched.h>
#include <linux/socket.h>
#include <net/sock.h>

#include "bpf.h"
#include "yellow_card.h"
#include "log.h"

static int sock_ret(struct kretprobe_instance *ri, struct pt_regs *regs)
{
    int result = (int)regs_return_value(regs);
    char comm[TASK_COMM_LEN];
    int tgid = task_tgid_nr(current);

    if (result == 0){
        get_task_comm(comm, current);
        add_yellow_card(tgid);
        event_send(CBPF,tgid);
        pr_info_ratelimited("DoorBraker9: AF_PACKET + cBPF detected!!! pid=%d tid=%d comm=%s ret=%d",tgid, task_pid_nr(current), comm, result);
    }
    

    return 0;

}

static int sock_entry(struct kretprobe_instance *ri, struct pt_regs *regs){
    struct sock *sk;

    sk = (struct sock *)regs_get_kernel_argument(regs,1);

    if (!sk || sk->sk_family != AF_PACKET)
        return 1;

    return 0;
}

static struct kretprobe sock_probe = {
    .kp.symbol_name = "sk_attach_filter",
    .entry_handler = sock_entry,
    .handler = sock_ret,
};

int register_bpf_probe(void)
{
    return register_kretprobe(&sock_probe);
}

void unregister_bpf_probe(void)
{
    unregister_kretprobe(&sock_probe);
}
