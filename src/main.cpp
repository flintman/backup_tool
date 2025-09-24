#include "backup_manager.h"
#include <iostream>

int main(int argc, char** argv) {
    BackupManager bm("backup.env");
    if (argc > 1) {
        for (int i = 1; i < argc; ++i) bm.addDirectoryToBackup(argv[i]);
    }
    bool ok = bm.backup();
    return ok ? 0 : 1;
}
