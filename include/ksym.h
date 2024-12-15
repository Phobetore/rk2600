#ifndef KSYM_H
#define KSYM_H

#include <linux/types.h>

typedef unsigned long (*kallsyms_lookup_name_t)(const char *name);

extern kallsyms_lookup_name_t ksym_lookup_name;

int ksym_init(void);
void ksym_cleanup(void);

#endif
