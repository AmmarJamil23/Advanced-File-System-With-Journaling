# Advanced-File-System-With-Journaling
Advanced File System with Journaling
This project implements a basic file system with journaling support in C++. It simulates core file system functionalities 
such as creating, writing, reading, and deleting files on a virtual disk (disk.img). It also includes a journaling mechanism
for crash recovery, ensuring consistency and durability.

Features
Virtual disk storage (disk.img)
SuperBlock and Inode-based file metadata management
Support for up to 100 files and 1024 blocks
Fixed-size data blocks (512 bytes)
Journaling for crash recovery:
Logs write and delete operations before actual execution
Supports automatic recovery of incomplete operations

Functions for:
Creating files
Writing to files
Reading from files
Deleting files
Recovering from crashes via the journal

File Structure
fs.h: Header file defining constants, data structures (SuperBlock, Inode, JournalEntry), and function declarations.
fs.cpp: Contains the core logic for file system operations and journaling.
main.cpp: Demonstrates usage of the file system APIs.
disk.img: Simulated binary disk image file.

How It Works
File System Initialization (init_fs)
Initializes the disk image with:
SuperBlock
Empty Inode Table
Zeroed data blocks
Reserved journal area

File Operations

Create: Scans Inode table for a free slot and assigns the filename.
Write: Splits the data into 512-byte blocks.Finds free data blocks. 
Logs the write in the journal.Writes data and marks the journal as committed.
Read: Reconstructs the data from block pointers in the Inode.
Delete:Logs the content of each block before clearing. Clears data blocks and the corresponding Inode.

Journaling and Recovery
Each operation (write or delete) is logged before execution.
The journal tracks op_type, filename, block number, data, and commit status.
On startup, recoverJournal scans for uncommitted entries and completes them before continuing.

Compilation
To compile the project:

g++ fs.cpp main.cpp -o myfs

Running the File System
Initialize the file system:

init_fs("disk.img");
Run operations using the executable:

./myfs
The main.cpp file demonstrates:

Recovery from crashes

Example Output
arduino
Copy
Edit
[Recovery] Incomplete operation on file: test1.txt
File created successfully.
Data written successfully.
File content read successfully:
This is a sample line written to test1.txt using the custom file system.
File deleted successfully.
Limitations
File size is limited to 10 blocks (5 KB per file max).

