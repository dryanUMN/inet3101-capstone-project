# INET 3101 Capstone Project
Source Code for INET3101 Capstone Project

## Program Description
I chose the first Option, the Air / Train / Bus Reservation System because I felt like it was a good opportunity to build upon previous assignments learnings. This program is an extension of previously created seat management applications, which lets passengers reserve seats on different trips while keeping seat availability and passenger information organized, with the added features of broader Trip management, and use of a network server to remotely access binary database file to store trip data between clients. 

# Design & Architecture Details
## Program organization
- client.c — accepts user commands and exchanges messages with the server over TCP.
- server.c — accepts client connections, interprets commands, calls reservation functions, and manages database access.
- reservation.h and reservation.c — define the reservation data structure and implement trip creation, seat holds, confirmations, cancellations, passenger updates, hold expiration, and waitlists, the bulk of the functions that the User interacts with.
- seatmap.h and seatmap.c — provide the seat-map operations used to initialize seats, look up seats, and count available seats. Taken and adjusted from previous assignment
- storage.h and storage.c — manage the persistent database file and record-level access functions.
- Makefile — builds server.exe and client.exe with MSYS2 MinGW-w64 GCC. (The outlined system programming used Linux examples, however both my personal machines run Windows 10 and 11 respectively, so I did use AI to enable it to compile on Windows)

## Data structures and ADTs
The program uses C struct types to represent its data:
- Seat stores a seat number, status (EMPTY, HELD, or BOOKED), passenger first and last names, hold expiration time, and confirmation token.
- Passenger stores a first and last name for a waiting-list entry.
- Trip stores a trip ID, transportation mode, label, seat count, fixed-size array of seats, and fixed-size waitlist array.
- Database stores the collection of trips and the next token counter.
The project supports up to 32 trips, 64 seats per trip, and 32 waitlist entries per trip. The seat-map functions form an ADT: the server and reservation functions can work with seats through defined functions instead of repeating seat-management code. Reservation operations are kept separate from networking and file storage. 

## Database persistence strategy
The system uses a binary file of independently accessible, fixed-size trip records, rather than a SQL database or a single saved Database structure. (I did end up struggling around with singe saved Database structure method for a while, and eventually looked online + with AI and pivoted to binary file database instead after I finally got it to work.) The file begins with a small header containing a format identifier, format version, trip count, and token counter. Each subsequent record contains one complete Trip, including its seats and waitlist.

storage.c uses standard C file I/O functions such as fopen(), fread(), fwrite(), fseek(), fflush(), and fclose(). Because each trip record has the same size, fseek() can move directly to a particular record by its index. The interface has functions storage_read_trip() and storage_write_trip() for accessing one trip at a time.

## Socket networking protocol
The application uses TCP to communicate between the client and server on the local computer. On Windows, networking is implemented with Winsock2. (Which again, I needed a little help from AI to adjust the program I was writing to function on my Windows System) The server initializes networking, creates a socket, binds to an address and port, listens for connections, and accepts clients. The client connects to the server and sends commands. Both programs use send() and recv() to exchange text.


# Pros & Cons of Solution
I think the programs Pros of the program are it's simple and intuitive UI and design, which allows for swift navigation and usage for the User. Additionally, the program itself is fairly organized, with each C source file having a clearly defined purpose and usage. Additionally, (after a lot of troubleshooting), the persistent Data storage system works really well between clients and connection sessions.

The cons of the program is its memory ineffeciency due to a simplified data structures that take up a set amount of memory regardless of how many seats/waitlist spots are used. This fixed seat/waitlist memory allocation also means that the storage limits are very strict in how many seats, waitlist entries, and name characters the program allows. Finally, the menu, while simple and intuitive, does run through a terminal session, so it's can be a little easy to get lost and have to scroll up. 

# AI Audit Reflection
Prompts used for program creation assistance, architectural review, and bug review: 

"Using the following attached reservation.c and reservation.h file, how can I adjust the find_trip function to work."
"Using the attached client.c and server.c files, how can I adjust the sock based networking to work on Windows 11"
"Help me make a Makefile for the following attached zip program"
"Review the following attached zip program and look for any possible oversights or improvements, with an emphasis on the chosen implementation of client-server database access and how the server processes commands"

# Video Demonstration Link
It can also be found as a File in this source code repo
https://youtu.be/MxKX9mLH3Zk
