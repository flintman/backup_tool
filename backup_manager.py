import subprocess
import paramiko
import os
import shutil
import requests
from datetime import datetime
from pathlib import Path

class BackupManager:
    def __init__(self, **kwargs):
        self.current_dir = str(Path().absolute())
        self.datetime_now = datetime.now().strftime("%Y-%m-%d_%H-%M-%S")
        self.backup_destination = f"/{self.current_dir}/files_to"
        self.backup_sql_destination = f"/{self.current_dir}/mysql"
        self.is_nextcloud = kwargs.get('is_nextcloud', False)
        self.is_docker= kwargs.get('is_docker', False)
        self.is_mysql= kwargs.get('is_mysql', False)
        self.databases= kwargs.get('databases', "")
        
        self.server_name = kwargs.get('server_name', "XXXXXXXXXXX-server")
        self.directories_to_backup = [self.backup_sql_destination]
        self.mysql_username = kwargs.get('mysql_username', "USERNAME")
        self.mysql_password = kwargs.get('mysql_password', "PASSWORD")

        self.backup_server_username = kwargs.get('backup_server_username', "SSHUSERNAME")
        self.backup_server_password = kwargs.get('backup_server_password', "SSHPASSWORD")
        self.backup_server_ip = kwargs.get('backup_server_ip', "XXX.XXX.XXX.XXX")
        self.backup_server_destination = kwargs.get('backup_server_destination', f"/media/flintman/backups/{self.server_name}")

        self.bot_id = kwargs.get('bot_id', "BOTID")
        self.channel_id = kwargs.get('channel_id', "CHANNELID")

    def send_telegram_message(self, message: str):
        url = f'https://api.telegram.org/bot{self.bot_id}/sendMessage?chat_id={self.channel_id}&text={message}'
        response = requests.post(url, verify=False)
        print(f"{message}")
        return response

    # Add a method to update directories_to_backup attribute
    def add_directory_to_backup(self, directory):
        self.directories_to_backup.append(directory)

    # Clears the directory after the backup is pushed. 
    def clear_directory(self, directory):
        shutil.rmtree(directory)
        os.mkdir(directory)
        print(f"Clearing directory: {directory}")

    def backup_directories(self):
        if not os.path.exists(self.backup_destination):
            os.makedirs(self.backup_destination)
        
        print("Starting to backup Directories")
        
        log_directory = f"{self.backup_destination}/backup_{self.server_name}_{self.datetime_now}"
        os.makedirs(log_directory)

        backup_file = f"{log_directory}/backup_all_directories_{self.datetime_now}.tar.gz"
        directories_string = " ".join(self.directories_to_backup)

        tar_log_file = f"tar_backup_log_{self.datetime_now}.txt"
        subprocess.run(f"tar -zcvf {backup_file} {directories_string} > {log_directory}/{tar_log_file} 2>&1", shell=True)
        
        print(f"Backup of all directories completed: {backup_file}")
        return backup_file, f"backup_{self.server_name}_{self.datetime_now}", tar_log_file

    def backup_mysql_all_databases(self):
        if not os.path.exists(self.backup_sql_destination):
            os.makedirs(self.backup_sql_destination)

        print("Starting to backup Mysql Databases")

        databases_output = subprocess.run(f"mysql -u {self.mysql_username} -p{self.mysql_password} -e 'SHOW DATABASES;' | grep -Ev '(Database|information_schema|performance_schema)'", shell=True, capture_output=True, text=True)
        databases = databases_output.stdout.split()

        for db in databases:
            sql_backup_file = f"{self.backup_sql_destination}/{db}_{self.datetime_now}.sql"
            subprocess.run(f"mysqldump -u {self.mysql_username} -p{self.mysql_password} {db} > {sql_backup_file}", shell=True)
            print(f"Backup of MySQL database {db} completed: {sql_backup_file}")

    def backup_mysql_docker_databases(self):
        if not os.path.exists(self.backup_sql_destination):
            os.makedirs(self.backup_sql_destination)

        print("Starting to backup MySQL Databases")

        for db_info in self.databases:
            docker_name = db_info['docker_name']
            db_name = db_info['name']
            db_user = db_info['user']
            db_password = db_info['password']

            print(f"Backup of MySQL database {db_name} Started")
            subprocess.run(f"docker exec {docker_name} mysqldump -u {db_user} --password={db_password} {db_name} > {self.backup_sql_destination}/{db_name}_{self.datetime_now}.sql", shell=True, capture_output=True, text=True)
            print(f"Backup of MySQL database {db_name} completed")

    def push_to_backup_server(self, local_file, log_directory, tar_log_file):
        print("Backing up to remote Server")
        ssh_client = paramiko.SSHClient()
        ssh_client.set_missing_host_key_policy(paramiko.AutoAddPolicy())

        try:
            ssh_client.connect(self.backup_server_ip, username=self.backup_server_username, password=self.backup_server_password)
            sftp = ssh_client.open_sftp()
            
            try:
                sftp.stat(f"{self.backup_server_destination}/{log_directory}/")
            except FileNotFoundError:
                sftp.mkdir(f"{self.backup_server_destination}/{log_directory}/")
            
            sftp.put(local_file, f"{self.backup_server_destination}/{log_directory}/{os.path.basename(local_file)}")
            sftp.put(f"{self.backup_destination}/{log_directory}/{tar_log_file}", f"{self.backup_server_destination}/{log_directory}/{os.path.basename(tar_log_file)}")
            print(f"Backup file '{local_file}' pushed to backup server")
            ssh_client.close()
        except paramiko.AuthenticationException:
            print("Authentication failed. Please check your credentials.")
            ssh_client.close()

    def backup(self):
        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.send_telegram_message(f"{current_time}: Backing Service for {self.server_name} has started")

        if self.is_nextcloud:
            print("Putting Next Cloud into Maintenance Mode")
            subprocess.run("sudo -u www-data php /var/www/html/nc_here/occ maintenance:mode --on", shell=True)

        if self.is_docker:
            self.backup_mysql_docker_databases()

        if self.is_mysql:
            self.backup_mysql_all_databases()

        backup_file, log_directory, tar_log_file = self.backup_directories()
        self.push_to_backup_server(backup_file, log_directory, tar_log_file)

        self.clear_directory(self.backup_sql_destination)
        self.clear_directory(self.backup_destination)

        if self.is_nextcloud:
            rsync_options = "-avz --compress --partial --partial-dir=.rsync-partial --bwlimit=150m"
            rsync_command = f"sshpass -p '{self.backup_server_password}' rsync {rsync_options} /var/www/html/ {self.backup_server_username}@{self.backup_server_ip}:{self.backup_server_destination}/backup_{self.server_name}_{self.datetime_now}/"
            subprocess.run(rsync_command, shell=True)
            subprocess.run("sudo -u www-data php /var/www/html/nc_here/occ maintenance:mode --off", shell=True)

        current_time = datetime.now().strftime("%Y-%m-%d %H:%M:%S")
        self.send_telegram_message(f"{current_time}: Backing Service for {self.server_name} has completed")

