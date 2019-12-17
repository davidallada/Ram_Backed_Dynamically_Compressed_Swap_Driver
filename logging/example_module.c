// How to use this example:
// 1. start service.c (compile / run it yo)
// 2. call `make` in this directory
// 3. Insert the module with `sudo insmod ex_module.ko`
// 4. Check dmesg to see that compression is successful
// 5. Remove the module with `sudo rmmod ex_module`


#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/delay.h>
#include <linux/kthread.h>

#include "network_logging.h"

MODULE_LICENSE("banana");

struct task_struct * task_id;

static int make_network_requests(void * data)
{
    int idx = 0;
    while (!kthread_should_stop()) {

        char msg[30];
        snprintf(msg, 30, "Hello, world! %d", idx);
        log_network(msg);

        msleep_interruptible(1000);

        idx++;
    }
    return 0;
}

static int __init init_kernel_module(void)
{
    // Initialize the tcp network client
    logger_connect();

    // Start the kernel thread
    task_id = kthread_run(make_network_requests, NULL, "make_network_requests");

    return 0;
}

static void __exit exit_kernel_module(void)
{
    // stop the kernel thread
    kthread_stop(task_id);

    // exit the TCP connection
    logger_exit();
}

module_init(init_kernel_module);
module_exit(exit_kernel_module);
