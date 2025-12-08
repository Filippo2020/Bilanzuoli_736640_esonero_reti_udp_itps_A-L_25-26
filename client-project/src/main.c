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
#include <ctype.h>

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

#define DEFAULT_PORT "12345"
#define CITY_NAME_LEN 32

static const char* supported_cities[] = {
    "Bari","Roma","Milano","Napoli","Torino","Palermo",
    "Genova","Bologna","Firenze","Venezia"
};
static const size_t supported_cities_count =
    sizeof(supported_cities)/sizeof(supported_cities[0]);

/* Confronto case-insensitive */
static int ci_equal(const char* a, const char* b) {
    for (; *a && *b; a++, b++) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
    }
    return *a == *b;
}

/* Controlla se la stringa contiene solo lettere */
static int is_valid_city(const char* s) {
    if (!s || !*s) return 0;
    for (; *s; s++) {
        if (!isalpha((unsigned char)*s)) return 0;
    }
    return 1;
}

int main(int argc, char* argv[]) {

    const char* port = DEFAULT_PORT;

    /* FIX: accetta solo -p porta, IGNORA tutto il resto */
    if (argc == 3 && strcmp(argv[1], "-p") == 0) {
        port = argv[2];
    } else if (argc > 1) {
        /* Argomenti non riconosciuti → ignorali */
        fprintf(stderr, "Argomenti non riconosciuti, uso porta di default.\n");
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
    server_addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

    char input[64];
    printf("Inserisci richiesta (tipo città, es. t Roma): ");

    if (!fgets(input, sizeof(input), stdin)) {
        fprintf(stderr, "Errore lettura input\n");
        CLOSESOCK(sockfd);
        return 1;
    }

    input[strcspn(input, "\r\n")] = 0;

    char rtype;
    char city[CITY_NAME_LEN+1] = {0};

    if (sscanf(input, " %c %32[^\n]", &rtype, city) != 2) {
        printf("Richiesta non valida\n");
        CLOSESOCK(sockfd);
        return 0;
    }

    /* Controllo tipo */
    if (rtype != 't' && rtype != 'h' && rtype != 'w' && rtype != 'p') {
        printf("Richiesta non valida\n");
        CLOSESOCK(sockfd);
        return 0;
    }

    /* Controllo città */
    if (!is_valid_city(city)) {
        printf("Richiesta non valida\n");
        CLOSESOCK(sockfd);
        return 0;
    }

    int found = 0;
    for (size_t i = 0; i < supported_cities_count; i++) {
        if (ci_equal(city, supported_cities[i])) {
            found = 1;
            break;
        }
    }

    if (!found) {
        printf("Città non disponibile\n");
        CLOSESOCK(sockfd);
        return 0;
    }

    unsigned char reqbuf[1 + CITY_NAME_LEN] = {0};
    reqbuf[0] = (unsigned char)rtype;

    size_t len = strlen(city);
    if (len > CITY_NAME_LEN) len = CITY_NAME_LEN;
    memcpy(&reqbuf[1], city, len);

    if (sendto(sockfd, (char*)reqbuf, sizeof(reqbuf), 0,
               (struct sockaddr*)&server_addr,
               sizeof(server_addr)) < 0) {
        perror("sendto");
        CLOSESOCK(sockfd);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    printf("Richiesta inviata al server.\n");

    CLOSESOCK(sockfd);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
