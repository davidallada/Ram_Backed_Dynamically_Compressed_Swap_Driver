#ifndef _network_compression_client_h_
#define _network_compression_client_h_

#define PORT 8082

/*
 * Initializes the TCP connection to the logging service.
 * Call this on module init.
 */
int
logger_connect(void);


/*
 * Ends the TCP connection to the logging service.
 * Call this on module exit.
 */
void
logger_exit(void);


/*
 * Takes a a proper string and sends it to the logging server.
 * Returns 0 on success, -1 on failure.
 */
int
log_network(char * msg);

#endif
