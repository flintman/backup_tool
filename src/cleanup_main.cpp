#include "backup_cleanup.h"
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <string>

static int getenv_or_default(const std::string& file, const std::string& key, int defVal) {
    std::ifstream in(file);
    if (!in.is_open()) return defVal;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0]=='#') continue;
        auto pos = line.find('=');
        if (pos==std::string::npos) continue;
        auto k = line.substr(0,pos);
        if (k==key) {
            try { return std::stoi(line.substr(pos+1)); } catch(...) { return defVal; }
        }
    }
    return defVal;
}

int main(int argc, char** argv) {
    std::string root = "/media/flintman/backups";
    int keep = getenv_or_default("backup.env", "CLEANUP_KEEP_LATEST", 1);
    if (argc > 1) root = argv[1];
    if (argc > 2) keep = std::atoi(argv[2]);

    BackupCleanup bc(root, keep);
    bool ok = bc.removeOldBackups();
    return ok ? 0 : 1;
}
