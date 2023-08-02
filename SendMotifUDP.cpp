/*
  ---------------------------------- license ------------------------------------
  Copyright (C) 2023 Thierry JOUBERT.  All Rights Reserved.

  Permission is hereby granted, free of charge, to any person obtaining a copy of
  this software and associated documentation files (the "Software"), to deal in
  the Software without restriction, including without limitation the rights to
  use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
  the Software, and to permit persons to whom the Software is furnished to do so,
  subject to the following conditions:

  The above copyright notice and this permission notice shall be included in all
  copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS
  FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR
  COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER
  IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
  CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
  -------------------------------------------------------------------------------
*/

// SendMotifUDP.cpp 
//
// 1. Send an MPX binary file to the sky
// --> UDP transfer
// --> Max size is 2300 bytes
//
// T. JOUBERT
// v1.0   28 Jun. 2023     UDP socket
// v1.1   24 Jul. 2023     Multi packets
// 

#define _CRT_SECURE_NO_WARNINGS

#include <stdio.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "10.1.1.1"
#define SERVER_PORT 2023
#define MAX_BUF     2300 // biggest MPX accepted by MegaPix firmware
#define UDP_MTU     1470 // UDP maximum transfer = 1472 

#define VERSION "v1.1  2023-07-24"

char buffer[MAX_BUF] = { 0 };

int main(int argc, char** argv)
{
    WSADATA wsaData;
    SOCKET clientSocket;
    struct sockaddr_in serverAddress;

    long file_size;
    size_t result;
    FILE* fp;

    if (argc < 2)
    {
        printf("Version: %s \n\n", VERSION);
        printf("syntaxe: %s fichier_MPX_binaire\n", argv[0]);
        printf("     ex: %s snoopy.mpx\n", argv[0]);
        return 1;
    }
    
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) { // Winsock Init
        printf("Échec de l'initialisation de Winsock. Erreur : %d\n", WSAGetLastError());
        return 1;
    }

    clientSocket = socket(AF_INET, SOCK_DGRAM, 0);  // UDP socket
    if (clientSocket == INVALID_SOCKET) {
        printf("Échec de la création du socket. Erreur : %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }

    serverAddress.sin_family = AF_INET;     // Server address
    serverAddress.sin_port = htons(SERVER_PORT);
    if (inet_pton(AF_INET, SERVER_IP, &(serverAddress.sin_addr)) <= 0) {
        printf("Server IP:%s is invalid.\n", SERVER_IP);
        closesocket(clientSocket);
        WSACleanup();
        return 1;
    }

    fp = fopen(argv[1], "rb");

    fseek(fp, 0, SEEK_END);                 // get file size
    file_size = ftell(fp);
    rewind(fp);
    if (file_size > MAX_BUF)
    {
        printf("!!file %s is %ld bytes, too big - limit is %d bytes!!\n", argv[1],file_size, MAX_BUF);
        return 1;
    }
    result = fread(buffer, sizeof(char), file_size, fp); // read file bytes

    if (result != file_size) {
        printf("!!fread %s error!!\n", argv[1]);
        return 1;
    }

    fclose(fp);
    printf("Got %zd bytes from %s\n", result, argv[1]);

    if (result > UDP_MTU)                   // cut the buffer if bigger than UDP_MTU
    {
        printf("Sending %ld bytes\n", UDP_MTU);
        if (sendto(clientSocket, buffer, UDP_MTU, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
            printf("!!Socket Erreur : %d!!\n", WSAGetLastError());
        }
        Sleep(500);
        printf("Sending %zd bytes\n", result - UDP_MTU);
        if (sendto(clientSocket, buffer+UDP_MTU, (int)(result-UDP_MTU), 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
            printf("!!Socket Erreur : %d!!\n", WSAGetLastError());
        } 
    }
    else
    {
        printf("Sending %zd bytes\n", result);
        if (sendto(clientSocket, buffer, (int)result, 0, (struct sockaddr*)&serverAddress, sizeof(serverAddress)) == SOCKET_ERROR) {
            printf("Échec de l'envoi du message. Erreur : %d\n", WSAGetLastError());
        }
    }

    closesocket(clientSocket);                 // Cleanup
    WSACleanup();

    return 0;
}
