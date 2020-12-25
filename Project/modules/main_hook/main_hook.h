#ifndef __MAIN_HOOK_H
#define __MAIN_HOOK_H
#include <rtthread.h>

struct main_hook_module
{
    void(*hook)(void);
    rt_slist_t slist;
};

void main_hook_module_register(struct main_hook_module *module);
void main_hook_process(void);

#endif
