/*
 ============================================================================
 Name        : main.c
 Author      : Bilanzuoli Filippo
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0601
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "protocol.h"

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  typedef SOCKET sock_t;
  #define CLOSESOCK(s) closesocket(s)
#else
  #include <unistd.h>
  #include <errno.h>
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netdb.h>
  #include <arpa/inet.h>
  #include <netinet/in.h>
  typedef int sock_t;
  #define INVALID_SOCKET (-1)
  #define CLOSESOCK(s) close(s)
#endif

/* Funzioni simulate per valori meteo */
float rand_float_range(float lo, float hi) {
    float r = (float)rand() / (float)RAND_MAX;
    return lo + r * (hi - lo);
}
float get_temperature(void) { return rand_float_range(-10.0f, 40.0f); }
float get_humidity(void)    { return rand_float_range(20.0f, 100.0f); }
float get_wind(void)        { return rand_float_range(0.0f, 100.0f); }
float get_pressure(void)    { return rand_float_range(950.0f, 1050.0f); }

/* Confronto case-insensitive */
static int ci_equal(const char* a, const char* b) {
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b)) return 0;
    }
    return *a == *b;
}

/* Rimuove spazi iniziali e finali */
static void trim_inplace(char* s) {
    char *p = s;
    while (*p == ' ') p++;
    if (p != s) memmove(s, p, strlen(p)+1);
    size_t len = strlen(s);
    while (len > 0 && s[len-1] == ' ') { s[len-1] = '\0'; len--; }
}

/* Città supportate */
static const char* supported_cities[] = {
    "Bari","Roma","Milano","Napoli","Torino","Palermo","Genova","Bologna","Firenze","Venezia"
};
static const size_t supported_cities_count = sizeof(supported_cities)/sizeof(supported_cities[0]);

int main(int argc, char* argv[]) {
    const char* port = DEFAULT_PORT;
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = argv[2];
    } else if (argc != 1) {
        fprintf(stderr, "Uso: %s [-p port]\n", argv[0]);
        return 1;
    }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }
#endif

    sock_t sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd == INVALID_SOCKET) {
        perror("socket");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons((unsigned short)atoi(port));
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("bind");
        CLOSESOCK(sockfd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    srand((unsigned int)time(NULL));

    printf("Server UDP in ascolto sulla porta %s...\n", port);

    for (;;) {
        struct sockaddr_in client_addr;
        socklen_t addrlen = sizeof(client_addr);
        unsigned char reqbuf[1 + CITY_NAME_LEN] = {0};

        ssize_t n = recvfrom(sockfd, (char*)reqbuf, sizeof(reqbuf), 0,
                             (struct sockaddr*)&client_addr, &addrlen);
        if (n <= 0) continue;

        char rtype = (char)reqbuf[0];
        char city[CITY_NAME_LEN+1] = {0};
        if (n > 1) {
            size_t copy_len = (size_t)n-1;
            if (copy_len > CITY_NAME_LEN) copy_len = CITY_NAME_LEN;
            memcpy(city, &reqbuf[1], copy_len);
        }
        trim_inplace(city);

        printf("Richiesta '%c %s'\n", rtype, city);
        fflush(stdout);

        int type_ok = (rtype=='t'||rtype=='h'||rtype=='w'||rtype=='p');
        int city_ok = 0;
        for (size_t i = 0; i < supported_cities_count; i++)
            if (ci_equal(city, supported_cities[i])) { city_ok = 1; break; }

        uint32_t status;
        char resp_type = '\0';
        float value = 0.0f;

        if (!type_ok) {
            status = STATUS_INVALID_REQUEST;
        } else if (!city_ok) {
            status = STATUS_CITY_UNAVAILABLE;
        } else {
            status = STATUS_SUCCESS;
            resp_type = rtype;
            switch (rtype) {
                case 't': value = get_temperature(); break;
                case 'h': value = get_humidity(); break;
                case 'w': value = get_wind(); break;
                case 'p': value = get_pressure(); break;
            }
        }

        unsigned char respbuf[9] = {0};
        uint32_t net_status = htonl(status);
        memcpy(respbuf, &net_status, 4);
        respbuf[4] = resp_type;
        uint32_t value_u32;
        memcpy(&value_u32, &value, sizeof(value_u32));
        value_u32 = htonl(value_u32);
        memcpy(&respbuf[5], &value_u32, 4);

        sendto(sockfd, (char*)respbuf, sizeof(respbuf), 0,
               (struct sockaddr*)&client_addr, addrlen);
    }

    CLOSESOCK(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
