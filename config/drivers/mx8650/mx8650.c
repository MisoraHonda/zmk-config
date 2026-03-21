#include <zephyr/kernel.h>
#include <zephyr/device.h>

static int mx8650_init(const struct device *dev) {
    printk("\n\n!!! MX8650 DRIVER LOADED SUCCESSFULLY !!!\n\n");
    return 0;
}

SYS_INIT(mx8650_init, POST_KERNEL, 90);
