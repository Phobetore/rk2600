#ifndef HOOKING_H
#define HOOKING_H

#include <linux/ftrace.h>

struct ftrace_hook {
    const char *name;
    void *function;    // Fonction de remplacement
    void *original;    // Pointeur vers l'adresse originale (à caster ensuite)
    unsigned long address;
    struct ftrace_ops ops;
};

#define HOOK(_name, _hook, _orig) { \
    .name = (_name),                \
    .function = (_hook),            \
    .original = (_orig)             \
}

int install_hook(struct ftrace_hook *hook);
void remove_hook(struct ftrace_hook *hook);
int install_hooks(struct ftrace_hook *hooks, size_t count);
void remove_hooks(struct ftrace_hook *hooks, size_t count);

#endif
