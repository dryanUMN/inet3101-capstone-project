#include "seatmap.h"
#include <string.h>

/* Initialize the seats for a trip. */
void seatmap_init(Trip *trip, int count)
{
    trip->count = count;
    for (int i = 0; i < count; i++) {
        memset(&trip->seats[i], 0, sizeof(Seat));
        trip->seats[i].id = i + 1;
    }
}

/* Return NULL if the seat number is outside the trip's range. */
Seat *seatmap_get(Trip *trip, int seat_id)
{
    if (trip == NULL || seat_id < 1 || seat_id > trip->count) {
        return NULL;
    }
    return &trip->seats[seat_id - 1];
}

/* Count empty seats (held seats are not available). */
int seatmap_available(const Trip *trip)
{
    int available = 0;
    if (trip == NULL) {
        return 0;
    }
    for (int i = 0; i < trip->count; i++) {
        if (trip->seats[i].status == EMPTY) {
            available++;
        }
    }
    return available;
}
