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
    // 書き込み後、線を休ませるためにプルアップ状態の入力に戻す
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
    
    // ▼ センサーがデータを返す前に、マイコン側を「入力＋内部プルアップ」に切り替える ▼
    gpio_pin_configure_dt(&cfg->sdio, GPIO_INPUT | GPIO_PULL_UP);
    k_busy_wait(WAIT_TIME * 2);
    
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
    k_msleep(500); // 電源安定待ち

    // ▼ 1. PIDを聞く前に、まずリセットコマンドでセンサーを叩き起こす ▼
    mx8650_write(dev, 0x06, 0x80); 
    k_msleep(50); // 起きるまで少し長めに待つ
    mx8650_write(dev, 0x06, 0x00); // 800 CPI設定
    
    int loop_counter = 0;

    while (1) {
        if (loop_counter % 100 == 0) {
            uint8_t pid1 = mx8650_read(dev, 0x00);
            uint8_t pid2 = mx8650_read(dev, 0x01);
            // ログが邪魔なら printk の行をコメントアウト(//)してもOKです
            printk("=== MX8650 ALIVE === PID1: 0x%02x, PID2: 0x%02x\n", pid1, pid2);
        }
        loop_counter++;

        uint8_t status = mx8650_read(dev, 0x02);
        
        if (status != 0xFF && (status & 0x80)) {
            // 1. センサーから生のデータを読み取る
            int8_t raw_dx = (int8_t)mx8650_read(dev, 0x03);
            int8_t raw_dy = (int8_t)mx8650_read(dev, 0x04);

            // 2. 物理的な取り付け角度（90度回転）を補正する
            // 報告の動き: 左->下(+Y), 下->右(+X), 右->上(-Y), 上->左(-X)
            // 補正式: X軸には「生の-Y」、Y軸には「生の+X」を割り当てる
            int16_t mapped_dx = -raw_dy;
            int16_t mapped_dy = raw_dx;

            // 3. 補正済みのデータをPCに送る
            input_report_rel(dev, INPUT_REL_X, mapped_dx, false, K_FOREVER);
            input_report_rel(dev, INPUT_REL_Y, mapped_dy, true, K_FOREVER);
        }
        k_msleep(10);
    }
K_THREAD_STACK_DEFINE(mx8650_stack, 1024);
struct k_thread mx8650_thread_data;

static int mx8650_init(const struct device *dev) {
    const struct mx8650_config *cfg = dev->config;
    
    gpio_pin_configure_dt(&cfg->sclk, GPIO_OUTPUT_ACTIVE); 
    gpio_pin_set_dt(&cfg->sclk, 1); // クロックの待機状態は確実にHighにする
    gpio_pin_configure_dt(&cfg->sdio, GPIO_INPUT | GPIO_PULL_UP);
    
    k_thread_create(&mx8650_thread_data, mx8650_stack, 1024,
                    mx8650_thread, (void *)dev, NULL, NULL,
                    K_PRIO_COOP(10), 0, K_NO_WAIT);
    return 0;
}

static const struct mx8650_config mx8650_config_0 = {
    .sclk = GPIO_DT_SPEC_GET(TRACKBALL_NODE, sclk_gpios),
    .sdio = GPIO_DT_SPEC_GET(TRACKBALL_NODE, sdio_gpios),
};

DEVICE_DT_DEFINE(TRACKBALL_NODE, mx8650_init, NULL, NULL,
                 &mx8650_config_0, POST_KERNEL, 90, NULL);

#endif /* DT_NODE_EXISTS(TRACKBALL_NODE) */
