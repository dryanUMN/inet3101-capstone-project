#include "reservation.h"
#include "seatmap.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

/* A passenger name must fit the fixed-size database field. */
int valid_name(const char *name)
{
    size_t length = strlen(name);
    if (length == 0 || length >= NAME_LEN) {
        return 0;
    }
    for (size_t i = 0; i < length; i++) {
        if ((unsigned char)name[i] < 32 || (unsigned char)name[i] > 126) {
            return 0;
        }
    }
    return 1;
}

Trip *find_trip(Database *db, int id)
{
    for (int i = 0; i < db->count; i++) {
        if (db->trips[i].id == id) {
            return &db->trips[i];
        }
    }
    return NULL;
}

int add_trip(Database *db, const char *mode, const char *label, int seats)
{
    if (db->count >= MAX_TRIPS || seats < 1 || seats > MAX_SEATS ||
        label[0] == '\0' || strlen(label) >= LABEL_LEN) {
        return -1;
    }
    if (strcmp(mode, "AIR") != 0 && strcmp(mode, "TRAIN") != 0 &&
        strcmp(mode, "BUS") != 0) {
        return -1;
    }

    Trip *trip = &db->trips[db->count];
    memset(trip, 0, sizeof(*trip));
    trip->id = db->count ? db->trips[db->count - 1].id + 1 : 1;
    snprintf(trip->mode, sizeof(trip->mode), "%s", mode);
    snprintf(trip->label, sizeof(trip->label), "%s", label);
    seatmap_init(trip, seats);
    db->count++;
    return trip->id;
}

/* Give an empty seat to the first person waiting, if any. */
void promote_waitlist(Trip *trip)
{
    if (trip->wait_count == 0) {
        return;
    }
    for (int i = 0; i < trip->count; i++) {
        Seat *seat = &trip->seats[i];
        if (seat->status == EMPTY) {
            Passenger passenger = trip->wait[0];
            trip->wait_count--;
            memmove(trip->wait, trip->wait + 1,
                    (size_t)trip->wait_count * sizeof(Passenger));
            seat->status = BOOKED;
            snprintf(seat->first, sizeof(seat->first), "%s", passenger.first);
            snprintf(seat->last, sizeof(seat->last), "%s", passenger.last);
            return;
        }
    }
}

/* A hold expires after HOLD_SECONDS unless it is confirmed. */
void expire_holds(Database *db)
{
    int64_t now = (int64_t)time(NULL);
    for (int i = 0; i < db->count; i++) {
        Trip *trip = &db->trips[i];
        for (int j = 0; j < trip->count; j++) {
            Seat *seat = &trip->seats[j];
            if (seat->status == HELD && seat->expires <= now) {
                int seat_id = seat->id;
                memset(seat, 0, sizeof(*seat));
                seat->id = seat_id;
                promote_waitlist(trip);
            }
        }
    }
}

int hold_seat(Database *db, int trip_id, int seat_id,
              const char *first, const char *last,
              char *token, size_t token_size)
{
    Seat *seat = seatmap_get(find_trip(db, trip_id), seat_id);
    if (seat == NULL || seat->status != EMPTY || !valid_name(first) ||
        !valid_name(last) || token_size < 40) {
        return -1;
    }

    seat->status = HELD;
    snprintf(seat->first, sizeof(seat->first), "%s", first);
    snprintf(seat->last, sizeof(seat->last), "%s", last);
    seat->expires = (int64_t)time(NULL) + HOLD_SECONDS;

    /* A unique classroom-demo token, NOT a secure authentication token. */
    snprintf(seat->token, sizeof(seat->token), "%016llx%016llx",
             (unsigned long long)++db->next_token,
             (unsigned long long)seat->expires);
    snprintf(token, token_size, "%s", seat->token);
    return 0;
}

int confirm_seat(Database *db, int trip_id, int seat_id, const char *token)
{
    Seat *seat = seatmap_get(find_trip(db, trip_id), seat_id);
    if (seat == NULL || seat->status != HELD || strcmp(seat->token, token) != 0) {
        return -1;
    }
    seat->status = BOOKED;
    seat->expires = 0;
    seat->token[0] = '\0';
    return 0;
}

int cancel_seat(Database *db, int trip_id, int seat_id)
{
    Trip *trip = find_trip(db, trip_id);
    Seat *seat = seatmap_get(trip, seat_id);
    if (seat == NULL || seat->status == EMPTY) {
        return -1;
    }
    int original_id = seat->id;
    memset(seat, 0, sizeof(*seat));
    seat->id = original_id;
    promote_waitlist(trip);
    return 0;
}

int update_passenger(Database *db, int trip_id, int seat_id,
                     const char *first, const char *last)
{
    Seat *seat = seatmap_get(find_trip(db, trip_id), seat_id);
    if (seat == NULL || seat->status != BOOKED ||
        !valid_name(first) || !valid_name(last)) {
        return -1;
    }
    snprintf(seat->first, sizeof(seat->first), "%s", first);
    snprintf(seat->last, sizeof(seat->last), "%s", last);
    return 0;
}

int join_waitlist(Database *db, int trip_id,
                  const char *first, const char *last)
{
    Trip *trip = find_trip(db, trip_id);
    if (trip == NULL || !valid_name(first) || !valid_name(last) ||
        trip->wait_count >= MAX_WAIT || seatmap_available(trip) > 0) {
        return -1;
    }
    Passenger *passenger = &trip->wait[trip->wait_count];
    snprintf(passenger->first, sizeof(passenger->first), "%s", first);
    snprintf(passenger->last, sizeof(passenger->last), "%s", last);
    trip->wait_count++;
    return trip->wait_count;
}
