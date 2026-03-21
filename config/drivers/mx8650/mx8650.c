#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>

#define TRACKBALL_NODE DT_NODELABEL(trackball)

struct mx8650_config {
    struct gpio_dt_spec sclk;
    struct gpio_dt_spec sdio;
};

// 起動確認用の初期化関数
static int mx8650_init(const struct device *dev) {
    const struct mx8650_config *cfg = dev->config;
    
    // ログに確実に出す
    printk("\n\n*******************************************\n");
    printk("*** ZMK: MX8650 DRIVER INITIALIZED OK!  ***\n");
    printk("*******************************************\n\n");

    // ピンの初期設定
    gpio_pin_configure_dt(&cfg->sclk, GPIO_OUTPUT_ACTIVE); 
    gpio_pin_configure_dt(&cfg->sdio, GPIO_INPUT);
    
    return 0;
}

static const struct mx8650_config mx8650_config_0 = {
    .sclk = GPIO_DT_SPEC_GET(TRACKBALL_NODE, sclk_gpios),
    .sdio = GPIO_DT_SPEC_GET(TRACKBALL_NODE, sdio_gpios),
};

// 第1引数に TRACKBALL_NODE を指定することで、.overlay と紐付けます
DEVICE_DT_DEFINE(TRACKBALL_NODE, mx8650_init, NULL, NULL,
                 &mx8650_config_0, POST_KERNEL, 90, NULL);
