#include <iostream>
#include <string>
#include <sstream>

using namespace std;

int main() {
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
            else cout << "Importing file: " << filename << endl;
        } 
        else if (cmd == "ls") {
            cout << "Listing files (stub)...\n";
        } 
        else if (cmd == "del") {
            string filename;
            ss >> filename;
            if (filename.empty()) cout << "Usage: del <filename>\n";
            else cout << "Deleting file: " << filename << endl;
        } 
        else if (cmd == "mv") {
            string src, dest;
            ss >> src >> dest;
            if (src.empty() || dest.empty()) cout << "Usage: mv <oldname> <newname>\n";
            else cout << "Renaming " << src << " -> " << dest << endl;
        } 
        else if (cmd == "cp") {
            string src, dest;
            ss >> src >> dest;
            if (src.empty() || dest.empty()) cout << "Usage: cp <src> <dest>\n";
            else cout << "Copying " << src << " -> " << dest << endl;
        } 
        else {
            cout << "Unknown command: " << cmd << endl;
        }

        cout << "$ ";
    }

    return 0;
}