#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/uaccess.h>

#define DRIVER_NAME "ads1115_driver"
#define CLASS_NAME "ads1115"
#define DEVICE_NAME "ads1115"

#define ADS1115_REG_CONVERSION 0x00
#define ADS1115_REG_CONFIG 0x01
#define ADS1115_REG_LO_THRESH 0x02
#define ADS1115_REG_HI_THRESH 0x03

// IOCTL commands
#define ADS1115_IOCTL_MAGIC 'a'
#define ADS1115_IOCTL_READ_CONVERSION _IOR(ADS1115_IOCTL_MAGIC, 1, int16_t)
#define ADS1115_IOCTL_CONFIG _IOW(ADS1115_IOCTL_MAGIC, 2, int16_t)
#define ADS1115_IOCTL_LO_THRESH _IOW(ADS1115_IOCTL_MAGIC, 3, int16_t)
#define ADS1115_IOCTL_HI_THRESH _IOW(ADS1115_IOCTL_MAGIC, 4, int16_t)

static struct i2c_client *ads1115_client;
static struct class* ads1115_class = NULL;
static struct device* ads1115_device = NULL;
static int major_number;

static int ads_read(struct i2c_client *client)
{
    u8 buf[2];
    int16_t conversion_data;

    if (i2c_smbus_read_i2c_block_data(client, ADS1115_REG_CONVERSION, sizeof(buf), buf) < 0) {
        printk(KERN_ERR "Failed to read ADC conversion data\n");
        return -EIO;
    }

    // Combine high and low bytes to form 16-bit values
    conversion_data = (buf[0]<<8)|buf[1];

    return conversion_data;
}

static int ads_write(struct i2c_client *client, uint8_t reg_address, int16_t data)
{
    if (i2c_smbus_write_word_data(client, reg_address, data) < 0) {
        printk(KERN_ERR "Failed to config ADC data\n");
        return -EIO;
    }
}

static long ads1115_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
    int16_t data;
    int16_t config_data;

    switch (cmd) {
        case ADS1115_IOCTL_READ_CONVERSION:
            data = ads_read(ads1115_client, 0);
            if (copy_to_user((int16_t __user *)arg, &data, sizeof(data))) {
                return -EFAULT;
            }
            break;
        case ADS1115_IOCTL_CONFIG:
            if (copy_from_user(&config_data, (int16_t __user *)arg, sizeof(config_data))) {
                pr_err("Failed to copy data from user\n");
                return -EFAULT;
            }

            ads_write(ads1115_client, ADS1115_REG_CONFIG, config_data);
            
            break;
        case ADS1115_IOCTL_LO_THRESH:
            if (copy_from_user(&config_data, (int16_t __user *)arg, sizeof(config_data))) {
                pr_err("Failed to copy data from user\n");
                return -EFAULT;
            }
            
            ads_write(ads1115_client, ADS1115_REG_LO_THRESH, config_data);
            
            break;
        case ADS1115_IOCTL_HI_THRESH:
            if (copy_from_user(&config_data, (int16_t __user *)arg, sizeof(config_data))) {
                pr_err("Failed to copy data from user\n");
                return -EFAULT;
            }
            
            ads_write(ads1115_client, ADS1115_REG_HI_THRESH, config_data);
            
            break;
        default:
            return -EINVAL;
    }

    return 0;
}

static int ads1115_open(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "MPU6050 device opened\n");
    return 0;
}

static int ads1115_release(struct inode *inodep, struct file *filep)
{
    printk(KERN_INFO "MPU6050 device closed\n");
    return 0;
}

static struct file_operations fops = {
    .open = ads1115_open,
    .unlocked_ioctl = ads1115_ioctl,
    .release = ads1115_release,
};

static int ads1115_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
    ads1115_client = client;

    // Create a char device
    major_number = register_chrdev(0, DEVICE_NAME, &fops);
    if (major_number < 0) {
        printk(KERN_ERR "Failed to register a major number\n");
        return major_number;
    }

    ads1115_class = class_create(THIS_MODULE, CLASS_NAME);
    if (IS_ERR(ads1115_class)) {
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to register device class\n");
        return PTR_ERR(ads1115_class);
    }

    ads1115_device = device_create(ads1115_class, NULL, MKDEV(major_number, 0), NULL, DEVICE_NAME);
    if (IS_ERR(ads1115_device)) {
        class_destroy(ads1115_class);
        unregister_chrdev(major_number, DEVICE_NAME);
        printk(KERN_ERR "Failed to create the device\n");
        return PTR_ERR(ads1115_device);
    }

    printk(KERN_INFO "ADS1115 driver installed\n");
    return 0;
}

static void ads1115_remove(struct i2c_client *client)
{
    device_destroy(ads1115_class, MKDEV(major_number, 0));
    class_unregister(ads1115_class);
    class_destroy(ads1115_class);
    unregister_chrdev(major_number, DEVICE_NAME);

    printk(KERN_INFO "ADS1115 driver removed\n");
}

static const struct of_device_id ads1115_of_match[] = {
    { .compatible = "invensense,ads1115", },
    { },
};
MODULE_DEVICE_TABLE(of, ads1115_of_match);

static struct i2c_driver ads1115_driver = {
    .driver = {
        .name   = DRIVER_NAME,
        .owner  = THIS_MODULE,
        .of_match_table = of_match_ptr(ads1115_of_match),
    },
    .probe      = ads1115_probe,
    .remove     = ads1115_remove,
};

static int __init ads1115_init(void)
{
    printk(KERN_INFO "Initializing ADS1115 driver\n");
    return i2c_add_driver(&ads1115_driver);
}

static void __exit ads1115_exit(void)
{
    printk(KERN_INFO "Exiting ADS1115 driver\n");
    i2c_del_driver(&ads1115_driver);
}

module_init(ads1115_init);
module_exit(ads1115_exit);

MODULE_AUTHOR("Your Name");
MODULE_DESCRIPTION("ADS1115 I2C Client Driver with IOCTL Interface");
MODULE_LICENSE("GPL");