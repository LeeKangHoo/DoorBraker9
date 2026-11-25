#include <linux/errno.h>
#include <linux/sched.h>
#include <linux/string.h>
#include <linux/tracepoint.h>

#include "fork_detector.h"
#include "yellow_card.h"
#include "log.h"

static struct tracepoint *fork_tp;

static void find_fork_tracepoint(struct tracepoint *tp, void *data){
    if (strcmp(tp->name, "sched_process_fork") == 0)
        fork_tp = tp;
}

static void on_process_fork(void *data, struct task_struct *parent, struct task_struct *child){
    pid_t parent_tgid = task_tgid_nr(parent);
    pid_t child_tgid = task_tgid_nr(child);

    if (parent_tgid == child_tgid)
        return;

    if (check_yellow_card(parent_tgid)){
        pr_info_ratelimited("DoorBraker9: yellow fork detected parent=%d child=%d\n", parent_tgid, child_tgid);
        event_send(FORK,child_tgid);
        add_yellow_card(child_tgid);
    }
}


int register_fork_probe(void){
    for_each_kernel_tracepoint(find_fork_tracepoint, NULL);
    if (!fork_tp)
        return -ENOENT;
    return tracepoint_probe_register(fork_tp, on_process_fork, NULL);
}

void unregister_fork_probe(void){
    tracepoint_probe_unregister(fork_tp, on_process_fork, NULL);
    tracepoint_synchronize_unregister();
}
