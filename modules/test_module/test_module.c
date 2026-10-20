/*
 * Sample AuroraOS Loadable Kernel Module (LKM)
 */

extern int printk(const char *fmt, ...);

int init_module(void)
{
    printk("<6>[TEST_MODULE] Hello from AuroraOS Loadable Kernel Module!\n");
    return 0;
}

void cleanup_module(void)
{
    printk("<6>[TEST_MODULE] Goodbye from AuroraOS Loadable Kernel Module!\n");
}
