#include "fs.h"
#include <fstream>
#include <iostream>
#include <cstring>

using namespace std;
bool createFile(const string &diskname, const string &filename) {
    fstream disk(diskname, ios::in | ios::out | ios::binary);
    if (!disk) {
        cerr << "Error: Cannot open disk file.\n";
        return false;
    }

    // Skip SuperBlock
    disk.seekg(sizeof(SuperBlock), ios::beg);

    Inode inode;
    for (int i = 0; i < MAX_FILES; ++i) {
        streampos pos = disk.tellg();
        disk.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

        // If inode is not used, allocate it
        if (inode.used == 0) {
            strncpy(inode.filename, filename.c_str(), FILENAME_LEN);
            inode.used = 1;
            inode.size = 0;
            memset(inode.block_pointers, -1, sizeof(inode.block_pointers));

            // Move write pointer back and write updated inode
            disk.seekp(pos);
            disk.write(reinterpret_cast<char*>(&inode), sizeof(Inode));

            disk.close();
            cout << "File created: " << filename << endl;
            return true;
        }
    }

    cerr << "Error: No space left in inode table.\n";
    disk.close();
    return false;
}


bool writeFile(const string &diskname, const string &filename, const string &data) {
    fstream disk(diskname, ios::in | ios::out | ios::binary);
    if (!disk) {
        cerr << "Error: Cannot open disk file.\n";
        return false;
    }

    SuperBlock sb;
    disk.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // Step 1: Find the inode
    Inode inode;
    streampos inode_start = disk.tellg();
    bool found = false;
    int inode_index = -1;

    for (int i = 0; i < MAX_FILES; ++i) {
        disk.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
        if (inode.used && filename == inode.filename) {
            found = true;
            inode_index = i;
            break;
        }
    }

    if (!found) {
        cerr << "Error: File not found.\n";
        disk.close();
        return false;
    }

    // Step 2: Write data into free blocks
    int blocks_needed = (data.size() + BLOCK_SIZE - 1) / BLOCK_SIZE;
    if (blocks_needed > 10) {
        cerr << "Error: File too large.\n";
        disk.close();
        return false;
    }

    int block_no = sb.data_start;
    int written_blocks = 0;
    string chunk;

    for (int i = 0; i < MAX_BLOCKS - sb.data_start; ++i) {
        streampos pos = sizeof(SuperBlock) + sizeof(Inode) * MAX_FILES + (block_no * BLOCK_SIZE);
        disk.seekg(pos);
        char test[BLOCK_SIZE];
        disk.read(test, BLOCK_SIZE);

        bool is_free = true;
        for (int j = 0; j < BLOCK_SIZE; j++) {
            if (test[j] != 0) {
                is_free = false;
                break;
            }
        }

        if (is_free) {
            // Take a chunk of data
            chunk = data.substr(written_blocks * BLOCK_SIZE, BLOCK_SIZE);

            // Log the write in journal
            streampos journalPos = logJournalEntry(disk, 1, filename, block_no, chunk);

            //  Write the actual data block
            disk.seekp(pos);
            disk.write(chunk.c_str(), chunk.size());

            // Update inode
            inode.block_pointers[written_blocks] = block_no;
            written_blocks++;

            //  Mark journal as committed
            if (journalPos != -1) {
                JournalEntry entry;
                disk.seekg(journalPos);
                disk.read(reinterpret_cast<char*>(&entry), sizeof(JournalEntry));
                entry.committed = 1;
                disk.seekp(journalPos);
                disk.write(reinterpret_cast<char*>(&entry), sizeof(JournalEntry));
            }

            if (written_blocks == blocks_needed)
                break;
        }

        block_no++;
    }

    if (written_blocks < blocks_needed) {
        cerr << "Error: Not enough space.\n";
        disk.close();
        return false;
    }

    inode.size = data.size();

    // Step 3: Update inode
    streampos inode_pos = sizeof(SuperBlock) + sizeof(Inode) * inode_index;
    disk.seekp(inode_pos);
    disk.write(reinterpret_cast<char*>(&inode), sizeof(Inode));

    disk.close();
    cout << "Data written to file: " << filename << endl;
    return true;
}



string readFile(const string &diskname, const string &filename) {
    fstream disk(diskname, ios::in | ios::binary);
    if (!disk) {
        cerr << "Error: Cannot open disk file.\n";
        return "";
    }

    SuperBlock sb;
    disk.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    // Search for the file in the inode table
    Inode inode;
    for (int i = 0; i < MAX_FILES; ++i) {
        disk.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
        if (inode.used && filename == inode.filename) {
            string result = "";
            char buffer[BLOCK_SIZE];

            for (int j = 0; j < 10 && inode.block_pointers[j] != -1; ++j) {
                streampos pos = sizeof(SuperBlock) + sizeof(Inode) * MAX_FILES + inode.block_pointers[j] * BLOCK_SIZE;
                disk.seekg(pos);
                disk.read(buffer, BLOCK_SIZE);
                result.append(buffer, BLOCK_SIZE);
            }

            disk.close();
            return result.substr(0, inode.size); // Trim padding
        }
    }

    disk.close();
    cerr << "Error: File not found.\n";
    return "";
}


bool deleteFile(const string &diskname, const string &filename) {
    fstream disk(diskname, ios::in | ios::out | ios::binary);
    if (!disk) {
        cerr << "Error: Cannot open disk file.\n";
        return false;
    }

    SuperBlock sb;
    disk.read(reinterpret_cast<char*>(&sb), sizeof(SuperBlock));

    Inode inode;
    for (int i = 0; i < MAX_FILES; ++i) {
        streampos inode_pos = disk.tellg();
        disk.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

        if (inode.used && filename == inode.filename) {
            char buffer[BLOCK_SIZE];

            //  Log each block before deletion
            for (int j = 0; j < 10 && inode.block_pointers[j] != -1; ++j) {
                int block_no = inode.block_pointers[j];
                streampos block_pos = sizeof(SuperBlock) + sizeof(Inode) * MAX_FILES + block_no * BLOCK_SIZE;

                // Read existing data
                disk.seekg(block_pos);
                disk.read(buffer, BLOCK_SIZE);

                // Log the delete intent
                string data_str(buffer, BLOCK_SIZE);
                streampos journalPos = logJournalEntry(disk, 2, filename, block_no, data_str);

                // Clear block
                disk.seekp(block_pos);
                char zero_block[BLOCK_SIZE] = {0};
                disk.write(zero_block, BLOCK_SIZE);

                //  Mark journal as committed after clearing
                if (journalPos != -1) {
                    JournalEntry entry;
                    disk.seekg(journalPos);
                    disk.read(reinterpret_cast<char*>(&entry), sizeof(JournalEntry));
                    entry.committed = 1;
                    disk.seekp(journalPos);
                    disk.write(reinterpret_cast<char*>(&entry), sizeof(JournalEntry));
                }
            }

            // Clear inode
            memset(&inode, 0, sizeof(Inode));
            disk.seekp(inode_pos);
            disk.write(reinterpret_cast<char*>(&inode), sizeof(Inode));

            disk.close();
            cout << "File deleted: " << filename << endl;
            return true;
        }
    }

    disk.close();
    cerr << "Error: File not found.\n";
    return false;
}




streampos logJournalEntry(fstream &disk, int op_type, const string &filename, int block_no, const string &data) {
    JournalEntry entry;
    strncpy(entry.filename, filename.c_str(), FILENAME_LEN);
    entry.block_no = block_no;
    strncpy(entry.data, data.c_str(), BLOCK_SIZE);
    entry.op_type = op_type;
    entry.committed = 0;

    streampos journal_offset = sizeof(SuperBlock) + sizeof(Inode) * MAX_FILES + 1000 * BLOCK_SIZE;
    JournalEntry temp;

    for (int i = 0; i < 24; ++i) {
        streampos pos = journal_offset + static_cast<streamoff>(i * sizeof(JournalEntry));
        disk.seekg(pos);
        disk.read(reinterpret_cast<char*>(&temp), sizeof(JournalEntry));

        if (temp.committed == 1 || temp.op_type == -1) {
            disk.seekp(pos);
            disk.write(reinterpret_cast<char*>(&entry), sizeof(JournalEntry));
            return pos; //  Return the position
        }
    }

    return -1; // If journal full or error
}


void recoverJournal(const string &diskname) {
    fstream disk(diskname, ios::in | ios::out | ios::binary);
    if (!disk) return;

    streampos journal_offset = sizeof(SuperBlock) + sizeof(Inode) * MAX_FILES + 1000 * BLOCK_SIZE;

    for (int i = 0; i < 24; ++i) {
        streampos pos = journal_offset + static_cast<streamoff>(i * sizeof(JournalEntry));
        disk.seekg(pos);

        JournalEntry entry;
        disk.read(reinterpret_cast<char*>(&entry), sizeof(JournalEntry));

        if (entry.op_type != -1 && entry.committed == 0) {
            cout << "[Recovery] Incomplete operation on file: " << entry.filename << endl;

            // Restore data for write or delete
            if (entry.op_type == 1 || entry.op_type == 2) {
                streampos block_pos = sizeof(SuperBlock) + sizeof(Inode) * MAX_FILES + entry.block_no * BLOCK_SIZE;
                disk.seekp(block_pos);
                disk.write(entry.data, BLOCK_SIZE);
            }

            // Mark the journal as committed after recovery
            entry.committed = 1;
            disk.seekp(pos);
            disk.write(reinterpret_cast<char*>(&entry), sizeof(JournalEntry));
        }
    }

    disk.close();
}



void init_fs(const string &diskname) {
    ofstream disk(diskname, ios::binary | ios::out);
    if (!disk) {
        cerr << "Error: Cannot create disk file.\n";
        return;
    }

    // Write SuperBlock
    SuperBlock sb;
    disk.write(reinterpret_cast<char*>(&sb), sizeof(sb));

    // Write empty Inode Table
    Inode inodes[MAX_FILES] = {};
    disk.write(reinterpret_cast<char*>(&inodes), sizeof(inodes));

    // Fill remaining space with zero blocks
    char zero_block[BLOCK_SIZE] = {};
    int used_blocks = 1 + (sizeof(inodes) / BLOCK_SIZE); // SuperBlock + Inodes
    int remaining_blocks = MAX_BLOCKS - used_blocks;

    for (int i = 0; i < remaining_blocks; ++i) {
        disk.write(zero_block, BLOCK_SIZE);
    }

    disk.close();
    cout << "File system initialized: " << diskname << endl;
}

