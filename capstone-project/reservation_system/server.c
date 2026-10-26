
#include "reservation.h"
#include "seatmap.h"
#include "storage.h"

#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <process.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define DEFAULT_PORT 5555
#define LINE_SIZE 512



/* Send an entire message, even if send() writes only part of it. */
static void send_text(SOCKET client_fd, const char *message)
{
    size_t remaining = strlen(message);

    while (remaining > 0) {
        int sent = send(client_fd, message, (int)remaining, 0);
        if (sent <= 0) {
            return;
        }
        message += sent;
        remaining -= (size_t)sent;
    }
}

/* Convert a positive integer argument without accepting extra characters. */
static int read_number(const char *text)
{
    char *end;
    long value;

    if (text == NULL || text[0] == '\0') {
        return -1;
    }
    errno = 0;
    value = strtol(text, &end, 10);
    if (errno != 0 || *end != '\0' || value < 1 || value > 65535) {
        return -1;
    }
    return (int)value;
}

/* Parse and execute one command while holding the database file lock. */
static void process_command(SOCKET client_fd, FILE *database_file, char *line)
{
    char *args[7] = {NULL};
    
    char output[256];
    int count = 0;
    int trip_id, seat_id;
    int changed = 0;
    Database *db;

    /* Split a command into space-separated words. */
    for (char *word = strtok(line, " \t\r\n");
         word != NULL && count < 7;
         word = strtok(NULL, " \t\r\n")) {
        args[count++] = word;
    }

    if (count == 0) {
        return;
    }
    if (count == 7) {
        send_text(client_fd, "ERR too many arguments\n");
        return;
    }
    if (strcmp(args[0], "HELP") == 0) {
        send_text(client_fd,
            "\nCOMMANDS:\n"
            "  LIST - List Trips\n  SHOW (trip #) - Show's a trip's seats and availability\n"
            "  ADD (AIR|TRAIN|BUS) (trip name) (seats) - Add's a trip of a specified type, name, and seat count\n"
            "  HOLD (trip #) (seat #) (first name) (last name) - Hold's a specified seat of a specified trip under a given name\n"
            "  CONFIRM (trip #) (seat #) (token) - Confirms a seat with a token given after placing a hold on a seat\n"
            "  CANCEL (trip #) (seat #) - Cancels the seat reservation of a given trip and seat\n"
            "  UPDATE (trip #) (seat #) (first name) (last name) - Updates the name of a given trip and seat number\n"
            "  WAIT (trip #) (first name) (last name) - Adds a name to the waitlist for when a trip is fully booked\n"
            "  DELETE (trip #) - Deletes an entire trip of a given number\n  QUIT\n\n");
        return;
    }

    /* Lock before loading, changing, and saving shared records. */
    if (storage_lock(database_file) != 0) {
        send_text(client_fd, "ERR cannot lock database\n");
        return;
    }

    db = malloc(sizeof(*db));
    if (db == NULL) {
        send_text(client_fd, "ERR out of memory\n");
        storage_unlock(database_file);
        return;
    }
    if (storage_load(database_file, db) != 0) {
        send_text(client_fd, "ERR cannot read database\n");
        goto cleanup;
    }

    expire_holds(db);
    /* An expired hold can change the database, even on a LIST request. */
    changed = 1;
    trip_id = count > 1 ? read_number(args[1]) : -1;
    seat_id = count > 2 ? read_number(args[2]) : -1;

    if (strcmp(args[0], "LIST") == 0 && count == 1) {
        for (int i = 0; i < db->count; i++) {
            Trip *trip = &db->trips[i];
            snprintf(output, sizeof(output),
                "TRIP %d %s %s seats=%d available=%d wait=%d\n",
                trip->id, trip->mode, trip->label, trip->count,
                seatmap_available(trip), trip->wait_count);
            send_text(client_fd, output);
        }
        send_text(client_fd, "END\n\n");
    } else if (strcmp(args[0], "SHOW") == 0 && count == 2) {
        Trip *trip = find_trip(db, trip_id);
        if (trip == NULL) {
            send_text(client_fd, "ERR trip not found\n");
        } else {
            snprintf(output, sizeof(output),
                "TRIP %d %s %s available=%d wait=%d\n",
                trip->id, trip->mode, trip->label,
                seatmap_available(trip), trip->wait_count);
            send_text(client_fd, output);
            for (int i = 0; i < trip->count; i++) {
                Seat *seat = &trip->seats[i];
                const char *status = seat->status == EMPTY ? "EMPTY" :
                                     seat->status == HELD ? "HELD" : "BOOKED";
                snprintf(output, sizeof(output), "SEAT %d %s %s %s\n",
                    seat->id, status, seat->first, seat->last);
                send_text(client_fd, output);
            }
            send_text(client_fd, "END\n\n");
        }
    } else if (strcmp(args[0], "ADD") == 0 && count == 4) {
        int new_id = add_trip(db, args[1], args[2], read_number(args[3]));
        if (new_id < 0) {
            send_text(client_fd, "ERR invalid trip\n");
        } else {
            snprintf(output, sizeof(output), "OK trip=%d\n\n", new_id);
            send_text(client_fd, output);
        }
    } else if (strcmp(args[0], "HOLD") == 0 && count == 5) {
        char token[40];
        if (hold_seat(db, trip_id, seat_id, args[3], args[4],
                      token, sizeof(token)) != 0) {
            send_text(client_fd, "ERR unavailable or invalid\n");
        } else {
            snprintf(output, sizeof(output), "OK token=%s expires_in=%d\n\n",
                     token, HOLD_SECONDS);
            send_text(client_fd, output);
        }
    } else if (strcmp(args[0], "CONFIRM") == 0 && count == 4) {
        send_text(client_fd, confirm_seat(db, trip_id, seat_id, args[3]) != 0 ?
                  "ERR invalid hold/token\n\n" : "OK confirmed\n\n");
    } else if (strcmp(args[0], "CANCEL") == 0 && count == 3) {
        send_text(client_fd, cancel_seat(db, trip_id, seat_id) != 0 ?
                  "ERR seat not reserved\n\n" : "OK cancelled\n\n");
    } else if (strcmp(args[0], "UPDATE") == 0 && count == 5) {
        send_text(client_fd,
                  update_passenger(db, trip_id, seat_id, args[3], args[4]) != 0 ?
                  "ERR update failed\n\n" : "OK updated\n\n");
    } else if (strcmp(args[0], "WAIT") == 0 && count == 4) {
        int position = join_waitlist(db, trip_id, args[2], args[3]);
        if (position < 0) {
            send_text(client_fd, "ERR waitlist requires full trip\n\n");
        } else {
            snprintf(output, sizeof(output), "OK wait_position=%d\n\n", position);
            send_text(client_fd, output);
        }
    } else if (strcmp(args[0], "DELETE") == 0 && count == 2) {
        int index = 0;
        while (index < db->count && db->trips[index].id != trip_id) {
            index++;
        }
        if (index == db->count) {
            send_text(client_fd, "ERR trip not found\n");
        } else {
            for (int i = index; i < db->count - 1; i++) {
                db->trips[i] = db->trips[i + 1];
            }
            db->count--;
            send_text(client_fd, "OK deleted\n\n");
        }
    } else {
        send_text(client_fd, "ERR command; type HELP\n\n");
    }

    if (changed && storage_save(database_file, db) != 0) {
        send_text(client_fd, "ERR save failed\n");
    }

cleanup:
    free(db);
    storage_unlock(database_file);
}

/* Read newline-terminated commands from one connected client. */
static void handle_client(SOCKET client_fd, FILE *database_file)
{
    char line[LINE_SIZE];
    size_t position = 0;
    int too_long = 0;
    char character;

    send_text(client_fd, "Welcome to Derek's Trip Reservation System. Type HELP for commands.\n");
    while (recv(client_fd, &character, 1, 0) == 1) {
        if (character == '\n') {
            if (too_long) {
                send_text(client_fd, "ERR line too long\n");
            } else {
                line[position] = '\0';
                if (strcmp(line, "QUIT") == 0) {
                    send_text(client_fd, "BYE\n");
                    break;
                }
                process_command(client_fd, database_file, line);
            }
            position = 0;
            too_long = 0;
        } else if (!too_long) {
            if (position < sizeof(line) - 1) {
                line[position++] = character;
            } else {
                too_long = 1;
            }
        }
    }
}

typedef struct {
    SOCKET socket;
    char path[MAX_PATH];
} ClientArgs;

/* A thread handles each connection, with its own FILE stream. */
static unsigned __stdcall client_thread(void *argument)
{
    ClientArgs *args = argument;
    SOCKET client = args->socket;
    FILE *file = storage_open(args->path);
    free(args);
    if (file != NULL) {
        handle_client(client, file);
        fclose(file);
    }
    closesocket(client);
    return 0;
}

int main(int argc, char *argv[])
{
    int port = argc > 1 ? read_number(argv[1]) : DEFAULT_PORT;
    const char *path = argc > 2 ? argv[2] : DB_PATH;
    WSADATA winsock;
    SOCKET server_fd;
    struct sockaddr_in address = {0};
    FILE *check;

    if (port < 1 || strlen(path) >= MAX_PATH) {
        fprintf(stderr, "Invalid port or database path.\n");
        return 1;
    }
    if (WSAStartup(MAKEWORD(2, 2), &winsock) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        return 1;
    }
    /* Create the database before accepting connections. */
    check = storage_open(path);
    if (check == NULL) {
        perror("open database");
        WSACleanup();
        return 1;
    }
    fclose(check);

    server_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_fd == INVALID_SOCKET) {
        fprintf(stderr, "socket failed: %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
    }
    address.sin_family = AF_INET;
    address.sin_port = htons((unsigned short)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) == SOCKET_ERROR ||
        listen(server_fd, 16) == SOCKET_ERROR) {
        fprintf(stderr, "bind/listen failed: %d\n", WSAGetLastError());
        closesocket(server_fd);
        WSACleanup();
        return 1;
    }
    printf("Reservation server on 127.0.0.1:%d\n", port);
    fflush(stdout);

    for (;;) {
        SOCKET client = accept(server_fd, NULL, NULL);
        if (client == INVALID_SOCKET) {
            fprintf(stderr, "accept failed: %d\n", WSAGetLastError());
            break;
        }
        ClientArgs *args = malloc(sizeof(*args));
        if (args == NULL) {
            closesocket(client);
            continue;
        }
        args->socket = client;
        strcpy(args->path, path);
        uintptr_t thread = _beginthreadex(NULL, 0, client_thread, args, 0, NULL);
        if (thread == 0) {
            free(args);
            closesocket(client);
        } else {
            CloseHandle((HANDLE)thread);
        }
    }
    closesocket(server_fd);
    WSACleanup();
    return 0;
}
