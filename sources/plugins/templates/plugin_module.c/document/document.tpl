<$if header$><$header: join(\n)$>

#include <linux/module.h>
<$endif$>#include <kedr/kedr_mem/functions.h>

<$if concat(block)$><$block: join(\n)$>

static struct kedr_repl_pair rp[] =
{
    <$replacement_pair: join(,\n\t)$>
};

<$endif$><$if on_target_load$>
static void on_target_load(struct module* m)
{
    (void)m;
    {
    <$on_target_load$>
    }
}
<$endif$><$if on_target_unload$>
static void on_target_unload(struct module* m)
{
    (void)m;
    {
    <$on_target_unload$>
    }
}
<$endif$><$if on_before_exit$>
static void on_before_exit(struct module* m)
{
#define pc ((unsigned long)m->exit)
#define tid (kedr_get_thread_id())
    (void)m;
    {
    <$on_before_exit$>
    }
#undef pc
#undef tid
}
<$endif$>struct kedr_fh_plugin fh_plugin = {
	.owner = THIS_MODULE,
<$if on_target_load$>    .on_target_loaded = on_target_load,
<$endif$><$if on_target_unload$>    .on_target_about_to_unload = on_target_unload,
<$endif$><$if on_before_exit$>    .on_before_exit_call = on_before_exit,
<$endif$><$if concat(block)$>    .repl_pairs = rp,
<$endif$>};

<$if concat(object.name)$><$object_init_function: join(\n)$>
<$object_destroy_function: join(\n)$>

typedef int (*object_init_function_t)(void);
typedef void (*object_destroy_function_t)(void);

object_init_function_t object_init_functions[] =
{
    <$object_init_function_ref: join(,\n\t)$>
};

object_destroy_function_t object_destroy_functions[] =
{
    <$object_destroy_function_ref: join(,\n\t)$>
};

static int __init plugin_init(void)
{
    int result;
    int i;
    for(i = 0; i < ARRAY_SIZE(object_init_functions); i++)
    {
        result = object_init_functions[i]();
        if(result) goto err;
    }
    
    result = kedr_fh_plugin_register(&fh_plugin);
    if(result) goto err;
    return 0;
err:
    for(--i; i >= 0; --i)
    {
        object_destroy_functions[i]();
    }
    return result;
}

static void __exit plugin_exit(void)
{
    int i;
    kedr_fh_plugin_unregister(&fh_plugin);
    for(i = ARRAY_SIZE(object_destroy_functions) - 1; i >= 0; --i)
    {
        object_destroy_functions[i]();
    }
}

<$else$>static int __init plugin_init(void)
{
    return kedr_fh_plugin_register(&fh_plugin);
}

static void __exit plugin_exit(void)
{
    kedr_fh_plugin_unregister(&fh_plugin);
}
<$endif$>

module_init(plugin_init);
module_exit(plugin_exit);

<$if module.author$>MODULE_AUTHOR("<$module.author$>");
<$endif$><$if module.license$>MODULE_LICENSE("<$module.license$>");
<$endif$>