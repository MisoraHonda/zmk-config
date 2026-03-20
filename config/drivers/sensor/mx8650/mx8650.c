#define DT_DRV_COMPAT pixart_mx8650

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/kernel.h>
#include <zephyr/input/input.h>

struct mx8650_config {
    struct gpio_dt_spec sclk;
    struct gpio_dt_spec sdio;
};

/* 信号の待ち時間を 10マイクロ秒に延長 (より確実に) */
#define WAIT_TIME 10

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
    gpio_pin_configure_dt(&cfg->sdio, GPIO_INPUT);
    k_busy_wait(WAIT_TIME * 2); // ターンアラウンド時間を長めに
    for (int i = 7; i >= 0; i--) {
        gpio_pin_set_dt(&cfg->sclk, 0);
        k_busy_wait(WAIT_TIME);
        gpio_pin_set_dt(&cfg->sclk, 1);
        if (gpio_pin_get_dt(&cfg->sdio)) res |= (1 << i);
        k_busy_wait(WAIT_TIME);
    }
    return res;
}

static void mx8650_thread(void *p1, void *p2, void *p3) {
    const struct device *dev = p1;
    
    // 【重要】データシート P.15 に基づく初期化シーケンス
    k_msleep(50);               // 電圧安定待ち
    mx8650_write(dev, 0x06, 0x80); // Soft Reset
    k_msleep(10);               // リセット完了待ち
    mx8650_write(dev, 0x06, 0x00); // 通常モード & 800 CPI

    while (1) {
        uint8_t status = mx8650_read(dev, 0x02);
        if (status & 0x80) { // Motion bit
            // 2の補数として読み取る
            int8_t dx = (int8_t)mx8650_read(dev, 0x03);
            int8_t dy = (int8_t)mx8650_read(dev, 0x04);
            
            // ZMK 4.1.0 形式の入力報告
            input_report_rel(dev, INPUT_REL_X, dx, false, K_FOREVER);
            input_report_rel(dev, INPUT_REL_Y, dy, true, K_FOREVER);
        }
        k_msleep(10); // ポーリング間隔
    }
}
// (以下、Thread定義やDEVICE_DT_INST_DEFINEは前回と同じ)
