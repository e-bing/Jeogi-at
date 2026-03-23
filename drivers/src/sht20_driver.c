#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/device.h>

#define DEVICE_NAME "sht20"
#define SHT20_ADDR 0x40
#define CMD_TEMP_NO_HOLD 0xF3
#define CMD_HUMI_NO_HOLD 0xF5

static int major_number;
static struct class* sht20_class = NULL;
static struct device* sht20_device = NULL;
static struct i2c_client *sht20_client_ptr = NULL;

// CRC-8 검증 함수 (Poly: 0x31, Init: 0x00)
static u8 sht20_crc8(const u8 *data, int len) {
    u8 crc = 0x00;
    int i, j;
    for (i = 0; i < len; i++) {
        crc ^= data[i];
        for (j = 0; j < 8; j++) {
            if (crc & 0x80)
                crc = (crc << 1) ^ 0x31;
            else
                crc <<= 1;
        }
    }
    return crc;
}

// 센서에서 데이터를 읽어오는 함수
static int read_sht20_data(struct i2c_client *client, char cmd, unsigned short *result) {
    u8 buf[3];
    int ret;

    // 1. 측정 명령 전송
    ret = i2c_master_send(client, &cmd, 1);
    if (ret < 0) return ret;

    // 2. 측정 시간 대기 (SHT20 가이드라인 기준)
    msleep(100);

    // 3. 데이터 3바이트 수신 (MSB, LSB, Checksum)
    ret = i2c_master_recv(client, buf, 3);
    if (ret < 0) return ret;

    // 4. CRC-8 검증
    if (sht20_crc8(buf, 2) != buf[2])
        return -EIO;

    *result = (buf[0] << 8) | (buf[1] & 0xFC);
    return 0;
}

static ssize_t sht20_read(struct file *file, char __user *user_buf, size_t count, loff_t *ppos) {
    struct i2c_client *client = (struct i2c_client *)file->private_data;
    unsigned short raw_temp, raw_humi;
    char out_buf[64];
    int temp_x100, humi_x100;
    int len;

    // 1. 센서로부터 원본 데이터 읽기
    if (read_sht20_data(client, CMD_TEMP_NO_HOLD, &raw_temp) < 0) return -EIO;
    if (read_sht20_data(client, CMD_HUMI_NO_HOLD, &raw_humi) < 0) return -EIO;

    // 2. 정수 연산으로 변환 (100배 곱해서 계산)
    // 공식: T = -46.85 + 175.72 * (raw / 65536)
    // 정수식: T*100 = -4685 + (17572 * raw) / 65536
    temp_x100 = -4685 + ((long long)17572 * raw_temp) / 65536;
    
    // 공식: RH = -6.0 + 125.0 * (raw / 65536)
    // 정수식: RH*100 = -600 + (12500 * raw) / 65536
    humi_x100 = -600 + ((long long)12500 * raw_humi) / 65536;

    // 3. 소수점 자릿수를 나누어서 문자열 생성
    len = snprintf(out_buf, sizeof(out_buf), "Temp: %d.%02d C, Humi: %d.%02d %%\n", 
                   temp_x100 / 100, abs(temp_x100 % 100),
                   humi_x100 / 100, abs(humi_x100 % 100));

    if (copy_to_user(user_buf, out_buf, len)) return -EFAULT;

    return (ppos && *ppos > 0) ? 0 : len;
}

static int sht20_open(struct inode *inode, struct file *file) {
    if (!sht20_client_ptr) return -ENODEV;
    file->private_data = sht20_client_ptr;
    return 0;
}

static struct file_operations fops = {
    .owner = THIS_MODULE,
    .open = sht20_open,
    .read = sht20_read,
};

static int sht20_probe(struct i2c_client *client, const struct i2c_device_id *id) {
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    sht20_class = class_create(THIS_MODULE, DEVICE_NAME);
    sht20_device = device_create(sht20_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    
    // probe 시점에 들어온 client 정보를 전역 변수에 저장한다.
    sht20_client_ptr = client;
    i2c_set_clientdata(client, client); 

    printk(KERN_INFO "SHT20: 드라이버 로드 및 장치 등록 완료\n");
    return 0;
}

static void sht20_remove(struct i2c_client *client) {
    sht20_client_ptr = NULL; // 해제된 메모리를 다시 참조하지 않도록 포인터를 비운다.   
    device_destroy(sht20_class, MKDEV(major_number, 0));
    class_unregister(sht20_class);
    class_destroy(sht20_class);
    unregister_chrdev(major_number, DEVICE_NAME);
}

static const struct i2c_device_id sht20_id[] = {
    { "sht20", 0 },
    { }
};
MODULE_DEVICE_TABLE(i2c, sht20_id);

static struct i2c_driver sht20_driver = {
    .driver = {
        .name = "sht20",
        .owner = THIS_MODULE,
    },
    .probe = sht20_probe,
    .remove = sht20_remove,
    .id_table = sht20_id,
};

module_i2c_driver(sht20_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("User");
MODULE_DESCRIPTION("SHT20 I2C Sensor Driver");
