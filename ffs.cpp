#include <iostream>
#include <string>
#include <sstream>
#include <fstream>
#include <vector>
#include <filesystem>
#include <cstring>
#include <algorithm>

using namespace std;
namespace fs = std::filesystem;

#define INODE_BITMAP_SIZE 4096     
#define DATA_BITMAP_SIZE 4096      
#define INODE_TABLE_SIZE (4096 * 1024)  
#define DATA_BLOCKS_SIZE (4096 * 1024)  
#define FFS_TOTAL_SIZE (INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + INODE_TABLE_SIZE + DATA_BLOCKS_SIZE)

#define BLOCK_SIZE 4096
#define MAX_FILENAME 256
#define MAX_DIRECT_PTRS 500
#define MAX_FILE_SIZE (4 * 1024 * 1024)

const string FFS_FILENAME = "ffs_data";

struct Inode {
    char filename[MAX_FILENAME];
    int filesize;
    int direct_ptrs[MAX_DIRECT_PTRS];
    int indirect_ptr;
};

void initFFS() {
    if (!fs::exists(FFS_FILENAME)) {
        ofstream f(FFS_FILENAME, ios::binary);
        vector<char> zero(FFS_TOTAL_SIZE, 0);
        f.write(zero.data(), zero.size());
        f.close();
    }
}

int findFreeIndex(vector<char>& bitmap) {
    for (int i = 0; i < (int)bitmap.size(); i++) {
        if (bitmap[i] == 0) return i;
    }
    return -1; 
}

void importFile(const string& filename) {
    ifstream src(filename, ios::binary);
    if (!src.is_open()) {
        cout << "Error: cannot open " << filename << endl;
        return;
    }

    fstream ffs(FFS_FILENAME, ios::in | ios::out | ios::binary);
    if (!ffs.is_open()) {
        cout << "Error: cannot open FFS storage\n";
        return;
    }

    vector<char> inode_bitmap(INODE_BITMAP_SIZE);
    vector<char> data_bitmap(DATA_BITMAP_SIZE);
    ffs.seekg(0);
    ffs.read(inode_bitmap.data(), INODE_BITMAP_SIZE);
    ffs.read(data_bitmap.data(), DATA_BITMAP_SIZE);

    vector<string> existingNames;
    for (int i = 0; i < INODE_BITMAP_SIZE; i++) {
        if (inode_bitmap[i] == 1) {
            long inode_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + (i * BLOCK_SIZE);
            ffs.seekg(inode_offset);
            Inode inode{};
            ffs.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
            existingNames.push_back(inode.filename);
        }
    }

    string baseName = filename;
    string namePart = filename;
    string extPart = "";

    size_t dotPos = filename.find_last_of('.');
    if (dotPos != string::npos) {
        namePart = filename.substr(0, dotPos);
        extPart = filename.substr(dotPos);
    }

    string finalName = filename;
    int counter = 1;
    while (find(existingNames.begin(), existingNames.end(), finalName) != existingNames.end()) {
        finalName = namePart + "(" + to_string(counter++) + ")" + extPart;
    }

    int inode_idx = findFreeIndex(inode_bitmap);
    if (inode_idx == -1) {
        cout << "No free inode available\n";
        return;
    }

    int data_idx = findFreeIndex(data_bitmap);
    if (data_idx == -1) {
        cout << "No free data block available\n";
        return;
    }

    vector<char> buf(BLOCK_SIZE, 0);
    src.read(buf.data(), BLOCK_SIZE);
    int read_bytes = src.gcount();

    long data_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + INODE_TABLE_SIZE + (data_idx * BLOCK_SIZE);
    ffs.seekp(data_offset);
    ffs.write(buf.data(), BLOCK_SIZE);

    Inode inode{};
    strncpy(inode.filename, finalName.c_str(), MAX_FILENAME);
    inode.filesize = read_bytes;
    inode.direct_ptrs[0] = data_idx;
    inode.indirect_ptr = -1;

    long inode_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + (inode_idx * BLOCK_SIZE);
    ffs.seekp(inode_offset);
    ffs.write(reinterpret_cast<char*>(&inode), sizeof(Inode));

    inode_bitmap[inode_idx] = 1;
    data_bitmap[data_idx] = 1;
    ffs.seekp(0);
    ffs.write(inode_bitmap.data(), INODE_BITMAP_SIZE);
    ffs.write(data_bitmap.data(), DATA_BITMAP_SIZE);

    cout << "Imported " << filename << " as " << finalName << " (" << read_bytes << " bytes)\n";
}

void listFiles() {
    fstream ffs(FFS_FILENAME, ios::in | ios::binary);
    if (!ffs.is_open()) {
        cout << "Error: cannot open FFS storage\n";
        return;
    }

    vector<char> inode_bitmap(INODE_BITMAP_SIZE);
    ffs.seekg(0);
    ffs.read(inode_bitmap.data(), INODE_BITMAP_SIZE);

    bool anyFile = false;

    for (int i = 0; i < INODE_BITMAP_SIZE; i++) {
        if (inode_bitmap[i] == 1) {
            anyFile = true;

            long inode_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + (i * BLOCK_SIZE);
            ffs.seekg(inode_offset);
            Inode inode{};
            ffs.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

            cout << inode.filename << " " << inode.filesize << " bytes" << endl;
        }
    }

    if (!anyFile)
        cout << "(No files in FFS)" << endl;
}

void deleteFile(const string& filename) {
    fstream ffs(FFS_FILENAME, ios::in | ios::out | ios::binary);
    if (!ffs.is_open()) {
        cout << "Error: cannot open FFS storage\n";
        return;
    }

    vector<char> inode_bitmap(INODE_BITMAP_SIZE);
    vector<char> data_bitmap(DATA_BITMAP_SIZE);
    ffs.seekg(0);
    ffs.read(inode_bitmap.data(), INODE_BITMAP_SIZE);
    ffs.read(data_bitmap.data(), DATA_BITMAP_SIZE);

    bool found = false;

    for (int i = 0; i < INODE_BITMAP_SIZE; i++) {
        if (inode_bitmap[i] == 1) {
            long inode_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + (i * BLOCK_SIZE);
            ffs.seekg(inode_offset);
            Inode inode{};
            ffs.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

            if (filename == inode.filename) {
                found = true;

                for (int j = 0; j < MAX_DIRECT_PTRS; j++) {
                    int blockIdx = inode.direct_ptrs[j];
                    if (blockIdx >= 0 && blockIdx < DATA_BLOCKS_SIZE / BLOCK_SIZE) {
                        data_bitmap[blockIdx] = 0;
                    } else {
                        break;
                    }
                }

                inode_bitmap[i] = 0;

                Inode empty{};
                ffs.seekp(inode_offset);
                ffs.write(reinterpret_cast<char*>(&empty), sizeof(Inode));

                cout << "Deleted file: " << filename << endl;
                break;
            }
        }
    }

    if (!found) {
        cout << "File not found: " << filename << endl;
        return;
    }

    ffs.seekp(0);
    ffs.write(inode_bitmap.data(), INODE_BITMAP_SIZE);
    ffs.write(data_bitmap.data(), DATA_BITMAP_SIZE);
}

void mvFile(const string& oldname, const string& newname) {
    fstream ffs(FFS_FILENAME, ios::in | ios::out | ios::binary);
    if (!ffs.is_open()) {
        cout << "Error: cannot open FFS storage\n";
        return;
    }

    vector<char> inode_bitmap(INODE_BITMAP_SIZE);
    ffs.seekg(0);
    ffs.read(inode_bitmap.data(), INODE_BITMAP_SIZE);

    bool found = false;

    for (int i = 0; i < INODE_BITMAP_SIZE; i++) {
        if (inode_bitmap[i] == 1) {
            long inode_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + (i * BLOCK_SIZE);
            ffs.seekg(inode_offset);
            Inode inode{};
            ffs.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

            if (newname == inode.filename) {
                cout << "Error: file with name '" << newname << "' already exists.\n";
                return;
            }
        }
    }

    for (int i = 0; i < INODE_BITMAP_SIZE; i++) {
        if (inode_bitmap[i] == 1) {
            long inode_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + (i * BLOCK_SIZE);
            ffs.seekg(inode_offset);
            Inode inode{};
            ffs.read(reinterpret_cast<char*>(&inode), sizeof(Inode));

            if (oldname == inode.filename) {
                found = true;

                memset(inode.filename, 0, MAX_FILENAME);
                strncpy(inode.filename, newname.c_str(), MAX_FILENAME - 1);

                ffs.seekp(inode_offset);
                ffs.write(reinterpret_cast<char*>(&inode), sizeof(Inode));

                cout << "Renamed " << oldname << " -> " << newname << endl;
                break;
            }
        }
    }

    if (!found) {
        cout << "File not found: " << oldname << endl;
    }
}

void cpFile(const string& srcname, const string& destname) {
    fstream ffs(FFS_FILENAME, ios::in | ios::out | ios::binary);
    if (!ffs.is_open()) {
        cout << "Error: cannot open FFS storage\n";
        return;
    }

    vector<char> inode_bitmap(INODE_BITMAP_SIZE);
    vector<char> data_bitmap(DATA_BITMAP_SIZE);
    ffs.seekg(0);
    ffs.read(inode_bitmap.data(), INODE_BITMAP_SIZE);
    ffs.read(data_bitmap.data(), DATA_BITMAP_SIZE);

    for (int i = 0; i < INODE_BITMAP_SIZE; i++) {
        if (inode_bitmap[i] == 1) {
            long inode_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + (i * BLOCK_SIZE);
            ffs.seekg(inode_offset);
            Inode inode{};
            ffs.read(reinterpret_cast<char*>(&inode), sizeof(Inode));
            if (destname == inode.filename) {
                cout << "Error: destination file '" << destname << "' already exists.\n";
                return;
            }
        }
    }

    Inode src_inode{};
    bool found = false;
    int src_inode_index = -1;

    for (int i = 0; i < INODE_BITMAP_SIZE; i++) {
        if (inode_bitmap[i] == 1) {
            long inode_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + (i * BLOCK_SIZE);
            ffs.seekg(inode_offset);
            ffs.read(reinterpret_cast<char*>(&src_inode), sizeof(Inode));

            if (srcname == src_inode.filename) {
                found = true;
                src_inode_index = i;
                break;
            }
        }
    }

    if (!found) {
        cout << "File not found: " << srcname << endl;
        return;
    }

    int new_inode_idx = findFreeIndex(inode_bitmap);
    if (new_inode_idx == -1) {
        cout << "No free inode available\n";
        return;
    }

    Inode new_inode{};
    strncpy(new_inode.filename, destname.c_str(), MAX_FILENAME - 1);
    new_inode.filesize = src_inode.filesize;
    new_inode.indirect_ptr = -1;

    int total_blocks = (src_inode.filesize + BLOCK_SIZE - 1) / BLOCK_SIZE;
    for (int b = 0; b < total_blocks; b++) {
        int src_block_idx = src_inode.direct_ptrs[b];
        int dest_block_idx = findFreeIndex(data_bitmap);
        if (dest_block_idx == -1) {
            cout << "No free data block available\n";
            return;
        }

        vector<char> buf(BLOCK_SIZE);
        long src_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + INODE_TABLE_SIZE + (src_block_idx * BLOCK_SIZE);
        ffs.seekg(src_offset);
        ffs.read(buf.data(), BLOCK_SIZE);

        long dest_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + INODE_TABLE_SIZE + (dest_block_idx * BLOCK_SIZE);
        ffs.seekp(dest_offset);
        ffs.write(buf.data(), BLOCK_SIZE);

        new_inode.direct_ptrs[b] = dest_block_idx;
        data_bitmap[dest_block_idx] = 1;
    }

    long inode_offset = INODE_BITMAP_SIZE + DATA_BITMAP_SIZE + (new_inode_idx * BLOCK_SIZE);
    ffs.seekp(inode_offset);
    ffs.write(reinterpret_cast<char*>(&new_inode), sizeof(Inode));

    inode_bitmap[new_inode_idx] = 1;

    ffs.seekp(0);
    ffs.write(inode_bitmap.data(), INODE_BITMAP_SIZE);
    ffs.write(data_bitmap.data(), DATA_BITMAP_SIZE);

    cout << "Copied " << srcname << " -> " << destname << " (" << new_inode.filesize << " bytes)\n";
}


int main() {
    initFFS();
    cout << "$ ";  
    
    string line;
    while (true) {
        if (!getline(cin, line)) break;

        stringstream ss(line);
        string cmd;
        ss >> cmd;

        if (cmd.empty()) {
            cout << "$ ";
            continue;
        }

        if (cmd == "exit") {
            cout << "Exiting FFS..." << endl;
            break;
        } 
        else if (cmd == "import") {
            string filename;
            ss >> filename;
            if (filename.empty()) cout << "Usage: import <filename>\n";
            else importFile(filename);
        }
        else if (cmd == "ls") {
            listFiles();
        } 
        else if (cmd == "del") {
            string filename;
            ss >> filename;
            if (filename.empty()) cout << "Usage: del <filename>\n";
            else deleteFile(filename);
        } 
        else if (cmd == "mv") {
            string src, dest;
            ss >> src >> dest;
            if (src.empty() || dest.empty()) cout << "Usage: mv <oldname> <newname>\n";
            else mvFile(src, dest);
        } 
        else if (cmd == "cp") {
            string src, dest;
            ss >> src >> dest;
            if (src.empty() || dest.empty()) cout << "Usage: cp <src> <dest>\n";
            else cpFile(src, dest);
        } 
        else {
            cout << "Unknown command: " << cmd << endl;
        }

        cout << "$ ";
    }

    return 0;
}