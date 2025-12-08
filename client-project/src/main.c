/*
 ============================================================================
 Name        : EsoneroUDP.c
 Author      : Bilanzuoli Filippo
 Version     :
 Copyright   : Your copyright notice
 Description : Hello World in C, Ansi-style
 ============================================================================
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <winsock2.h>
#include <windows.h>
typedef SOCKET sock_t;
#define CLOSESOCK closesocket
#else
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
typedef int sock_t;
#define INVALID_SOCKET (-1)
#define CLOSESOCK close
#endif

#include "protocol.h"

static void usage(const char* prog) {
    fprintf(stderr, "Uso: %s [-s server] [-p port] -r \"type city\"\n", prog);
    exit(1);
}

static void trim_inplace(char* s) {
    char *p = s;
    while (*p == ' ') p++;
    if (p != s) memmove(s, p, strlen(p)+1);
    size_t len = strlen(s);
    while (len > 0 && s[len-1] == ' ') { s[len-1] = '\0'; len--; }
}

static void normalize_city(char* city) {
    if (city[0] >= 'a' && city[0] <= 'z') city[0] -= 32;
    for (int i=1; city[i]; i++) {
        if (city[i] >= 'A' && city[i] <= 'Z') city[i] += 32;
    }
}

int main(int argc, char* argv[]) {
    const char* server = DEFAULT_SERVER;
    const char* port = DEFAULT_PORT;
    char request_raw[1024] = {0};
    int have_request = 0;

    for (int i=1; i<argc; i++) {
        if (strcmp(argv[i], "-s")==0 && i+1<argc) { server = argv[i+1]; i++; }
        else if (strcmp(argv[i], "-p")==0 && i+1<argc) { port = argv[i+1]; i++; }
        else if (strcmp(argv[i], "-r")==0 && i+1<argc) { strncpy(request_raw, argv[i+1], sizeof(request_raw)-1); have_request=1; i++; }
        else usage(argv[0]);
    }

    if (!have_request) usage(argv[0]);
    trim_inplace(request_raw);
    if (strlen(request_raw)<2) usage(argv[0]);

    char rtype = request_raw[0];
    const char* rest = request_raw + 1;
    while (*rest==' ') rest++;

    if (strchr(rest,'\t')) { fprintf(stderr,"Errore: tabulazioni non permesse.\n"); return 1; }

    char city[CITY_NAME_LEN];
    memset(city,0,sizeof(city));
    strncpy(city, rest, CITY_NAME_LEN-1);
    if (strlen(city) >= CITY_NAME_LEN) { fprintf(stderr,"Errore: nome città troppo lungo.\n"); return 1; }

#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2,2), &wsa) !=0) return 1;
#endif

    /* --- RESOLUZIONE SERVER --- */
    struct sockaddr_in server_addr;
    memset(&server_addr,0,sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(atoi(port));

    if (strcmp(server,"localhost")==0) {
        server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");
    } else {
        server_addr.sin_addr.s_addr = inet_addr(server);
        if (server_addr.sin_addr.s_addr == INADDR_NONE) {
            fprintf(stderr,"Errore: indirizzo server non valido\n");
#ifdef _WIN32
            WSACleanup();
#endif
            return 1;
        }
    }

    sock_t sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock == INVALID_SOCKET) {
        fprintf(stderr,"Errore: impossibile creare socket UDP\n");
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    /* --- SERIALIZZAZIONE RICHIESTA --- */
    unsigned char reqbuf[1 + CITY_NAME_LEN];
    memset(reqbuf,0,sizeof(reqbuf));
    reqbuf[0]=(unsigned char)rtype;
    strncpy((char*)&reqbuf[1],city,CITY_NAME_LEN-1);

    ssize_t sent = sendto(sock,(const char*)reqbuf,sizeof(reqbuf),0,
                          (struct sockaddr*)&server_addr,sizeof(server_addr));
    if (sent != sizeof(reqbuf)) {
        fprintf(stderr,"Errore: sendto fallita\n");
        CLOSESOCK(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    /* --- RICEZIONE RISPOSTA --- */
    unsigned char respbuf[9];
    struct sockaddr_in from;
    int fromlen = sizeof(from);
    ssize_t rec = recvfrom(sock, (char*)respbuf, sizeof(respbuf), 0,
                           (struct sockaddr*)&from, &fromlen);
    if (rec != sizeof(respbuf)) {
        fprintf(stderr,"Errore: risposta incompleta\n");
        CLOSESOCK(sock);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    CLOSESOCK(sock);

    /* --- DESERIALIZZAZIONE --- */
    uint32_t net_status, net_val32;
    memcpy(&net_status,&respbuf[0],4);
    char resp_type = respbuf[4];
    memcpy(&net_val32,&respbuf[5],4);

    uint32_t status = ntohl(net_status);
    uint32_t val32 = ntohl(net_val32);

    float value;
    memcpy(&value,&val32,sizeof(float));

    normalize_city(city);

    /* --- OUTPUT --- */
    printf("Ricevuto risultato dal server %s (ip %s). ",
           (strcmp(server,"localhost")==0)?"localhost":server,
           (strcmp(server,"localhost")==0)?"127.0.0.1":server);

    if (status == STATUS_SUCCESS) {
        if (resp_type=='t') printf("%s: Temperatura = %.1f°C\n",city,value);
        else if (resp_type=='h') printf("%s: Umidità = %.1f%%\n",city,value);
        else if (resp_type=='w') printf("%s: Vento = %.1f km/h\n",city,value);
        else if (resp_type=='p') printf("%s: Pressione = %.1f hPa\n",city,value);
        else printf("Richiesta non valida\n");
    } else if (status==STATUS_CITY_UNAVAILABLE) {
        printf("Città non disponibile\n");
    } else {
        printf("Richiesta non valida\n");
    }

#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
