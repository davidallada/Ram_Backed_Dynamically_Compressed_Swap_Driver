#ifndef _network_compression_client_h_
#define _network_compression_client_h_

#define PORT 8080

/*
 * Initializes the TCP connection to the compression service.
 * Call this on module init.
 */
int
tcp_client_connect(void);


/*
 * Ends the TCP connection to the compression service.
 * Call this on module exit.
 */
void
network_client_exit(void);


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
);


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
);

#endif
