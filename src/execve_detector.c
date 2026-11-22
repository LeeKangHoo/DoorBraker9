#include <linux/binfmts.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/printk.h>
#include <linux/sched.h>

#include "execve_detector.h"
#include "yellow_card.h"


static int on_execve(struct linux_binprm *bprm)
{
    pid_t tgid = task_tgid_nr(current);

    if (redred(tgid)) {
        pr_info_ratelimited("DoorBraker9: detected yellow execve tgid=%d file=%s\n",tgid, bprm->filename);
        return -EPERM;
    }

    return -ENOEXEC;
}

static struct linux_binfmt execve_handler = {
    .module = THIS_MODULE,
    .load_binary = on_execve,
};

void register_execve_detector(void)
{
    insert_binfmt(&execve_handler);
}

void unregister_execve_detector(void)
{
    unregister_binfmt(&execve_handler);
}
