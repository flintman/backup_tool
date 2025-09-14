from backup_manager import BackupManager

backup_manager = BackupManager(
    is_docker=True,
    server_name="SERVERX-server",
    backup_server_username="USERNAME",
    backup_server_password="PASSWORD",
    backup_server_ip="IP ADRESS",
    databases = [

    ],
    bot_id="",
    channel_id="",

)
backup_manager.add_directory_to_backup("")
backup_manager.backup()
