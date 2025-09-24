// backup_cleanup.h
#ifndef BACKUP_CLEANUP_H
#define BACKUP_CLEANUP_H

#include <string>

class BackupCleanup {
public:
    explicit BackupCleanup(const std::string& backupRoot, int keepLatest = 1);
    // Removes old backups under backupRoot/nextcloud-server keeping N most recent
    bool removeOldBackups(const std::string& serverFolderName = "nextcloud-server");

private:
    std::string backupRoot;
    int keepLatest;
};

#endif // BACKUP_CLEANUP_H
