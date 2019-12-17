// COMPILE: gcc service.c -o service.out
// Specification:
//
// INPUT
// first byte: either a 'c' or 'd' character, (compress or decompress)
// next 4 bytes: integer (i) representing chunk size in bytes
// next i bytes: the chunk to be compression/decompression
//
// OUTPUT
// first 4 bytes: integer (j) representing the chunk size in bytes
// next i bytes: the resulting chunk after compression/decompression

// TODO:
// [ ] Make this work for concurrent requests
// [ ] Don't exit when things break -- try to persist

#include <unistd.h>
#include <stdio.h>
#include <sys/socket.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <string.h>

/*
 * Takes a src array to compress and srclen value.
 * Sets dst with compressed version of src.
 * Sets dstlen with length of dst.
 * Returns 0 on success, -1 on failure.
 */
static int compress(char * src, int srclen, char * dst, int * dstlen)
{
    // TODO: Actually compress
    *dstlen = srclen;
    for (int i = 0; i < srclen; i++) {
        dst[i] = src[i];
    }
    return 0;
}

/*
 * Takes a src array to compress and srclen value.
 * Sets dst with compressed version of src.
 * Sets dstlen with length of dst.
 * Returns 0 on success, -1 on failure.
 */
static int decompress(char * src, int srclen, char * dst, int * dstlen)
{
    // TODO: Actually decompress
    *dstlen = srclen;
    for (int i = 0; i < srclen; i++) {
        dst[i] = src[i];
    }
    return 0;
}

#define PORT 8080
int main(int argc, char const *argv[])
{
    int server_fd;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);
    char header[5];
    char chunk[5000];
    char output_header[4];
    char output_chunk[5000];

    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    if (setsockopt(
                server_fd,
                SOL_SOCKET,
                SO_REUSEADDR | SO_REUSEPORT,
                &opt,
                sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons( PORT );

    if (bind(
            server_fd,
            (struct sockaddr *)&address,
            sizeof(address)
    ) < 0)
    {
        perror("bind failed");
        exit(EXIT_FAILURE);
    }

    int new_socket;
    printf("listen begin\n");
    if (listen(server_fd, 3) < 0) {
        perror("listen");
        exit(EXIT_FAILURE);
    }
    printf("listen end\n");

    printf("accept begin\n");
    if ((new_socket = accept(
            server_fd,
            (struct sockaddr *)&address,
            (socklen_t*)&addrlen)) < 0) {
        perror("accept");
        exit(EXIT_FAILURE);
    }
    printf("accept end\n");

    for (;;) {
        printf("recv begin\n");
        // read the request type
        recv(new_socket, header, 1, MSG_WAITALL);
        printf("recv end\n");

        char operation_type = *header;
        printf("OPERATION TYPE = %c\n", operation_type);

        if (operation_type == 'x') {
            printf("asked to stop, closing socket");
            close(new_socket);
            break;
        }

        // read the chunk size
        recv(new_socket, header+1, 4, MSG_WAITALL);

        int chunk_size = *( (int*)(header+1) );
        printf("CHUNK SIZE = %d\n", chunk_size);
        
        if (chunk_size > 4096 || chunk_size < 0) {
            printf("Invalid chunk size");
            exit(EXIT_FAILURE);
        }

        // collect the chunk
        recv(new_socket, chunk, chunk_size, MSG_WAITALL);

        int output_size = -1;

        if (operation_type == 'c') {
            // do compression
            if (compress(chunk, chunk_size, output_chunk, &output_size) < 0) {
                perror("failed to compress");
                exit(EXIT_FAILURE);
            }
        } else if (operation_type == 'd') {
            // do decompression
            if (decompress(chunk, chunk_size, output_chunk, &output_size) < 0) {
                perror("failed to decompress");
                exit(EXIT_FAILURE);
            }
        } else {
            perror("invalid operation type");
            exit(EXIT_FAILURE);
        }

        if (output_size > 4096 || output_size < 0) {
            printf("Invalid output size");
            exit(EXIT_FAILURE);
        }

        *( (int *)output_header ) = output_size;
        printf("send begin\n");
        // send the header
        if (send(new_socket, output_header, 4, 0) < 0) {
            perror("Failed to send header");
            exit(EXIT_FAILURE);
        }
        // send the chunk
        if (send(new_socket, output_chunk, output_size, 0) < 0) {
            perror("Failed to send chunk");
            exit(EXIT_FAILURE);
        }

        printf("send end\n");
    }

    return 0;
}
