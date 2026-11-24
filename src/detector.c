#include <linux/module.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/sched.h>
#include <linux/tracepoint.h>

#include "bpf.h"
#include "yellow_card.h"
#include "dup2_detector.h"
#include "fork_detector.h"
#include "execve_detector.h"
#include "log.h"


// 프로세스 종료 관련 tracepoint 관리
static struct tracepoint *exit_tp;

static void find_exit_tracepoint(struct tracepoint *tp, void *data){
    if (strcmp(tp->name,"sched_process_exit") == 0)
        exit_tp = tp;
}

static void on_process_exit(void *data, struct task_struct *task, bool group_dead){
    if (!group_dead)
        return;

    remove_yellow_card(task_tgid_nr(task));
}

// 동작 함수들


static int __init detector_init(void)
{
    int ret;

    for_each_kernel_tracepoint(find_exit_tracepoint, NULL);
    if (!exit_tp)
        return -ENOENT;

    ret = tracepoint_probe_register(exit_tp, on_process_exit, NULL);
    if (ret)
        return ret;

    ret = register_bpf_probe();
    if (ret) 
        return ret;
    

    ret = register_dup2_probe();
    if (ret)
        return ret;

    ret = register_fork_probe();
    if (ret)
        return ret;


    register_execve_detector();


    pr_info("DoorBraker9: loaded\n");
    return 0;
}

static void __exit detector_exit(void){
    unregister_bpf_probe();
    tracepoint_probe_unregister(exit_tp, on_process_exit, NULL);
    tracepoint_synchronize_unregister();
    unregister_dup2_probe();
    unregister_fork_probe();
    unregister_execve_detector();
    clear_yellow_card();
    log_exit();
    pr_info("DoorBraker9: unloaded\n");
}

module_init(detector_init);
module_exit(detector_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("TEST DOORBRAKER9");

