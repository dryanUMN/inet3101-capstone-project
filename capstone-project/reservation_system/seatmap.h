#ifndef SEATMAP_H
#define SEATMAP_H
#include "reservation.h"
int seatmap_available(const Trip *trip);
Seat *seatmap_get(Trip *trip,int seat_id);
void seatmap_init(Trip *trip,int count);
#endif
