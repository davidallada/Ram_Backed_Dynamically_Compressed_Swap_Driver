#!/usr/bin/python3
import socket
import sys
from datetime import datetime

PORT = 8082

# Create a TCP/IP socket
sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
sock.bind(('localhost', 8082))

sock.listen(1)
while True:
    connection, client_address = sock.accept()
    print("Got a connection request from", client_address)
    message = ''
    while True:
        data = connection.recv(1)
        if data:
            dec = data.decode('utf-8')
            message += dec
            if dec == '\0':
                print('[', datetime.now(), ']', message)
                message = ''
        else:
            break
