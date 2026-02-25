#include <linux/module.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/pwm.h>
#include <linux/uaccess.h>
#include <linux/kernel.h>
#include <linux/device.h>

#define GPIO_IN1 17
#define GPIO_IN2 27
#define PWM_PERIOD 1000000

static struct pwm_device *pwm_motor = NULL;
static int major_number;
static struct class* motor_class  = NULL;
static struct device* motor_device = NULL;

static int motor_open(struct inode *inode, struct file *file) {
    return 0;
}

static ssize_t motor_write(struct file *file, const char __user *buf, size_t len, loff_t *off) {
    char kbuf[16];
    int speed;
    if (len > 15) len = 15;
    if (copy_from_user(kbuf, buf, len)) return -EFAULT;
    kbuf[len] = '\0';

    if (kstrtoint(kbuf, 10, &speed) >= 0) {
        if (speed < 0) speed = 0;
        if (speed > 100) speed = 100;
        
        if (speed == 0) {
            gpio_set_value(GPIO_IN1, 0);
            gpio_set_value(GPIO_IN2, 0);
        } else {
            gpio_set_value(GPIO_IN1, 1);
            gpio_set_value(GPIO_IN2, 0);
        }
        pwm_config(pwm_motor, (speed * PWM_PERIOD) / 100, PWM_PERIOD);
        pwm_enable(pwm_motor);
        printk(KERN_INFO "Motor: speed set to %d\n", speed);
    }
    return len;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = motor_open,
    .write = motor_write,
};

static int __init motor_init(void) {
    printk(KERN_INFO "Motor: Initializing...\n");

    // 1. 주번호 동적 할당
    major_number = register_chrdev(0, "motor", &fops);
    if (major_number < 0) return major_number;

    // 2. 클래스 등록 (자동으로 /dev/motor 생성을 돕는다)
    motor_class = class_create(THIS_MODULE, "motor_class");
    motor_device = device_create(motor_class, NULL, MKDEV(major_number, 0), NULL, "motor");

    // 3. GPIO 및 PWM 설정
    gpio_request(GPIO_IN1, "IN1");
    gpio_direction_output(GPIO_IN1, 0);
    gpio_request(GPIO_IN2, "IN2");
    gpio_direction_output(GPIO_IN2, 0);

    pwm_motor = pwm_request(0, "motor_pwm");
    if (IS_ERR(pwm_motor)) {
        printk(KERN_ERR "Motor: PWM request failed!\n");
        return PTR_ERR(pwm_motor);
    }
    
    printk(KERN_INFO "Motor: Registered with Major %d\n", major_number);
    return 0;
}

static void __exit motor_exit(void) {
    pwm_disable(pwm_motor);
    pwm_free(pwm_motor);
    gpio_free(GPIO_IN1);
    gpio_free(GPIO_IN2);
    device_destroy(motor_class, MKDEV(major_number, 0));
    class_unregister(motor_class);
    class_destroy(motor_class);
    unregister_chrdev(major_number, "motor");
}

module_init(motor_init);
module_exit(motor_exit);
MODULE_LICENSE("GPL");
