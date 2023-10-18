#include <iostream>
#include <fstream>
#include <string>
#include <map>
#include <stdio.h>
#include <stdint.h>
#include <algorithm>
#include <random>
#include <vector>
using namespace std;

string filePath = "programming_4_0.dat";
string writeFilePath = "";
bool fullExtract = false;
string extractPath = "extracted/";
const int blockSize = 2048;
const int blockCount = 256;
const int FATblocks = 3;    // Number of blocks used in the FAT
bool spaceInFAT = true;

fstream file (filePath, ios::binary | ios::in | ios::out);
int emptyBlocks = 0;
int emptyBlockIDs[blockCount];
bool blockUsageMap[blockCount];

// Helper function for jumping to a specific block (because c++ lets you move the point you read from to anywhere)
// In a language where you can't do this (python), I'd probably first extract each block to its own temporary file at the very start, so they can be accessed quickly while not having to be kept in memory
// Though given that the entire system takes up 0.5 MB, memory isn't really a concern
void jumpToBlock(int blockNum) {
    file.seekg (blockNum * blockSize, ios::beg);
    file.seekp (blockNum * blockSize, ios::beg);
}

// Struct for holding the data for a specific file
struct fileData {
    // Extra character for terminating null
    char name[9];
    int length;
    int numBlocks;
    int blockIDs[blockCount];
};

map<string,fileData> files;
vector<fileData> filesArr;
streampos FATend;

// Convert the char array given by the read function to an int32
uint32_t charArrToInt32(char charArr[]) {
    uint32_t finalVal = 0;
    for (int i = 0; i < 4; i++) {
        // Create a dummy int and do a binary OR to avoid strangeness from type conversions
        // The cast to uint8 makes sure that only 1 byte of data is kept
        uint32_t temp = 0;
        temp = temp | (uint8_t) charArr[i];
        // Bit shift the result and then OR the new bits into the main number. I could probably just add them though.
        temp = temp << i*8;
        finalVal = finalVal | temp;
    }
    return finalVal;
}

// Extracts a file from the system
void copyExternally(fileData fileInfo) {
    int curBlock = 0;
    streampos size;
    char *memblock;
    int bytesLeft = fileInfo.length;

    // Opens a file to write into, creates if it doesn't exist
    FILE* newFile = fopen((extractPath + fileInfo.name).c_str(),"wb");

    for (int i = 0; i < fileInfo.numBlocks; i++) {
        curBlock = fileInfo.blockIDs[i];
        jumpToBlock(curBlock);

        // The c++ file reading function I'm using reads the bytes into a char array, so it has to be initialized to the correct size (I could probably resuse it)
        // Only read until the end of the file instead of the full block
        size = min(blockSize,bytesLeft);
        bytesLeft -= (int) size;

        memblock = new char [size];
        file.read (memblock, size);
        // Write the chars directly into the destination file
        fwrite(memblock,size,1,newFile);
    }
}

// Same as above but print the characters to console. Great for text files but terrible for images
void readFile(fileData fileInfo) {
    int curBlock = 0;
    streampos size;
    char *memblock;
    int bytesLeft = fileInfo.length;

    for (int i = 0; i < fileInfo.numBlocks; i++) {
        curBlock = fileInfo.blockIDs[i];
        jumpToBlock(curBlock);
        // Only read until the end of the file instead of the full block
        size = min(blockSize,bytesLeft);
        bytesLeft -= (int) size;

        memblock = new char [size];
        file.read (memblock, size);
        cout << memblock;
    }
    cout << endl;
}

int main () {
    streampos size;
    char* memblock;
    
    // Get block bitmap
    size = blockCount/8;
    memblock = new char [size];
    file.read (memblock, size);
    cout << "block usage:" << endl;

    // Go over only the bytes that will contain data (the first 32 bytes)
    for (int i = 0; i < size; i++) {
        uint8_t curChar = memblock[i];
        // Loop over each bit in the byte
        for (int j = 0; j < 8; j++) {
            // AND the byte with a bit shifted int to isolate the byte, then shift back
            uint8_t offs = (uint8_t) 1 << j;
            blockUsageMap[i*8+j] = (((curChar & offs) >> j) == 1);
            // For displaying the block usage map in the command line
            string displayChar = ((curChar & offs) >> j) == 1 ?  "▓" : "░";

            // Store locations of empty blocks for later
            emptyBlockIDs[emptyBlocks] = i*8+j;
            emptyBlocks += ((curChar & offs) >> j) == 1 ? 0 : 1;
            cout << displayChar;
        }
        // makes sure the displayed block is 16 by 16, so it looks nicer on the command line
        cout << (i % 2 == 1 ? "\n" : "");
    }

    // Get file info from FAT
    jumpToBlock(1);

    int i = 0;    
    streampos FATstart = file.tellg();
    // Each file should take a minimum of 1 block, so there should be a maximum of blockCount files in the system (not counting bitmap and FAT)
    while (i < blockCount) {
        fileData curFile;

        size = 8;
        memblock = new char [size];
        file.read (memblock, size);
        // End of FAT
        if (memblock[0] == '\0') {
            // Record the end of the FAT, in case a file needs to be written later
            FATend = (int) file.tellg() - 8;
            break;
        }
        // If the end of the dedicated FAT blocks is reached 
        // Probably impossible, given that there are a max of 252 files (with one block each), and each would take up 14 bytes in the FAT (8 for name, 4 for size and 2 for block list including terminator)
        // Totalling to 3584 bytes (just over half the dedicated space)
        if (file.tellg() - FATstart >= FATblocks * blockSize) {
            cout << "The FAT is full, likely meaning that the system has become corrupted" << endl;
            break;
        }

        // First 8 bytes are the name, and are presumably printable characters
        strcpy(&curFile.name[0],memblock);
        curFile.name[8] = '\0';
        cout << curFile.name << ":" << endl;

        // Next 4 bytes are the file length. I should probably make it stop reading after it passes this amount.
        size = 4;
        memblock = new char [size];
        file.read (memblock, size);
        curFile.length = charArrToInt32(memblock);
        cout << curFile.length << " bytes" << endl;

        // Read block list for file, continue untill terminating character or max possible blocks used
        int j = 0;
        while (j < blockCount) {
            size = 1;
            memblock = new char [size];
            file.read (memblock, size);
            // Convoluted type casting to avoid negative numbers (issue with two's complement? If the first byte is ~240, then it gets converted to -16 without casting to uint8 first)
            int num = (int) (uint8_t) memblock[0];
            // Reached end of block list
            if (memblock[0] == '\0') {
                curFile.numBlocks = j;
                cout << j << " block" << (j != 1 ? "s" : "") << " (" << j*blockSize << " bytes)" << endl;
                break;
            }
            curFile.blockIDs[j] = num;
            j++;
        }
        files[curFile.name] = curFile;
        i++;
        cout << endl;
    }

    string instruction;
    cout << "Choose an action:" << endl;
    cout << "extract all (E)" << endl << "quit (Q)" << endl << "write file (W)" << endl;

    cin >> instruction;
    fullExtract = instruction == "E" ? true : false;

    // If the filesystem should be fully extracted
    if (fullExtract == true) {
        // Taken from the c++ documentation (https://en.cppreference.com/w/cpp/container/map/begin). Uses an iterator to get map elements by index
        for (auto it = files.begin(); it != files.end(); ++it) {
            copyExternally(it->second);
        }
        cout << "extracted to /" << extractPath << endl;
    }

    if (instruction == "W") {
        cout << "Enter the path to the file you want to write, relative to the running directory. Include file extension (if any)" << endl;
        cin >> writeFilePath;

        ifstream newFile (writeFilePath, ios::binary);

        // Get file size
        streampos fileBegin, fileEnd;
        fileBegin = newFile.tellg();
        newFile.seekg (0, ios::end);
        fileEnd = newFile.tellg();
        int newFileSize = (int) (fileEnd - fileBegin);
        cout << newFileSize << " bytes" << endl;
        int blocksNeeded = (int) ceil((float) newFileSize / (float) blockSize);
        cout << blocksNeeded << " blocks needed" << ", ";
        cout << emptyBlocks << " blocks available" << endl;

        if (emptyBlocks < blocksNeeded) {
            cout << "Insufficient space for file" << endl;
        } else if (!spaceInFAT) {
            cout << "Insufficient space in FAT" << endl;
        } else {
            // Determine blocks to overwrite
            vector<int> newBlockIDs;
            for (int i = 0; i < blocksNeeded; i++) {
                newBlockIDs.push_back(emptyBlockIDs[i]);
            }
            // Shuffle order, so the file can't just be read by opening in a text editor
            shuffle(std::begin(newBlockIDs), std::end(newBlockIDs), mt19937{random_device{}()});

            // Get file name from path
            // Figure out where the directories end and the file name begins
            int directoriesEnd = 0;
            directoriesEnd = writeFilePath.find_last_of('/');
            string fileName = writeFilePath.substr(directoriesEnd+1, writeFilePath.length());
            while (fileName.length() > 8) {
                cout << "File name is too long, please enter a shorter name (8 characters or less)" << endl;
                cin >> fileName;
            }

            // 8 bytes for name, 4 for size, 1 for each block and 1 extra for the terminator
            char fatEntry[8 + 4 + blocksNeeded + 1];

            // Padded file name
            for (int i = 0; i < 8; i++) {
                fatEntry[i] = i >= fileName.length() ? '\0' : fileName[i];
            }

            // File size
            uint32_t fSize = (uint32_t) newFileSize;
            char fSizeBytes[4];

            // Directly copy memory from the int to the bytes
            memcpy(fSizeBytes, &fSize, 4);
            for (int i = 0; i < 4; i++) {
                fatEntry[i+8] = fSizeBytes[i];
            }
            
            // Needed blocks
            for (int i = 0; i < blocksNeeded; i++){
                fatEntry[i+8+4] = (uint8_t) newBlockIDs[i];
            }
            // Ending null
            fatEntry[8 + 4 + blocksNeeded] = '\0';

            file.seekp(FATend);
            file.write(fatEntry,8 + 4 + blocksNeeded);

            // Block bitmap
            for (int id : newBlockIDs) {
                blockUsageMap[id] = true;
            }
            
            // Turn the edited array into bytes
            char *newBitmapBytes = new char[blockCount/8];
            for (int i = 0; i < blockCount/8; i++) {
                uint8_t nextChar = 0;
                for (int j = 0; j < 8; j++) {
                    // OR the byte with a bit shifted int
                    uint8_t newbit = (uint8_t) (blockUsageMap[i*8+j] ? 1 : 0) << j;
                    nextChar = nextChar | newbit;

                    //emptyBlockIDs[emptyBlocks] = i*8+j;
                    //emptyBlocks += ((curChar & offs) >> j) == 1 ? 0 : 1;
                }
                newBitmapBytes[i] = nextChar;
            }

            // Write the new bitmap
            jumpToBlock(0);
            file.write(newBitmapBytes,blockCount/8);

            // Write in the blocks
            int bytesLeft = newFileSize;
            newFile.seekg(0,ios::beg);
            for (int i = 0; i < blocksNeeded; i++) {
                jumpToBlock(newBlockIDs[i]);

                size = min(blockSize,bytesLeft);
                bytesLeft -= (int) size;

                memblock = new char [size];
                newFile.read (memblock, size);
                // Write the chars directly into the destination file
                file.write(memblock,size);
            }
        }

        newFile.close();
    }

    file.close();
}

char* getBitmap() {
    string output = "";
    streampos size;
    char *memblock;
    emptyBlocks = 0;

    // Get block bitmap
    jumpToBlock(0);
    size = blockCount/8;
    memblock = new char [size];
    file.read (memblock, size);
    cout << "block usage:" << endl;

    // Go over only the bytes that will contain data (the first 32 bytes)
    for (int i = 0; i < size; i++) {
        uint8_t curChar = memblock[i];
        // Loop over each bit in the byte
        for (int j = 0; j < 8; j++) {
            // AND the byte with a bit shifted int to isolate the byte, then shift back
            uint8_t offs = (uint8_t) 1 << j;
            blockUsageMap[i*8+j] = (((curChar & offs) >> j) == 1);
            // For displaying the block usage map in the command line
            string displayChar = ((curChar & offs) >> j) == 1 ?  "#" : "-";

            // Store locations of empty blocks for later
            emptyBlockIDs[emptyBlocks] = i*8+j;
            emptyBlocks += ((curChar & offs) >> j) == 1 ? 0 : 1;
            output += displayChar;
        }
        // makes sure the displayed block is 16 by 16, so it looks nicer on the command line
        output += (i % 2 == 1 ? "\n" : "");
    }
    char* returnStr = new char[output.size()];
    
    strcpy(returnStr, output.c_str());
    return returnStr;
}

fileData* readFat() {
    filesArr.clear();
    filesArr.reserve(blockCount);
    streampos size;
    char* memblock;
    // Get file info from FAT
    jumpToBlock(1);

    int i = 0;    
    streampos FATstart = file.tellg();
    // Each file should take a minimum of 1 block, so there should be a maximum of blockCount files in the system (not counting bitmap and FAT)
    while (i < blockCount) {
        fileData curFile;

        size = 8;
        memblock = new char [size];
        file.read (memblock, size);
        // End of FAT
        if (memblock[0] == '\0') {
            // Record the end of the FAT, in case a file needs to be written later
            FATend = (int) file.tellg() - 8;
            break;
        }
        // If the end of the dedicated FAT blocks is reached 
        // Probably impossible, given that there are a max of 252 files (with one block each), and each would take up 14 bytes in the FAT (8 for name, 4 for size and 2 for block list including terminator)
        // Totalling to 3584 bytes (just over half the dedicated space)
        if (file.tellg() - FATstart >= FATblocks * blockSize) {
            cout << "The FAT is full, likely meaning that the system has become corrupted" << endl;
            break;
        }

        // First 8 bytes are the name, and are presumably printable characters
        strcpy(&curFile.name[0],memblock);
        curFile.name[8] = '\0';
        //cout << curFile.name << ":" << endl;

        // Next 4 bytes are the file length. I should probably make it stop reading after it passes this amount.
        size = 4;
        memblock = new char [size];
        file.read (memblock, size);
        curFile.length = charArrToInt32(memblock);
        //cout << curFile.length << " bytes" << endl;

        // Read block list for file, continue untill terminating character or max possible blocks used
        int j = 0;
        while (j < blockCount) {
            size = 1;
            memblock = new char [size];
            file.read (memblock, size);
            // Convoluted type casting to avoid negative numbers (issue with two's complement? If the first byte is ~240, then it gets converted to -16 without casting to uint8 first)
            int num = (int) (uint8_t) memblock[0];
            // Reached end of block list
            if (memblock[0] == '\0') {
                curFile.numBlocks = j;
                //cout << j << " block" << (j != 1 ? "s" : "") << " (" << j*blockSize << " bytes)" << endl;
                break;
            }
            curFile.blockIDs[j] = num;
            j++;
        }
        files[curFile.name] = curFile;
        filesArr.push_back(curFile);
        i++;
        //cout << endl;
    }
    return &filesArr[0];
}

char* returnFileBytes(fileData fileInfo) {
    int curBlock = 0;
    streampos size;
    int bytesLeft = fileInfo.length;
    char* fullMem = new char[fileInfo.length];
    char* currentMem = fullMem;

    for (int i = 0; i < fileInfo.numBlocks; i++) {
        curBlock = fileInfo.blockIDs[i];
        jumpToBlock(curBlock);
        // Only read until the end of the file instead of the full block
        size = min(blockSize,bytesLeft);
        bytesLeft -= (int) size;

        file.read (currentMem, size);
        currentMem += size;
        //cout << memblock;
        //memcpy(fullMem+i*blockSize, );
    }
    //cout << endl;
    return fullMem;
}

void closeFileBytes(char* memLoc) {
    free(memLoc);
}

void writeFile(char* filePath, char* shortenedName) {
    streampos size;
    char* memblock;
    writeFilePath = string(filePath);

    ifstream newFile (writeFilePath, ios::binary);

    // Get file size
    streampos fileBegin, fileEnd;
    fileBegin = newFile.tellg();
    newFile.seekg (0, ios::end);
    fileEnd = newFile.tellg();
    int newFileSize = (int) (fileEnd - fileBegin);
    cout << newFileSize << " bytes" << endl;
    int blocksNeeded = (int) ceil((float) newFileSize / (float) blockSize);
    cout << blocksNeeded << " blocks needed" << ", ";
    cout << emptyBlocks << " blocks available" << endl;

    if (emptyBlocks < blocksNeeded) {
        cout << "Insufficient space for file" << endl;
    } else if (!spaceInFAT) {
        cout << "Insufficient space in FAT" << endl;
    } else {
        // Determine blocks to overwrite
        vector<int> newBlockIDs;
        for (int i = 0; i < blocksNeeded; i++) {
            newBlockIDs.push_back(emptyBlockIDs[i]);
        }
        // Shuffle order, so the file can't just be read by opening in a text editor
        shuffle(std::begin(newBlockIDs), std::end(newBlockIDs), mt19937{random_device{}()});

        // Get file name from path
        // Figure out where the directories end and the file name begins
        int directoriesEnd = 0;
        directoriesEnd = writeFilePath.find_last_of('/');
        string fileName = string(shortenedName, 8);
        cout << fileName << endl;

        // 8 bytes for name, 4 for size, 1 for each block and 1 extra for the terminator
        char fatEntry[8 + 4 + blocksNeeded + 1];

        // Padded file name
        for (int i = 0; i < 8; i++) {
            fatEntry[i] = i >= fileName.length() ? '\0' : fileName[i];
        }

        // File size
        uint32_t fSize = (uint32_t) newFileSize;
        char fSizeBytes[4];

        // Directly copy memory from the int to the bytes
        memcpy(fSizeBytes, &fSize, 4);
        for (int i = 0; i < 4; i++) {
            fatEntry[i+8] = fSizeBytes[i];
        }
        
        // Needed blocks
        for (int i = 0; i < blocksNeeded; i++){
            fatEntry[i+8+4] = (uint8_t) newBlockIDs[i];
        }
        // Ending null
        fatEntry[12 + blocksNeeded] = '\0';

        file.seekp(FATend);
        file.write(fatEntry,12 + blocksNeeded);

        cout << blockUsageMap[0] << endl;
        // Block bitmap
        for (int id : newBlockIDs) {
            blockUsageMap[id] = true;
        }
        
        // Turn the edited array into bytes
        char *newBitmapBytes = new char[blockCount/8];
        for (int i = 0; i < blockCount/8; i++) {
            uint8_t nextChar = 0;
            for (int j = 0; j < 8; j++) {
                // OR the byte with a bit shifted int
                uint8_t newbit = (uint8_t) (blockUsageMap[i*8+j] ? 1 : 0) << j;
                nextChar = nextChar | newbit;

                //emptyBlockIDs[emptyBlocks] = i*8+j;
                //emptyBlocks += ((curChar & offs) >> j) == 1 ? 0 : 1;
            }
            newBitmapBytes[i] = nextChar;
        }

        // Write the new bitmap
        jumpToBlock(0);
        file.write(newBitmapBytes,blockCount/8);

        // Write in the blocks
        int bytesLeft = newFileSize;
        newFile.seekg(0,ios::beg);
        for (int i = 0; i < blocksNeeded; i++) {
            jumpToBlock(newBlockIDs[i]);

            size = min(blockSize,bytesLeft);
            bytesLeft -= (int) size;

            memblock = new char [size];
            newFile.read (memblock, size);
            // Write the chars directly into the destination file
            file.write(memblock,size);
        }
    }

    newFile.close();
    file.flush();
}

extern "C" {
    char* getBlockUsageMap() {
        return getBitmap();
    }

    fileData* readFATEntries() {
        return readFat();
    }
    
    void extractFile(fileData f) {
        copyExternally(f);
    }
    char* getBytes(fileData f) {
        return returnFileBytes(f);
    }
    void freeBytes(char* loc) {
        closeFileBytes(loc);
    }
    void newFile(char* filePath, char* shortenedName) {
        //cout << filePath << "~~~~" << endl;
        writeFile(filePath, shortenedName);
    }
}