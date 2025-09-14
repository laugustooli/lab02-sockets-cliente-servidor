#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <pthread.h>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 12345
#define BUF_SIZE 4096
#define QUEUE_SIZE 10
#define NUM_MAX_CLIENTS 50

typedef struct {
    char ip_addr[INET_ADDRSTRLEN];
    time_t last_access_time;
} ClientState;

ClientState client_states[NUM_MAX_CLIENTS];
int num_clients = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;

void fatal(const char *string) {
    perror(string);
    exit(1);
}

void *connection_handler(void *socket_desc) {
    SOCKET sock = (SOCKET)socket_desc;
    free(socket_desc);
    char buf[BUF_SIZE];
    int read_size;

    struct sockaddr_in client_addr;
    int client_len = sizeof(client_addr);
    getpeername(sock, (struct sockaddr*)&client_addr, &client_len);
    char *client_ip = inet_ntoa(client_addr.sin_addr);

    memset(buf, 0, BUF_SIZE);
    read_size = recv(sock, buf, BUF_SIZE - 1, 0);

    if (read_size <= 0) {
        fprintf(stderr, "Cliente %s desconectou ou ocorreu um erro na leitura.\n", client_ip);
        closesocket(sock);
        return 0;
    }

    buf[strcspn(buf, "\r\n")] = 0;

    if (strncmp(buf, "MyGet ", 6) == 0) {
        char *filepath = buf + 6;
        FILE *file = fopen(filepath, "rb"); 
        if (file == NULL) {
            send(sock, "ERROR FILE_NOT_FOUND\n", 23, 0);
        } else {
            send(sock, "OK\n", 3, 0);
            size_t bytes_read;
            while ((bytes_read = fread(buf, 1, BUF_SIZE, file)) > 0) {
                send(sock, buf, bytes_read, 0);
            }
            fclose(file);

            pthread_mutex_lock(&clients_mutex);
            int client_found = 0;
            for (int i = 0; i < num_clients; i++) {
                if (strcmp(client_states[i].ip_addr, client_ip) == 0) {
                    client_states[i].last_access_time = time(NULL);
                    client_found = 1;
                    break;
                }
            }
            if (!client_found && num_clients < NUM_MAX_CLIENTS) {
                strcpy(client_states[num_clients].ip_addr, client_ip);
                client_states[num_clients].last_access_time = time(NULL);
                num_clients++;
            }
            pthread_mutex_unlock(&clients_mutex);
        }
    } else if (strcmp(buf, "MyLastAccess") == 0) {
        pthread_mutex_lock(&clients_mutex);
        char response_buf[128];
        int client_found = 0;
        for (int i = 0; i < num_clients; i++) {
            if (strcmp(client_states[i].ip_addr, client_ip) == 0) {
                struct tm *tm_info = localtime(&client_states[i].last_access_time);
                strftime(response_buf, sizeof(response_buf), "Last Access =%Y-%m-%d %H:%M:%S\n", tm_info);
                send(sock, response_buf, strlen(response_buf), 0);
                client_found = 1;
                break;
            }
        }
        if (!client_found) {
            send(sock, "Last Access =Null\n", 18, 0);
        }
        pthread_mutex_unlock(&clients_mutex);
    } else {
        send(sock, "ERROR INVALID_COMMAND\n", 24, 0);
    }

    printf("Handler: Fechando conexao com o cliente %s\n", client_ip);
    closesocket(sock);
    return 0;
}

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fatal("WSAStartup failed");
    } 
    SOCKET s, sa;       
    long b, l, on = 1;  
    SOCKET *new_sock;
    struct sockaddr_in channel, client_addr; 

    memset(&channel, 0, sizeof(channel));
    channel.sin_family = AF_INET;
    channel.sin_addr.s_addr = htonl(INADDR_ANY);
    channel.sin_port = htons(SERVER_PORT);

    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == INVALID_SOCKET) {
        fatal("socket failed");
    }

    setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char *)&on, sizeof(on));

    b = bind(s, (struct sockaddr *)&channel, sizeof(channel));
    if (b == SOCKET_ERROR) {
        fatal("bind failed");
    }

    l = listen(s, QUEUE_SIZE);
    if (l == SOCKET_ERROR) {
        fatal("listen failed");
    }

    printf("Servidor esperando por conexoes na porta %d...\n", SERVER_PORT);

    int c = sizeof(struct sockaddr_in);
    while ((sa = accept(s, (struct sockaddr *)&client_addr, &c))) {
        if (sa == INVALID_SOCKET) {
            fatal("accept failed");
        }

        char *client_ip = inet_ntoa(client_addr.sin_addr);
        int client_port = ntohs(client_addr.sin_port);
        printf("Conexao aceita de %s:%d\n", client_ip, client_port);

        new_sock = (SOCKET*)malloc(sizeof(SOCKET));
        *new_sock = sa;

        pthread_t sniffer_thread;
        if (pthread_create(&sniffer_thread, NULL, connection_handler, (void*)new_sock) < 0) {
            fatal("could not create thread");
        }
    }
    WSACleanup();
    return 0;
}