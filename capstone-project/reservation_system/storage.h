#ifndef STORAGE_H
#define STORAGE_H

#include <stdio.h>
#include "reservation.h"

/* Module 4: standard C FILE streams, fread(), fwrite(), and fseek(). */
FILE *storage_open(const char *path);
int storage_load(FILE *file, Database *db);
int storage_save(FILE *file, const Database *db);

/* Read or update one existing trip record without loading all trips.
 * Indices start at zero; caller must hold the database lock.
 */
int storage_read_trip(FILE *file, int index, Trip *trip);
int storage_write_trip(FILE *file, int index, const Trip *trip);

/* Module 6: protect the read/change/write transaction across clients. */
int storage_lock(FILE *file);
int storage_unlock(FILE *file);

#endif
