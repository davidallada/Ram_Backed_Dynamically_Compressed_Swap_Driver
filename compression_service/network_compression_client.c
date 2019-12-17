#ifndef _network_compression_client_c_
#define _network_compression_client_c_

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/net.h>
#include <net/sock.h>
#include <linux/tcp.h>
#include <linux/in.h>
#include <asm/uaccess.h>
#include <linux/socket.h>
#include <linux/slab.h>

MODULE_LICENSE("banana");

#define INTSZ 4  // sizeof int

#include "network_compression_client.h"
#define PORT 8080
struct socket *conn_socket;


static
int
tcp_client_receive(
    struct socket * sock,
    char *          buf,
    int             num_bytes,
    unsigned long   flags
) {
    struct msghdr msg;
    struct kvec vec;
    int max_size = 5000;
    int ret;
    msg.msg_name    = 0;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags   = flags;
    vec.iov_len = max_size;
    vec.iov_base = buf;
    ret = kernel_recvmsg(sock, &msg, &vec, max_size, num_bytes, flags);
    if (ret < 0)
    {
        pr_info(" Error: kernel_recvmsg %d \n", ret);
        return -1;
    }
    return 0;
}


static
u32
create_address(u8 *ip)
{
    u32 addr = 0;
    for (int i = 0; i < 4; i++) {
        addr += ip[i];
        if (i == 3) break;
        addr <<= 8;
    }
    return addr;
}


static
int
tcp_client_send(
    struct socket *   sock,
    const char *      buf,
    const size_t      length,
    unsigned long     flags
) {
    // TODO: understand/clean this
    struct msghdr msg;
    struct kvec vec;
    int len, written, left;
    written = 0;
    left = length;
    mm_segment_t oldmm;

    msg.msg_name    = 0;
    msg.msg_namelen = 0;
    msg.msg_control = NULL;
    msg.msg_controllen = 0;
    msg.msg_flags   = flags;

    oldmm = get_fs();
    set_fs(KERNEL_DS);
repeat_send:
    vec.iov_len = left;
    vec.iov_base = (char *)buf + written;

    len = kernel_sendmsg(sock, &msg, &vec, left, left);
    if ((len == -ERESTARTSYS) || (!(flags & MSG_DONTWAIT) && (len == -EAGAIN)))
        goto repeat_send;
    if (len > 0) {
        written += len;
        left -= len;
        if(left)
            goto repeat_send;
    }
    set_fs(oldmm);
    return written ? written:len;
}


/*
 * Initializes the TCP connection to the compression service.
 */
int
tcp_client_connect(void)
{
    struct sockaddr_in saddr;
    unsigned char destip[5] = {0, 0, 0, 0, '\0'};
    int ret;
    ret = sock_create(PF_INET, SOCK_STREAM, IPPROTO_TCP, &conn_socket);
    if (ret < 0) {
        pr_info(" Error: sock_create %d \n", ret);
        return -1;
    }

    memset(&saddr, 0, sizeof(saddr)); // TODO can we remove this?
    saddr.sin_family = AF_INET;
    saddr.sin_port = htons(PORT);
    saddr.sin_addr.s_addr = htonl(create_address(destip));

    ret = conn_socket->ops->connect(
            conn_socket,
            (struct sockaddr *)&saddr,
            sizeof(saddr),
            O_RDWR
    );
    if (ret && (ret != -EINPROGRESS)) {
        pr_info(" Error: conn_socket->ops->connect %d \n", ret);
    }

    return 0;
}


void
network_client_exit(void)
{
    if (conn_socket != NULL) {
        sock_release(conn_socket);
    }
}


/*
 * Takes a src array and a srclen to compress.
 * Sets the dst array with the compressed contents of src,
 *   and sets dstlen with the compressed length.
 * Returns 0 on success, -1 on failure.
 */
int
compress_over_network(
    char *   src,
    int      srclen,
    char *   dst,
    int *    dstlen
) {

    // Create the message to send (based on service spec)
    char header[1 + INTSZ];
    header[0] = 'c';
    *( (int *)(header + 1) ) = srclen;

    // Send the message header
    tcp_client_send(conn_socket, header, 1 + INTSZ, MSG_DONTWAIT);

    // Send the message buffer
    tcp_client_send(conn_socket, src, srclen, MSG_DONTWAIT);

    // Get the total number of bytes in the compressed buffer
    char response_header[4];
    tcp_client_receive(conn_socket, response_header, INTSZ, MSG_WAITALL);
    *dstlen = *( (int *)response_header );

    // Get the compressed chunk
    tcp_client_receive(conn_socket, dst, *dstlen, MSG_WAITALL);

    return 0;
}


/*
 * Takes a src array and a srclen to decompress.
 * Sets the dst array with the decompressed contents of src,
 *   and sets dstlen with the decompressed length.
 * Returns 0 on success, -1 on failure.
 */
int
decompress_over_network(
    char *   src,
    int      srclen,
    char *   dst,
    int *    dstlen
) {
    // Create the message to send (based on service spec)
    char header[1 + INTSZ];
    header[0] = 'd';
    *( (int *)(header + 1) ) = srclen;

    // Send the message header
    tcp_client_send(conn_socket, header, 1 + INTSZ, MSG_DONTWAIT);

    // Send the message buffer
    tcp_client_send(conn_socket, src, srclen, MSG_DONTWAIT);

    // Get the total number of bytes in the decompressed buffer
    char response_header[4];
    tcp_client_receive(conn_socket, response_header, INTSZ, MSG_WAITALL);
    *dstlen = *( (int *)response_header );

    // Get the decompressed chunk
    tcp_client_receive(conn_socket, dst, *dstlen, MSG_WAITALL);

    return 0;
}

#endif
