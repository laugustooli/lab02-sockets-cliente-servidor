#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>

#pragma comment(lib, "Ws2_32.lib")

#define SERVER_PORT 12345
#define BUF_SIZE 4096


void fatal(const char* msg) {
    int error_code = WSAGetLastError();
    char* err_msg = NULL;

    FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS,
                   NULL, error_code, MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), (LPSTR)&err_msg, 0, NULL);
    
    fprintf(stderr, "%s: Erro %d: %s\n", msg, error_code, err_msg);
    LocalFree(err_msg);
    exit(1);
}


int main(int argc, char *argv[]) {
    if (argc < 3) {
        fprintf(stderr, "Uso: %s <IP do Servidor> <Comando> [Argumento]\n", argv[0]);
        fprintf(stderr, "Comandos:\n");
        fprintf(stderr, "  MyGet <caminho_do_arquivo>\n");
        fprintf(stderr, "  MyLastAccess\n");
        return 1;
    }

    char *server_ip = argv[1];
    char *command = argv[2];

    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fatal("WSAStartup failed");
    }

    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == INVALID_SOCKET) {
        fatal("socket creation failed");
    }

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        fatal("connect failed");
    }

    printf("Conectado ao servidor %s\n", server_ip);

    char request_buf[BUF_SIZE];
    char response_buf[BUF_SIZE];

    if (strcmp(command, "MyGet") == 0 && argc == 4) {
        char *filepath = argv[3];
        sprintf(request_buf, "MyGet %s\n", filepath);
        send(client_socket, request_buf, strlen(request_buf), 0);

        memset(response_buf, 0, BUF_SIZE);
        int bytes_received = recv(client_socket, response_buf, BUF_SIZE - 1, 0);

        if (strncmp(response_buf, "OK\n", 3) == 0) {
            printf("Resposta do servidor: OK. Recebendo arquivo '%s'...\n", filepath);
            FILE *file = fopen(filepath, "wb"); 
            if (file == NULL) {
                fatal("Nao foi possivel criar o arquivo local.");
            }

            if (bytes_received > 3) {
                fwrite(response_buf + 3, 1, bytes_received - 3, file);
            }

            while ((bytes_received = recv(client_socket, response_buf, BUF_SIZE, 0)) > 0) {
                fwrite(response_buf, 1, bytes_received, file);
            }
            fclose(file);
            printf("Arquivo recebido com sucesso!\n");
        } else {
            printf("Resposta do servidor:\n%s", response_buf);
        }

    } else if (strcmp(command, "MyLastAccess") == 0) {
        sprintf(request_buf, "MyLastAccess\n");
        send(client_socket, request_buf, strlen(request_buf), 0);
        
        memset(response_buf, 0, BUF_SIZE);
        recv(client_socket, response_buf, BUF_SIZE - 1, 0);
        printf("Resposta do servidor: %s", response_buf);

    } else {
        fprintf(stderr, "Comando invalido ou argumentos insuficientes.\n");
    }
    closesocket(client_socket);
    WSACleanup();
    return 0;
}