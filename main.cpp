#include "fs.h"
#include <iostream>
using namespace std;

int main() {
    string diskName = "disk.img";
    string fileName = "test1.txt";
    string fileData = "This is a sample line written to test1.txt using the custom file system.";

    // Recover any uncommitted operations from previous crash
    recoverJournal(diskName);

    // Step 1: Create a file
    if (createFile(diskName, fileName)) {
        cout << "File created successfully.\n";
    } else {
        cout << "File creation failed.\n";
    }

    // Step 2: Write to the file
    if (writeFile(diskName, fileName, fileData)) {
        cout << "Data written successfully.\n";
    } else {
        cout << "Data write failed.\n";
    }

    // Step 3: Read from the file
    string content = readFile(diskName, fileName);
    if (!content.empty()) {
        cout << "File content read successfully:\n" << content << endl;
    } else {
        cout << "Failed to read file or file is empty.\n";
    }

    // Step 4: Delete the file
    if (deleteFile(diskName, fileName)) {
        cout << "File deleted successfully.\n";
    } else {
        cout << "File deletion failed.\n";
    }

    return 0;
}

