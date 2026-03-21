#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>

#define TRACKBALL_NODE DT_NODELABEL(trackball)

#if DT_NODE_EXISTS(TRACKBALL_NODE)

struct mx8650_config {
    struct gpio_dt_spec sclk;
    struct gpio_dt_spec sdio;
};

#define WAIT_TIME 15

static void mx8650_write(const struct device *dev, uint8_t addr, uint8_t data) {
    const struct mx8650_config *cfg = dev->config;
    addr |= 0x80; 
    gpio_pin_configure_dt(&cfg->sdio, GPIO_OUTPUT_ACTIVE);
    for (int i = 7; i >= 0; i--) {
        gpio_pin_set_dt(&cfg->sclk, 0);
        gpio_pin_set_dt(&cfg->sdio, (addr >> i) & 1);
        k_busy_wait(WAIT_TIME);
        gpio_pin_set_dt(&cfg->sclk, 1);
        k_busy_wait(WAIT_TIME);
    }
    for (int i = 7; i >= 0; i--) {
        gpio_pin_set_dt(&cfg->sclk, 0);
        gpio_pin_set_dt(&cfg->sdio, (data >> i) & 1);
        k_busy_wait(WAIT_TIME);
        gpio_pin_set_dt(&cfg->sclk, 1);
        k_busy_wait(WAIT_TIME);
    }
    gpio_pin_configure_dt(&cfg->sdio, GPIO_INPUT | GPIO_PULL_UP);
}

static uint8_t mx8650_read(const struct device *dev, uint8_t addr) {
    const struct mx8650_config *cfg = dev->config;
    uint8_t res = 0;
    addr &= 0x7F;
    gpio_pin_configure_dt(&cfg->sdio, GPIO_OUTPUT_ACTIVE);
    for (int i = 7; i >= 0; i--) {
        gpio_pin_set_dt(&cfg->sclk, 0);
        gpio_pin_set_dt(&cfg->sdio, (addr >> i) & 1);
        k_busy_wait(WAIT_TIME);
        gpio_pin_set_dt(&cfg->sclk, 1);
        k_busy_wait(WAIT_TIME);
    }
    
    gpio_pin_configure_dt(&cfg->sdio, GPIO_INPUT | GPIO_PULL_UP);
    k_busy_wait(WAIT_
