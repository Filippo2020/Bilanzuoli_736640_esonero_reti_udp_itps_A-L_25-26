/*
 ============================================================================
 Name        : main.c
 Author      : Bilanzuoli Filippo
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib")
typedef SOCKET sock_t;
#define CLOSESOCK closesocket
#else
#include <unistd.h>
#include <sys/socket.h>
#include <arpa/inet.h>
typedef int sock_t;
#define CLOSESOCK close
#define INVALID_SOCKET -1
#endif

#include "protocol.h"

#define BUF_SIZE 1024

/* Rimuove spazi e newline */
void trim(char *s) {
    size_t len = strlen(s);
    while (len > 0 && (s[len-1] == '\n' || s[len-1] == ' '))
        s[--len] = '\0';

    int start = 0;
    while (s[start] == ' ') start++;
    if (start > 0) memmove(s, s + start, strlen(s) - start + 1);
}

/* Rende spazi multipli → uno */
void normalize_spaces(char *str) {
    char tmp[BUF_SIZE];
    int j = 0, space = 0;
    for (int i = 0; str[i]; i++) {
        if (str[i] == ' ') {
            if (!space) tmp[j++] = ' ';
            space = 1;
        } else {
            tmp[j++] = str[i];
            space = 0;
        }
    }
    tmp[j] = '\0';
    strcpy(str, tmp);
}

int main() {

#ifdef _WIN32
    WSADATA wsa;
    WSAStartup(MAKEWORD(2,2), &wsa);
#endif

    char city[CITY_NAME_LEN+1];
    char type;

    printf("Tipo richiesta (t/h/w/p): ");
    scanf(" %c", &type);
    getchar(); // pulisce buffer

    printf("Inserisci la città: ");
    fgets(city, sizeof(city), stdin);

    trim(city);
    normalize_spaces(city);

    sock_t sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        perror("socket");
        return 1;
    }

    struct sockaddr_in serv;
    memset(&serv, 0, sizeof(serv));
    serv.sin_family = AF_INET;
    serv.sin_port = htons(atoi(DEFAULT_PORT));
    serv.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    /* Pacchetto richiesta: 1 byte tipo + nome città */
    unsigned char req[1 + CITY_NAME_LEN] = {0};
    req[0] = type;
    strncpy((char*)&req[1], city, CITY_NAME_LEN);

    sendto(sock, (char*)req, 1 + strlen(city), 0,
           (struct sockaddr*)&serv, sizeof(serv));

    /* Ricezione risposta */
    unsigned char resp[9];
    socklen_t slen = sizeof(serv);

    int r = recvfrom(sock, (char*)resp, sizeof(resp), 0,
                     (struct sockaddr*)&serv, &slen);

    if (r == 9) {
        uint32_t status = ntohl(*(uint32_t*)&resp[0]);
        char rtype = resp[4];
        float value;
        memcpy(&value, &resp[5], 4);

        if (status == STATUS_INVALID_REQUEST) {
            printf("Richiesta non valida\n");
        }
        else if (status == STATUS_CITY_UNAVAILABLE) {
            printf("Città non disponibile\n");
        }
        else if (status == STATUS_SUCCESS) {
            if (rtype == 't') printf("Temperatura = %.2f\n", value);
            else if (rtype == 'h') printf("Umidità = %.2f\n", value);
            else if (rtype == 'w') printf("Vento = %.2f\n", value);
            else if (rtype == 'p') printf("Pressione = %.2f\n", value);
        }
    }

    CLOSESOCK(sock);

#ifdef _WIN32
    WSACleanup();
#endif

    return 0;
}
