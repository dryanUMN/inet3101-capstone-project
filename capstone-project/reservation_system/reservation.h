#ifndef RESERVATION_H
#define RESERVATION_H
#include <stddef.h>
#include <stdint.h>
#define MAX_TRIPS 32
#define MAX_SEATS 64
#define MAX_WAIT 64
#define NAME_LEN 50
#define LABEL_LEN 64
#define HOLD_SECONDS 120
#define DB_PATH "reservations.dat"

/* Define Structures */
typedef enum { EMPTY=0, HELD=1, BOOKED=2 } SeatStatus;
typedef struct { int id; SeatStatus status; char first[NAME_LEN],last[NAME_LEN]; int64_t expires; char token[40]; } Seat;
typedef struct { char first[NAME_LEN],last[NAME_LEN]; } Passenger;
typedef struct { int id; char mode[12], label[LABEL_LEN]; int count,wait_count; Seat seats[MAX_SEATS]; Passenger wait[MAX_WAIT]; } Trip;
typedef struct { int count; Trip trips[MAX_TRIPS]; uint64_t next_token; } Database;

/* Define functions */
int valid_name(const char *s);
int add_trip(Database *db,const char *mode,const char *label,int seats);
Trip *find_trip(Database *db,int id);
void expire_holds(Database *db);
int hold_seat(Database *db,int trip,int seat,const char *first,const char *last,char *token,size_t size);
int confirm_seat(Database *db,int trip,int seat,const char *token);
int cancel_seat(Database *db,int trip,int seat);
int update_passenger(Database *db,int trip,int seat,const char *first,const char *last);
int join_waitlist(Database *db,int trip,const char *first,const char *last);
void promote_waitlist(Trip *trip);
#endif
