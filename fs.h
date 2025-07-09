#pragma once
#include <string>

using namespace std;

const int BLOCK_SIZE = 512;
const int MAX_BLOCKS = 1024;
const int MAX_FILES = 100;
const int FILENAME_LEN = 16;

struct SuperBlock {
    int total_blocks = MAX_BLOCKS;
    int used_blocks = 0;
    int inode_start = 1;
    int data_start = 11;
    int journal_start = 1000;
};

struct Inode {
    char filename[FILENAME_LEN];
    int size = 0;
    int block_pointers[10] = {0};
    int used = 0;
};

struct JournalEntry {
    int op_type; // 0=create, 1=write, 2=delete
    char filename[FILENAME_LEN];
    int block_no;
    char data[BLOCK_SIZE];
    int committed; // 0 = not committed, 1 = done
};

void init_fs(const string &diskname);
bool createFile(const string &diskname, const string &filename);
bool writeFile(const string &diskname, const string &filename, const string &data);
string readFile(const string &diskname, const string &filename);
bool deleteFile(const string &diskname, const string &filename);
void recoverJournal(const string &diskname);
streampos logJournalEntry(fstream &disk, int op_type, const string &filename, int block_no, const string &data);



