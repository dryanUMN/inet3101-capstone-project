#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define BUFFER_SIZE 1024

/* Receive messages while the main thread reads keyboard commands. */
static unsigned __stdcall receive_thread(void *argument)
{
    SOCKET connection = *(SOCKET *)argument;
    char buffer[BUFFER_SIZE];
    for (;;) {
        int received = recv(connection, buffer, sizeof(buffer), 0);
        if (received <= 0) break;
        fwrite(buffer, 1, (size_t)received, stdout);
        fflush(stdout);
    }
    return 0;
}

int main(int argc, char *argv[])
{
    const char *host = argc > 1 ? argv[1] : "127.0.0.1";
    int port = argc > 2 ? atoi(argv[2]) : 5555;
    WSADATA winsock;
    SOCKET connection;
    struct sockaddr_in address = {0};
    char buffer[BUFFER_SIZE];

    if (port < 1 || port > 65535) {
        fprintf(stderr, "Invalid port number.\n");
        return 1;
    }
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
    connection = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (connection == INVALID_SOCKET) {
        fprintf(stderr, "socket failed.\n");
        WSACleanup();
        return 1;
    }
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    if (InetPtonA(AF_INET, host, &address.sin_addr) != 1 ||
        connect(connection, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR) {
        fprintf(stderr, "Cannot connect: %d\n", WSAGetLastError());
        closesocket(connection);
        WSACleanup();
        return 1;
    }

    uintptr_t thread = _beginthreadex(NULL, 0, receive_thread, &connection, 0, NULL);
    if (thread == 0) {
        fprintf(stderr, "Cannot start receiver.\n");
        closesocket(connection);
        WSACleanup();
        return 1;
    }
    while (fgets(buffer, sizeof(buffer), stdin) != NULL) {
        size_t length = strlen(buffer);
        if (length == 0) continue;
        if (buffer[length - 1] != '\n') {
            int ch;
            while ((ch = getchar()) != '\n' && ch != EOF) {}
            fprintf(stderr, "Command too long.\n");
            continue;
        }
        size_t sent_total = 0;
        while (sent_total < length) {
            int sent = send(connection, buffer + sent_total, (int)(length - sent_total), 0);
            if (sent <= 0) goto finished;
            sent_total += (size_t)sent;
        }
        if (strcmp(buffer, "QUIT\n") == 0) break;
    }
finished:
    shutdown(connection, SD_SEND);
    WaitForSingleObject((HANDLE)thread, 2000);
    closesocket(connection);
    CloseHandle((HANDLE)thread);
    WSACleanup();
    return 0;
}
