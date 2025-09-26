// SPDX-License-Identifier: MIT
// Copyright (c) 2025 William C. Bellavance Jr.

#ifndef BACKUP_MANAGER_H
#define BACKUP_MANAGER_H

#include <string>
#include <vector>
#include <map>

struct DatabaseInfo {
    std::string dockerName;
    std::string name;
    std::string user;
    std::string password;
};

struct BackupResult {
    bool ok {false};
    std::string archivePath;       // full path to created .tar.gz
    std::string logDirectoryName;  // e.g., backup_SERVERX-server_YYYY-mm-dd_HH-MM-SS
    std::string tarLogFileName;    // e.g., tar_backup_log_...
    std::string message;
};

class BackupManager {
public:
    explicit BackupManager(const std::string& configPath = "backup.env");

    bool loadConfig();

    // Full backup flow
    bool backup();

    const std::map<std::string, std::string>& getConfig() const { return config; }
    const std::vector<std::string>& getDirectories() const { return directoriesToBackup; }

private:
    // Config and state
    std::string configPath;
    std::map<std::string, std::string> config;
    std::vector<std::string> directoriesToBackup; // Initialized with mysql dump folder
    std::vector<DatabaseInfo> databases;
    std::string currentDir;
    std::string timestamp; // YYYY-mm-dd_HH-MM-SS

    // Derived from config
    bool isDocker {false};
    bool isMysql {false};
    bool isNextcloud {false};
    std::string serverName;
    std::string backupServerUser;
    std::string backupServerPass;
    std::string backupServerIp;
    std::string backupServerDest; // default: /media/flintman/backups/<serverName>
    std::string mysqlUser;
    std::string mysqlPass;
    std::string botId;
    std::string channelId;

    // Local folders like Python
    std::string backupDestination;   // ./files_to
    std::string backupSqlDestination;// ./mysql

private:
    // Steps
    void initDefaults();
    bool parseEnvFile();
    void inflateConfig();
    void loadDatabasesFromConfig();
    static std::string trim(const std::string& s);
    static std::vector<std::string> split(const std::string& s, char sep);

    // External effects
    void sendTelegramMessage(const std::string& message);
    void clearDirectory(const std::string& directory);

    // Backup pieces
    void backupMysqlAllDatabases();
    void backupMysqlDockerDatabases();
    BackupResult backupDirectories();
    bool pushToBackupServer(const BackupResult& br);

    // Shell helpers
    static int runCommand(const std::string& cmd);
    static std::string runCommandCapture(const std::string& cmd);
    static bool ensureDir(const std::string& path);
};

#endif // BACKUP_MANAGER_H
