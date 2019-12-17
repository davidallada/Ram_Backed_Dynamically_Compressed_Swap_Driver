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

#include "network_compression_client.h"

MODULE_LICENSE("banana");

struct task_struct * task_id;

static int make_network_requests(void * data)
{
    // Allocate some memory for compression
    int buflen = 4096;
    char buffer[buflen];
    for (int i = 0; i < buflen; i++) buffer[i] = i;
    char compressed[buflen];
    int compressed_len;
    char decompressed[buflen];
    int decompressed_len;

    while (!kthread_should_stop()) {
        // Compress over the network
        compress_over_network(
            buffer,
            buflen,
            compressed,
            &compressed_len
        );

        // Decompress over the network
        decompress_over_network(
            compressed,
            compressed_len,
            decompressed,
            &decompressed_len
        );

        // Test that compression was accurate
        if (buflen != decompressed_len) {
            pr_info(" Error: incorrect decompression size \n");
            return 0;
        }

        for (int idx = 0; idx < buflen; idx++) {
            if (decompressed[idx] != buffer[idx]) {
                pr_info(" Error: lossy compression \n");
                return 0;
            }
        }

        msleep_interruptible(1000);

        pr_info("compression successful, result size = %d", compressed_len);
    }
    return 0;
}

static int __init init_kernel_module(void)
{
    // Initialize the tcp network client
    tcp_client_connect();

    // Start the kernel thread
    task_id = kthread_run(make_network_requests, NULL, "make_network_requests");

    return 0;
}

static void __exit exit_kernel_module(void)
{
    // stop the kernel thread
    kthread_stop(task_id);

    // exit the TCP connection
    network_client_exit();
}

module_init(init_kernel_module);
module_exit(exit_kernel_module);
