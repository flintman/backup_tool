# SPDX-License-Identifier: MIT
# Copyright (c) 2025 William C. Bellavance Jr.
# Backup Manager C++

Backup manager, Reads configuration from `backup.env` if present in each HOME folder, performs MySQL (docker or host) dumps, archives directories, pushes to a remote backup server via SSH/SCP (using `sshpass`), optionally handles Nextcloud maintenance mode, and sends Telegram notifications.

## Build

Requirements: g++, make,

```bash
make deb
```


## Requirements

```bash
sudo apt install tar docker.io mysql-client sshpass rsync curl
```

## Config

Edit `/home/INSTALL_USER/backup/backup.env`. Keys include:
- IS_DOCKER, IS_MYSQL, IS_NEXTCLOUD
- SERVER_NAME
- BACKUP_SERVER_USERNAME, BACKUP_SERVER_PASSWORD, BACKUP_SERVER_IP, BACKUP_SERVER_DEST
- BACKUP_FILES_DIR, BACKUP_SQL_DIR
- MYSQL_USERNAME, MYSQL_PASSWORD
- BOT_ID, CHANNEL_ID
- NEXTCLOUD_OCC_USER, NEXTCLOUD_PATH
- DATABASES_<i>_DOCKER_NAME, DATABASES_<i>_NAME, DATABASES_<i>_USER, DATABASES_<i>_PASSWORD,
- TOTAL_BACKUPS_TO_KEEP

## Run

```bash
sudo bmanager
```

## Cron

Example crontab entry (run daily at 02:30):

```cron
30 2 * * * bmanager > /home/USER/backup/log/backup.log 2>&1
```

