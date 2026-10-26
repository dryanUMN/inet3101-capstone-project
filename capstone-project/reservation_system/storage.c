
#include "storage.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <windows.h>
#include <io.h>

/* Each trip occupies one fixed-size record after the file header.
 * fseek() can locate a trip without reading the entire database.
 * File format version 2 is NOT compatible with the old snapshot format.
 */
#define FILE_MAGIC 0x52535632u
#define FILE_VERSION 2u

typedef struct {
    uint32_t magic;
    uint32_t version;
    int count;
    uint64_t next_token;
} FileHeader;

/* The server locks before reading and unlocks after saving. */
FILE *storage_open(const char *path)
{
    FILE *file = fopen(path, "r+b");
    if (file == NULL) {
        file = fopen(path, "w+b");
    }
    return file;
}

/* Lock the same first byte for every client. Each thread opens its own stream. */
static HANDLE file_handle(FILE *file)
{
    if (file == NULL) return INVALID_HANDLE_VALUE;
    intptr_t handle = _get_osfhandle(_fileno(file));
    return handle == -1 ? INVALID_HANDLE_VALUE : (HANDLE)handle;
}

int storage_lock(FILE *file)
{
    HANDLE handle = file_handle(file);
    OVERLAPPED position = {0};
    if (handle == INVALID_HANDLE_VALUE) return -1;
    return LockFileEx(handle, LOCKFILE_EXCLUSIVE_LOCK, 0, 1, 0,
                      &position) ? 0 : -1;
}

int storage_unlock(FILE *file)
{
    HANDLE handle = file_handle(file);
    OVERLAPPED position = {0};
    if (handle == INVALID_HANDLE_VALUE) return -1;
    return UnlockFileEx(handle, 0, 1, 0, &position) ? 0 : -1;
}

/* Move to a record's position. Record numbers begin at zero. */
static int seek_trip(FILE *file, int index)
{
    long offset;
    if (file == NULL || index < 0 || index >= MAX_TRIPS) {
        return -1;
    }
    offset = (long)sizeof(FileHeader) + (long)index * (long)sizeof(Trip);
    return fseek(file, offset, SEEK_SET);
}

static int read_header(FILE *file, FileHeader *header)
{
    long size;
    if (fseek(file, 0, SEEK_END) != 0 || (size = ftell(file)) < 0) {
        return -1;
    }
    if (size == 0) {
        memset(header, 0, sizeof(*header));
        header->magic = FILE_MAGIC;
        header->version = FILE_VERSION;
        return 0;
    }
    if (size < (long)sizeof(*header) || fseek(file, 0, SEEK_SET) != 0 ||
        fread(header, sizeof(*header), 1, file) != 1) {
        return -1;
    }
    if (header->magic != FILE_MAGIC || header->version != FILE_VERSION ||
        header->count < 0 || header->count > MAX_TRIPS ||
        size != (long)sizeof(*header) +
                (long)header->count * (long)sizeof(Trip)) {
        return -1;
    }
    return 0;
}

/* Read one trip record by its zero-based index. */
int storage_read_trip(FILE *file, int index, Trip *trip)
{
    FileHeader header;
    if (trip == NULL || read_header(file, &header) != 0 ||
        index < 0 || index >= header.count || seek_trip(file, index) != 0) {
        return -1;
    }
    return fread(trip, sizeof(*trip), 1, file) == 1 ? 0 : -1;
}

/* Overwrite one existing trip record, without rewriting other trips. */
int storage_write_trip(FILE *file, int index, const Trip *trip)
{
    FileHeader header;
    if (trip == NULL || read_header(file, &header) != 0 ||
        index < 0 || index >= header.count || seek_trip(file, index) != 0) {
        return -1;
    }
    if (fwrite(trip, sizeof(*trip), 1, file) != 1 || fflush(file) != 0) {
        return -1;
    }
    return _commit(_fileno(file));
}

/* Load the small collection for existing reservation functions. */
int storage_load(FILE *file, Database *db)
{
    FileHeader header;
    if (file == NULL || db == NULL || read_header(file, &header) != 0) {
        return -1;
    }
    memset(db, 0, sizeof(*db));
    db->count = header.count;
    db->next_token = header.next_token;
    for (int i = 0; i < db->count; i++) {
        Trip *trip = &db->trips[i];
        if (seek_trip(file, i) != 0 ||
            fread(trip, sizeof(*trip), 1, file) != 1 ||
            trip->count < 1 || trip->count > MAX_SEATS ||
            trip->wait_count < 0 || trip->wait_count > MAX_WAIT) {
            return -1;
        }
    }
    return 0;
}

/* Save only changed trip records; append new records or truncate deleted ones.
 * The lock must remain held during the entire operation.
 */
int storage_save(FILE *file, const Database *db)
{
    FileHeader old_header;
    FileHeader new_header;
    Trip old_trip;

    if (file == NULL || db == NULL || db->count < 0 ||
        db->count > MAX_TRIPS || read_header(file, &old_header) != 0) {
        return -1;
    }

    for (int i = 0; i < db->count; i++) {
        int changed = i >= old_header.count;
        if (!changed) {
            if (seek_trip(file, i) != 0 ||
                fread(&old_trip, sizeof(old_trip), 1, file) != 1) {
                return -1;
            }
            changed = memcmp(&old_trip, &db->trips[i], sizeof(Trip)) != 0;
        }
        if (changed) {
            if (seek_trip(file, i) != 0 ||
                fwrite(&db->trips[i], sizeof(Trip), 1, file) != 1) {
                return -1;
            }
        }
    }

    new_header.magic = FILE_MAGIC;
    new_header.version = FILE_VERSION;
    new_header.count = db->count;
    new_header.next_token = db->next_token;
    if (fseek(file, 0, SEEK_SET) != 0 ||
        fwrite(&new_header, sizeof(new_header), 1, file) != 1 ||
        fflush(file) != 0) {
        return -1;
    }
    if (_chsize_s(_fileno(file), (long long)sizeof(new_header) +
                  (long long)db->count * (long long)sizeof(Trip)) != 0) {
        return -1;
    }
    return _commit(_fileno(file));
}
