
// SPDX-License-Identifier: MIT
// Copyright (c) 2025 William C. Bellavance Jr.

#include "backup_manager.h"

#include <filesystem>
#include <fstream>
#include <sstream>
#include <iostream>
#include <chrono>
#include <iomanip>
#include <cstdlib>
#include <curl/curl.h>
#include <algorithm>

namespace fs = std::filesystem;

static std::string now_timestamp() {
    using clock = std::chrono::system_clock;
    auto t = clock::to_time_t(clock::now());
    std::tm tm{};
    localtime_r(&t, &tm);
    std::ostringstream os;
    os << std::put_time(&tm, "%Y-%m-%d_%H-%M-%S");
    return os.str();
}

BackupManager::BackupManager(const std::string& configPath)
    : configPath(configPath) {
    initDefaults();
}

void BackupManager::initDefaults() {
    currentDir = fs::current_path().string();
    timestamp = now_timestamp();

    backupDestination = currentDir + "/files_to";
    backupSqlDestination = currentDir + "/mysql";

    directoriesToBackup.clear();
    directoriesToBackup.push_back(backupSqlDestination);
}

std::string BackupManager::trim(const std::string& s) {
    const char* ws = " \t\r\n";
    const auto b = s.find_first_not_of(ws);
    if (b == std::string::npos) return "";
    const auto e = s.find_last_not_of(ws);
    return s.substr(b, e - b + 1);
}

std::vector<std::string> BackupManager::split(const std::string& s, char sep) {
    std::vector<std::string> out;
    std::string cur;
    std::istringstream is(s);
    while (std::getline(is, cur, sep)) out.push_back(cur);
    return out;
}

bool BackupManager::ensureDir(const std::string& path) {
    std::error_code ec;
    if (fs::exists(path, ec)) return fs::is_directory(path, ec);
    return fs::create_directories(path, ec);
}

bool BackupManager::loadConfig() {
    bool ok = parseEnvFile();
    inflateConfig();
    loadDatabasesFromConfig();
    return ok;
}

bool BackupManager::parseEnvFile() {
    std::ifstream in(configPath);
    if (!in.is_open()) return false;
    std::string line;
    while (std::getline(in, line)) {
        if (line.empty() || line[0] == '#') continue;
        auto pos = line.find('=');
        if (pos == std::string::npos) continue;
        auto key = trim(line.substr(0, pos));
        auto val = trim(line.substr(pos + 1));
        if (!key.empty()) config[key] = val;
    }
    return true;
}

void BackupManager::inflateConfig() {
    auto get = [&](const std::string& k, const std::string& d = std::string()) -> std::string {
        auto it = config.find(k);
        return it == config.end() ? d : it->second;
    };

    isDocker = get("IS_DOCKER", "False") == "True" || get("IS_DOCKER", "0") == "1";
    isMysql = get("IS_MYSQL", "False") == "True" || get("IS_MYSQL", "0") == "1";
    isNextcloud = get("IS_NEXTCLOUD", "False") == "True" || get("IS_NEXTCLOUD", "0") == "1";

    serverName = get("SERVER_NAME", "XXXXXXXXXXX-server");

    backupServerUser = get("BACKUP_SERVER_USERNAME", "SSHUSERNAME");
    backupServerPass = get("BACKUP_SERVER_PASSWORD", "SSHPASSWORD");
    backupServerIp   = get("BACKUP_SERVER_IP", "127.0.0.1");
    backupBandwidthLimit = get("BACKUP_BANDWIDTH_LIMIT", "200m");
    backupServerDest = get("BACKUP_SERVER_DEST", "");
    if (backupServerDest.empty()) backupServerDest = "/media/flintman/backups/" + serverName;

    mysqlUser = get("MYSQL_USERNAME", "USERNAME");
    mysqlPass = get("MYSQL_PASSWORD", "PASSWORD");

    botId = get("BOT_ID", "");
    channelId = get("CHANNEL_ID", "");

    // Allow overriding local directories
    backupDestination = get("BACKUP_FILES_DIR", "files_to");
    backupSqlDestination = get("BACKUP_SQL_DIR", "mysql");

    backupsToKeep = get("TOTAL_BACKUPS_TO_KEEP", "2");

    // Keep mysql dir in list
    directoriesToBackup.clear();

    if (isMysql || isDocker){
        directoriesToBackup.push_back(backupSqlDestination);
    }
}

void BackupManager::loadDatabasesFromConfig() {
    databases.clear();
    // Scan contiguous indices 0..N
    for (int i = 0; i < 1000; ++i) {
        std::string prefix = "DATABASES_" + std::to_string(i) + "_";
        auto it = config.find(prefix + "DOCKER_NAME");
        if (it == config.end()) {
            // Stop when an index is missing to avoid sparse search
            if (i > 0) break;
            else continue;
        }
        DatabaseInfo dbi;
        dbi.dockerName = it->second;
        dbi.name = config[prefix + "NAME"];
        dbi.user = config[prefix + "USER"];
        dbi.password = config[prefix + "PASSWORD"];
        databases.push_back(std::move(dbi));
    }
}

int BackupManager::runCommand(const std::string& cmd) {
    return std::system(cmd.c_str());
}

std::string BackupManager::runCommandCapture(const std::string& cmd) {
    std::string data;
    FILE* pipe = popen((cmd + " 2>/dev/null").c_str(), "r");
    if (!pipe) return data;
    char buf[4096];
    while (fgets(buf, sizeof(buf), pipe)) data.append(buf);
    pclose(pipe);
    return data;
}

void BackupManager::sendTelegramMessage(const std::string& message) {
    if (botId.empty() || channelId.empty()) {
        std::cout << message << " Not sent" << std::endl;
        return;
   }
    CURL* curl = curl_easy_init();

    std::string url = "https://api.telegram.org/bot" + botId +
                      "/sendMessage?chat_id=" + channelId +
                      "&text=" + curl_easy_escape(curl, message.c_str(), message.length());

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    std::cout << "Telegram message sent: " << message << std::endl;
}

void BackupManager::clearDirectory(const std::string& directory) {
    std::error_code ec;
    if (fs::exists(directory, ec)) fs::remove_all(directory, ec);
    fs::create_directories(directory, ec);
    std::cout << "Clearing directory: " << directory << std::endl;
}

void BackupManager::pruneRemoteBackups(const std::string& remoteDir, int backupsToKeep) {
    // List backup directories on remote, sort by mtime, remove oldest beyond backupsToKeep-1
    std::ostringstream listCmd;
    listCmd << "sshpass -p '" << backupServerPass << "' ssh -o StrictHostKeyChecking=no "
            << backupServerUser << "@" << backupServerIp << " "
            << "\"ls -1dt '" << remoteDir << "/backup_'* 2>/dev/null\"";
    std::string result = runCommandCapture(listCmd.str());
    std::vector<std::string> dirs;
    std::istringstream is(result);
    std::string line;
    while (std::getline(is, line)) {
        line = trim(line);
        if (!line.empty()) dirs.push_back(line);
    }
    if (dirs.size() <= size_t(backupsToKeep - 1)) return;
    for (size_t i = backupsToKeep - 1; i < dirs.size(); ++i) {
        std::cout << "Pruning remote backup: " << dirs[i] << std::endl;
        std::ostringstream rmCmd;
        rmCmd << "sshpass -p '" << backupServerPass << "' ssh -o StrictHostKeyChecking=no "
              << backupServerUser << "@" << backupServerIp << " "
              << "\"rm -rf '" << dirs[i] << "'\"";
        runCommand(rmCmd.str());
    }
}

void BackupManager::backupMysqlAllDatabases() {
    ensureDir(backupSqlDestination);
    std::cout << "Starting to backup Mysql Databases" << std::endl;
    // List DBs
    std::string cmd = "mysql -u '" + mysqlUser + "' -p'" + mysqlPass + "' -e 'SHOW DATABASES;' | grep -Ev '(Database|information_schema|performance_schema)'";
    auto out = runCommandCapture(cmd);
    std::istringstream is(out);
    std::string db;
    while (is >> db) {
        std::cout << "Backup of MySQL database " << db << " Started" << std::endl;
        std::string sqlFile = backupSqlDestination + "/" + db + "_" + timestamp + ".sql";
        std::string dump = "mysqldump -u '" + mysqlUser + "' -p'" + mysqlPass + "' '" + db + "' > '" + sqlFile + "'";
        runCommand(dump);
        std::cout << "Backup of MySQL database " << db << " completed: " << sqlFile << std::endl;
    }
}

void BackupManager::backupMysqlDockerDatabases() {
    ensureDir(backupSqlDestination);
    std::cout << "Starting to backup MySQL Databases" << std::endl;
    for (const auto& dbi : databases) {
        std::cout << "Backup of MySQL database " << dbi.name << " Started" << std::endl;
        std::string sqlFile = backupSqlDestination + "/" + dbi.name + "_" + timestamp + ".sql";
        std::ostringstream cmd;
        cmd << "docker exec " << dbi.dockerName
            << " mysqldump --no-tablespaces -u '" << dbi.user << "' --password='" << dbi.password
            << "' '" << dbi.name << "' > '" << sqlFile << "'";
        runCommand(cmd.str());
        std::cout << "Backup of MySQL database " << dbi.name << " completed" << std::endl;
    }
}

BackupResult BackupManager::backupDirectories() {
    BackupResult br;
    ensureDir(backupDestination);
    // Load additional backup folders from config if present
    auto folders = config.count("BACKUP_FOLDERS") ? config["BACKUP_FOLDERS"] : "";
    if (!folders.empty()) {
        auto extraDirs = split(folders, ',');
        for (auto& dir : extraDirs) {
            dir = trim(dir);
            if (!dir.empty()) directoriesToBackup.push_back(dir);
        }
    }

    std::cout << "Starting to backup Directories" << std::endl;

    std::string logDirName = "backup_" + serverName + "_" + timestamp;
    std::string logDir = backupDestination + "/" + logDirName;
    ensureDir(logDir);

    std::string archive = logDir + "/backup_all_directories_" + timestamp + ".tar.gz";
    std::string tarLog = "tar_backup_log_" + timestamp + ".txt";

    // Build tar command
    std::ostringstream dirs;
    for (size_t i = 0; i < directoriesToBackup.size(); ++i) {
        if (i) dirs << ' ';
        dirs << directoriesToBackup[i];
        std::cout << "Including directory in backup: " << directoriesToBackup[i] << std::endl;
    }

    std::ostringstream tar;
    tar << "tar -zcvf '" << archive << "' " << dirs.str()
        << " > '" << logDir << "/" << tarLog << "' 2>&1";

    runCommand(tar.str());

    std::cout << "Backup of all directories completed: " << archive << std::endl;

    br.ok = true;
    br.archivePath = archive;
    br.logDirectoryName = logDirName;
    br.tarLogFileName = tarLog;
    return br;
}

bool BackupManager::pushToBackupServer(const BackupResult& br) {
    std::cout << "Backing up to remote Server" << std::endl;

    // Replicate Python SFTP using sshpass + scp + ssh
    // Ensure remote directory exists
    std::ostringstream ensure;
    ensure << "sshpass -p '" << backupServerPass << "' ssh -o StrictHostKeyChecking=no "
           << backupServerUser << "@" << backupServerIp
           << " \"mkdir -p '" << backupServerDest << "/" << br.logDirectoryName << "'\"";
    runCommand(ensure.str());

    // Copy files
    std::ostringstream scp1;
    scp1 << "sshpass -p '" << backupServerPass << "' scp -o StrictHostKeyChecking=no '" << br.archivePath
         << "' " << backupServerUser << "@" << backupServerIp << ":'" << backupServerDest
         << "/" << br.logDirectoryName << "/'";
    runCommand(scp1.str());

    std::ostringstream scp2;
    scp2 << "sshpass -p '" << backupServerPass << "' scp -o StrictHostKeyChecking=no '"
         << backupDestination << "/" << br.logDirectoryName << "/" << br.tarLogFileName
         << "' " << backupServerUser << "@" << backupServerIp << ":'" << backupServerDest
         << "/" << br.logDirectoryName << "/'";
    runCommand(scp2.str());

    std::cout << "Backup file pushed to backup server" << std::endl;
    return true;
}

bool BackupManager::backup() {
    if (!loadConfig()) {
        std::cerr << "Failed to load config file: " << configPath << std::endl;
        return false;
    }

    // Telemetry: start
    {
        std::ostringstream msg;
        msg << now_timestamp() << ": Backing Service for " << serverName << " has started";
        sendTelegramMessage(msg.str());
    }

    if (isNextcloud) {
        std::cout << "Putting Next Cloud into Maintenance Mode" << std::endl;
        std::ostringstream occ;
        occ << "sudo -u " << config["NEXTCLOUD_OCC_USER"]
            << " php " << config["NEXTCLOUD_PATH"] << "occ"
            << " maintenance:mode --on";
        runCommand(occ.str());
    }

    if (isDocker) {
        backupMysqlDockerDatabases();
    }

    if (isMysql) {
        backupMysqlAllDatabases();
    }

    auto br = backupDirectories();
    if (br.ok) {
        pruneRemoteBackups(backupServerDest, std::stoi(backupsToKeep));
        std::cout << "Pushing to backup server" << std::endl;
        pushToBackupServer(br);
    }

    // Clear directories like Python
    if (isMysql || isDocker) {
        clearDirectory(backupSqlDestination);
    }
    clearDirectory(backupDestination);

    if (isNextcloud) {
        std::string rsyncOptions = "-avz --compress --partial --partial-dir=.rsync-partial --bwlimit=" + backupBandwidthLimit;
        std::ostringstream cmd;
        cmd << "sshpass -p '" << backupServerPass << "' rsync " << rsyncOptions
            << " " << config["NEXTCLOUD_PATH"] << " "
            << backupServerUser << "@" << backupServerIp << ":" << backupServerDest
            << "/backup_" << serverName << "_" << timestamp << "/nextcloud/";
        runCommand(cmd.str());

        std::ostringstream occ;
        occ << "sudo -u " << config["NEXTCLOUD_OCC_USER"]
            << " php " << config["NEXTCLOUD_PATH"] << "occ"
            << " maintenance:mode --off";
        runCommand(occ.str());
    }

    // Telemetry: complete
    {
        std::ostringstream msg;
        msg << now_timestamp() << ": Backing Service for " << serverName << " has completed";
        sendTelegramMessage(msg.str());
    }

    return br.ok;
}

int main() {
    // If you want to backup for all users, you need to iterate over /home directories
    // Example: backup for each user with a backup.env file
    bool allOk = true;
    for (const auto& entry : std::filesystem::directory_iterator("/home")) {
        if (!entry.is_directory()) continue;
        std::string username = entry.path().filename().string();
        std::string configPath = "/home/" + username + "/backup/backup.env";
        if (std::filesystem::exists(configPath)) {
            BackupManager bm(configPath);
            bool ok = bm.backup();
            allOk = allOk && ok;
        }
    }
    return allOk ? 0 : 1;
}