// SPDX-License-Identifier: GPL-2.0
#include <linux/init.h>
#include <linux/module.h>
#include <linux/utsname.h>
#include <linux/moduleparam.h>
char *mystr = "world";
int now;
static int __init hello_init(void)
{
pr_alert("hello %s and welcome %s!\n", utsname()->release, mystr);
now = ktime_get_seconds();
return 0;
}
static void __exit hello_exit(void)
{
pr_alert("%i goodbye kernel\n", now-(int)ktime_get_seconds());
}
module_init(hello_init);
module_exit(hello_exit);
module_param(mystr, charp, 0444);
MODULE_DESCRIPTION("Greetings");
MODULE_LICENSE("GPL");
