#!/usr/bin/env python3

import os
from datetime import datetime
import re
import shutil

backup_path = "/media/flintman/backups"

def remove_old_backups():
    for folder_name in os.listdir(backup_path):
        if folder_name == "nextcloud-server":
            server_folder = os.path.join(backup_path, folder_name)
            if os.path.isdir(server_folder):
                backups = []

                # Collect all backups in the server folder
                for backup_name in os.listdir(server_folder):
                    backup_full_path = os.path.join(server_folder, backup_name)
                    if os.path.isdir(backup_full_path):
                        match = re.search(r"\d{4}-\d{2}-\d{2}_\d{2}-\d{2}-\d{2}$", backup_name)
                        if match:
                            timestamp = match.group()
                            try:
                                backup_date = datetime.strptime(timestamp, "%Y-%m-%d_%H-%M-%S")
                                backups.append((backup_full_path, backup_date))
                            except ValueError:
                                # Skip folders that don't match the expected format
                                continue

                # Sort backups by date
                backups.sort(key=lambda x: x[1], reverse=True)

                # Keep the most recent backup and remove the rest
                if len(backups) > 1:
                    for old_backup, _ in backups[1:]:
                        print(f"Removing old backup: {old_backup}")
                        shutil.rmtree(old_backup)

if __name__ == "__main__":
    remove_old_backups()

