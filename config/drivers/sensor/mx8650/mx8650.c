#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>

#define DT_DRV_COMPAT pixart_mx8650

struct mx8650_config {
    struct gpio_dt_spec sclk;
    struct gpio_dt_spec sdio;
};

/* 1バイト書き込み */
static void mx8650_write(const struct device *dev, uint8_t addr, uint8_t data) {
    const struct mx8650_config *cfg = dev->config;
    addr |= 0x80; 
    gpio_pin_configure_dt(&cfg->sdio, GPIO_OUTPUT_ACTIVE);
    for (int i = 7; i >= 0; i--) {
        gpio_pin_set_dt(&cfg->sclk, 0);
        gpio_pin_set_dt(&cfg->sdio, (addr >> i) & 1);
        k_busy_wait(2);
        gpio_pin_set_dt(&cfg->sclk, 1);
        k_busy_wait(2);
    }
    for (int i = 7; i >= 0; i--) {
        gpio_pin_set_dt(&cfg->sclk, 0);
        gpio_pin_set_dt(&cfg->sdio, (data >> i) & 1);
        k_busy_wait(2);
        gpio_pin_set_dt(&cfg->sclk, 1);
        k_busy_wait(2);
    }
}

/* 1バイト読み出し */
static uint8_t mx8650_read(const struct device *dev, uint8_t addr) {
    const struct mx8650_config *cfg = dev->config;
    uint8_t res = 0;
    addr &= 0x7F;
    gpio_pin_configure_dt(&cfg->sdio, GPIO_OUTPUT_ACTIVE);
    for (int i = 7; i >= 0; i--) {
        gpio_pin_set_dt(&cfg->sclk, 0);
        gpio_pin_set_dt(&cfg->sdio, (addr >> i) & 1);
        k_busy_wait(2);
        gpio_pin_set_dt(&cfg->sclk, 1);
        k_busy_wait(2);
    }
    gpio_pin_configure_dt(&cfg->sdio, GPIO_INPUT);
    k_busy_wait(5);
    for (int i = 7; i >= 0; i--) {
        gpio_pin_set_dt(&cfg->sclk, 0);
        k_busy_wait(2);
        gpio_pin_set_dt(&cfg->sclk, 1);
        if (gpio_pin_get_dt(&cfg->sdio)) res |= (1 << i);
        k_busy_wait(2);
    }
    return res;
}

static void mx8650_thread(void *p1, void *p2, void *p3) {
    const struct device *dev = p1;
    mx8650_write(dev, 0x06, 0x00); // 800 CPI
    while (1) {
        uint8_t status = mx8650_read(dev, 0x02);
        if (status & 0x80) {
            int8_t dx = (int8_t)mx8650_read(dev, 0x03);
            int8_t dy = (int8_t)mx8650_read(dev, 0x04);
            input_report_rel(dev, INPUT_REL_X, dx, false, K_FOREVER);
            input_report_rel(dev, INPUT_REL_Y, dy, true, K_FOREVER);
        }
        k_msleep(10);
    }
}

K_THREAD_STACK_DEFINE(mx8650_stack, 1024);
struct k_thread mx8650_thread_data;

static int mx8650_init(const struct device *dev) {
    const struct mx8650_config *cfg = dev->config;
    if (!gpio_is_ready_dt(&cfg->sclk) || !gpio_is_ready_dt(&cfg->sdio)) return -ENODEV;
    gpio_pin_configure_dt(&cfg->sclk, GPIO_OUTPUT_INACTIVE);
    gpio_pin_configure_dt(&cfg->sdio, GPIO_INPUT);
    k_thread_create(&mx8650_thread_data, mx8650_stack, 1024,
                    mx8650_thread, (void *)dev, NULL, NULL,
                    K_PRIO_COOP(10), 0, K_NO_WAIT);
    return 0;
}

#define MX8650_INST(n) \
    static const struct mx8650_config mx8650_config_##n = { \
        .sclk = GPIO_DT_SPEC_INST_GET(n, sclk_gpios), \
        .sdio = GPIO_DT_SPEC_INST_GET(n, sdio_gpios), \
    }; \
    DEVICE_DT_INST_DEFINE(n, mx8650_init, NULL, NULL, \
                         &mx8650_config_##n, POST_KERNEL, \
                         CONFIG_INPUT_INIT_PRIORITY, NULL);

DT_INST_FOREACH_STATUS_OKAY(MX8650_INST)
