// Client side C/C++ program to demonstrate Socket programming 
#include <stdio.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <unistd.h> 
#include <string.h> 
#define PORT 8080 
   
int main(int argc, char const *argv[]) 
{ 
    int sock = 0; 
    struct sockaddr_in serv_addr; 
    char hello[10];
    hello[0] = 'c';
    *( (int*)(hello+1) ) = 1;
    hello[5] = 'a';
    hello[6] = 'y';
    char buffer[1024] = {0}; 
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) 
    { 
        printf("\n Socket creation error \n"); 
        return -1; 
    } 
   
    serv_addr.sin_family = AF_INET; 
    serv_addr.sin_port = htons(PORT); 
       
    // Convert IPv4 and IPv6 addresses from text to binary form 
    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr)<=0)  
    { 
        printf("\nInvalid address/ Address not supported \n"); 
        return -1; 
    } 
   
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) 
    { 
        printf("\nConnection Failed \n"); 
        return -1; 
    } 

    printf("finished connecting! sleeping... \n");
    sleep(1);

    for (int i = 0; i < 2; i++) {
        send(sock, hello, 5, 0); 
        recv(sock, buffer, 4, MSG_WAITALL); 
        printf("%s\n",buffer); 
        sleep(1);
    }

    send(sock, "x", 1, 0); 
    close(sock);
    
    //send(sock , hello , strlen(hello) , 0 ); 
    //printf("Another message sent\n"); 
    //read( sock , buffer, 1024); 
    //printf("%s\n",buffer ); 
    return 0; 
}
