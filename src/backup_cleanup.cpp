#include "backup_cleanup.h"
#include <filesystem>
#include <regex>
#include <vector>
#include <algorithm>
#include <iostream>

namespace fs = std::filesystem;

BackupCleanup::BackupCleanup(const std::string& backupRoot, int keepLatest)
    : backupRoot(backupRoot), keepLatest(keepLatest) {}

bool BackupCleanup::removeOldBackups(const std::string& serverFolderName) {
    std::error_code ec;
    fs::path root = fs::path(backupRoot) / serverFolderName;
    if (!fs::exists(root, ec) || !fs::is_directory(root, ec)) return true;

    std::regex re(".*(\\d{4}-\\d{2}-\\d{2}_\\d{2}-\\d{2}-\\d{2})$");

    struct Item { fs::path path; std::string stamp; };
    std::vector<Item> items;

    for (auto& entry : fs::directory_iterator(root, ec)) {
        if (!entry.is_directory()) continue;
        std::smatch m;
        auto name = entry.path().filename().string();
        if (std::regex_search(name, m, re)) {
            items.push_back({entry.path(), m[1].str()});
        }
    }

    std::sort(items.begin(), items.end(), [](const Item& a, const Item& b){ return a.stamp > b.stamp; });

    if ((int)items.size() <= keepLatest) return true;

    for (size_t i = keepLatest; i < items.size(); ++i) {
        std::cout << "Removing old backup: " << items[i].path << std::endl;
        fs::remove_all(items[i].path, ec);
    }

    return true;
}
