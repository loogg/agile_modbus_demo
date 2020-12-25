#include "main_hook.h"
#include <rthw.h>

static rt_slist_t main_hook_module_header = RT_SLIST_OBJECT_INIT(main_hook_module_header);

void main_hook_module_register(struct main_hook_module *module)
{
    rt_base_t level;

    rt_slist_init(&(module->slist));

    level = rt_hw_interrupt_disable();

    rt_slist_append(&main_hook_module_header, &(module->slist));

    rt_hw_interrupt_enable(level);
}

void main_hook_process(void)
{
    rt_slist_t *node;
    rt_slist_for_each(node, &main_hook_module_header)
    {
        struct main_hook_module *module = rt_slist_entry(node, struct main_hook_module, slist);
        if(module->hook)
            module->hook();
    }
}
