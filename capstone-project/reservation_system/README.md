# Air / Train / Bus Reservation System — Windows MSYS2 GCC

A small C client/server reservation system with multiple trips, seat holds,
confirmations, cancellations, passenger updates, and waitlists.

## Compile in VS Code (MSYS2 MinGW-w64 GCC)

Open a terminal with your MSYS2 GCC available on PATH:

```sh
mingw32-make
```

Run the server:

```sh
./server.exe 5555 reservations.dat
```

In another terminal, run the client:

```sh
./client.exe 127.0.0.1 5555
```

Type `HELP` for all commands. Example:

```text
ADD AIR Morning 24
LIST
SHOW 1
HOLD 1 5 Jane Doe
CONFIRM 1 5 TOKEN_FROM_HOLD
QUIT
```

## Course concepts

- **Module 4:** `FILE *`, `fopen`, `fread`, `fwrite`, `fseek`, `fclose`,
  and a fixed-size, individually accessible `Trip` record format.
- **Module 5:** TCP sockets using Winsock2 (`socket`, `bind`, `listen`,
  `accept`, `connect`, `send`, `recv`).
- **Module 6:** Windows threads for concurrent clients and `LockFileEx`
  to protect database transactions.

The storage file begins with a small header followed by fixed-size trip
records. A trip can be read or updated by record index. For simplicity,
existing reservation logic loads the small database into memory; saving
writes only changed trip records. Seats remain part of their trip record.

This format is incompatible with the earlier Linux snapshot database.
Use a new database filename or back up the old file.

See BUILDING.md for build instructions.
