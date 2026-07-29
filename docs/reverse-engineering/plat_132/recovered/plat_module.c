/*
 * Source-like reconstruction of plat_132.ko module initialization.
 *
 * Evidence: ARM64 code at 0x1c3b4 and direct IDA-calculated callees.
 * This file is semantic reconstruction, not a buildable vendor replacement.
 */

enum zte_pon_cpu_type {
    ZTE_PON_CPUTYPE_132 = 1,
    ZTE_PON_CPUTYPE_133 = 2,
    ZTE_PON_CPUTYPE_129 = 4,
};

extern int g_pon_cputype;

extern int printk(const char *format, ...);
extern int pon_driver_register(void);
extern int nppt_init(void);
extern int sipc_init(void);
extern int greg_init(void);
extern int nppt_smac_init(void);
extern int idm_init(void);

/* Opaque until the vendor platform-driver object is reconstructed. */
struct platform_driver;
struct module;

extern struct platform_driver zx_pon_driver;
extern struct module __this_module;

extern int __platform_driver_register(struct platform_driver *driver,
                                      struct module *owner);

/* IDA entry symbol: init_module. Alternative vendor name: plat_initModule. */
int plat_initModule(void)
{
    int ret;

    g_pon_cputype = ZTE_PON_CPUTYPE_133;
    printk("plat_initModule g_pon_cputype ZTE_PON_CPUTYPE_133\n");

    ret = pon_driver_register();
    if (ret != 0)
        return ret;

    return nppt_init();
}

int pon_driver_register(void)
{
    return __platform_driver_register(&zx_pon_driver, &__this_module);
}

int nppt_init(void)
{
    int ret;

    printk("nppt init start\n");
    ret = sipc_init();
    ret |= greg_init();
    ret |= nppt_smac_init();
    ret |= idm_init();
    printk("nppt init end. ret = %d\n", ret);

    return ret;
}
