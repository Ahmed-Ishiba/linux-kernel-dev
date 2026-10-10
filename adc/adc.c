// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/i2c.h>

static int adc_probe(struct i2c_client *client){
    s32 cfg, cfg2;
    cfg = i2c_smbus_read_word_swapped(client, 0x01);
    dev_info(&client->dev, "config register = 0x%04x\n", cfg);
    i2c_smbus_write_word_swapped(client, 0x01, 0x4383);
    cfg2 = i2c_smbus_read_word_swapped(client, 0x01);
    dev_info(&client->dev, "config register = 0x%04x\n", cfg2);
    return 0;
}

static const struct of_device_id adc_of_match[] = {
    { .compatible = "hiwonder,i2c-adc" },   
    { }                                     
};
MODULE_DEVICE_TABLE(of, adc_of_match);
static void adc_remove(struct i2c_client *client){
    pr_info("goodbye i2c");
}

static struct i2c_driver adc_driver = {
    .driver = {
        .name = "adc",
        .of_match_table = adc_of_match,
    },
    .probe  = adc_probe,
    .remove = adc_remove,
};

module_i2c_driver(adc_driver);
MODULE_DESCRIPTION("Greetings");
MODULE_LICENSE("GPL");