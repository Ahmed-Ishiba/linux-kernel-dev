// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/uaccess.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/mod_devicetable.h>
#include <linux/of.h>              
#include <linux/io.h>             
#include <linux/err.h>           
#include <linux/pm_runtime.h>    
#include <linux/serial_reg.h>  
#include <linux/fs.h>
#include <linux/interrupt.h>
#include <linux/miscdevice.h>
#include <linux/wait.h>
#define UART_REG_SHIFT   2
#ifndef UART_OMAP_MDR1
#define UART_OMAP_MDR1   0x08      /* byte offset 0x20 / 4 */
#endif
#define SERIAL_BUFSIZE 16
struct serial_dev{
        void __iomem *regs;
        struct miscdevice miscdev;
        char rx_buff[SERIAL_BUFSIZE];
        unsigned int buf_rd;
        unsigned int buf_wr;
        wait_queue_head_t wait;

};

static void reg_write(struct serial_dev *serial, u32 val, unsigned int reg)
{
    writel(val, serial->regs + (reg << UART_REG_SHIFT));
}
static u32 reg_read(struct serial_dev *serial, unsigned int reg)
{
    return readl(serial->regs + (reg << UART_REG_SHIFT));
}
static irqreturn_t serial_irq_handler(int irq, void * dev_id){
        struct serial_dev *serial = dev_id;
        serial->rx_buff[serial->buf_wr] = reg_read(serial, UART_RX);
        serial->buf_wr++;
        if(serial->buf_wr >= SERIAL_BUFSIZE) serial->buf_wr =0;
        wake_up(&serial->wait);

        pr_info("UART recieved %c \n",serial->rx_buff[serial->buf_wr]);

        return IRQ_HANDLED;
}
static const struct of_device_id serial_of_match[] = {
    { .compatible = "bootlin,serial" },   
    { },                        
};
void test_uart(struct serial_dev* serial, char c){
        while(!(reg_read(serial, UART_LSR) & UART_LSR_THRE)) cpu_relax();
        reg_write(serial, c,UART_TX);

}


static ssize_t serial_write(struct file *file, const char __user *buf, size_t count, loff_t *ppos){
        struct miscdevice *miscdev_ptr = file->private_data;
        struct serial_dev *serial = container_of(miscdev_ptr, struct serial_dev, miscdev);

        char c;
        size_t i;

        for (i = 0; i < count; i++) {
        if (get_user(c, buf + i))
                return i ? i : -EFAULT;
        
        pr_info("serial_write: sending byte 0x%02x\n",(unsigned char) c);
        test_uart(serial, c);
        }

        return count;

}

static ssize_t serial_read(struct file *file,  char __user *buf, size_t count, loff_t *ppos){
        struct miscdevice *miscdevice_ptr = file->private_data;
        struct serial_dev *serial = container_of(miscdevice_ptr, struct serial_dev, miscdev);
        char c;
        wait_event_interruptible(serial->wait,(serial->buf_rd != serial->buf_wr));
        c = serial->rx_buff[serial->buf_rd];
        serial->buf_rd++;
        if(serial->buf_rd >=SERIAL_BUFSIZE) serial->buf_rd = 0;
        
        if(put_user(c, buf)) return -EFAULT;
        return 1;
}


static const struct file_operations serial_fops={
        .owner = THIS_MODULE,
        .write = serial_write,
        .read = serial_read,
};

MODULE_DEVICE_TABLE(of, serial_of_match);
static int serial_probe(struct platform_device *pdev)
{
        unsigned int baud_divisor, uartclk;
        int ret,ret2;
        int irq;
        struct serial_dev *serial;
        serial = devm_kzalloc(&pdev->dev, sizeof(*serial), GFP_KERNEL);

        if(!serial) return -ENOMEM;
        init_waitqueue_head(&serial->wait);
        serial->regs = devm_platform_ioremap_resource(pdev, 0);

        if(IS_ERR(serial->regs)) return PTR_ERR(serial->regs);
        platform_set_drvdata(pdev, serial);
        pm_runtime_enable(&pdev->dev);
        pm_runtime_get_sync(&pdev->dev);

        ret = of_property_read_u32(pdev->dev.of_node, "clock-frequency",&uartclk);
        if(ret){
                dev_err(&pdev->dev, "clock-frequency property not found in device tree\n");
                return ret;
        }
        baud_divisor = uartclk /16/115200;
        reg_write(serial, 0x07,UART_OMAP_MDR1);
        reg_write(serial, 0x00, UART_LCR);
        reg_write(serial, UART_LCR_DLAB, UART_LCR);
        reg_write(serial, baud_divisor & 0xff, UART_DLL);
        reg_write(serial, (baud_divisor >> 8) & 0xff, UART_DLM);
        reg_write(serial, UART_LCR_WLEN8, UART_LCR);
        reg_write(serial, 0x00, UART_OMAP_MDR1);
        reg_write(serial, UART_FCR_CLEAR_RCVR | UART_FCR_CLEAR_XMIT, UART_FCR);
        reg_write(serial, UART_IER_RDI, UART_IER);

        test_uart(serial, 'f');
        serial ->miscdev.minor = MISC_DYNAMIC_MINOR;
        struct resource *res;
        res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
        char *name = devm_kasprintf(&pdev->dev, GFP_KERNEL, "serial-%x",res->start);
        serial->miscdev.name = name;
        serial->miscdev.fops = &serial_fops;
        serial->miscdev.parent = &pdev->dev;
        ret2 = misc_register(&serial->miscdev);
        if(ret2) return ret2;

        irq = platform_get_irq(pdev, 0);
        if(irq < 0) return irq;
	return devm_request_irq(&pdev->dev,  irq, serial_irq_handler, 0, "serial", serial);
}

static int serial_remove(struct platform_device *pdev)
{
        struct serial_dev *serial = platform_get_drvdata(pdev);

        misc_deregister(&serial->miscdev);
        pm_runtime_put_sync(&pdev->dev);
        pm_runtime_disable(&pdev->dev);
        pr_info("Called %s\n", __func__);

        return 0;
}


static struct platform_driver serial_driver = {
        .driver = {
                .name = "serial",
                .owner = THIS_MODULE,
                .of_match_table = serial_of_match,
        },
        .probe = serial_probe,
        .remove = serial_remove,
};
module_platform_driver(serial_driver);

MODULE_LICENSE("GPL");
