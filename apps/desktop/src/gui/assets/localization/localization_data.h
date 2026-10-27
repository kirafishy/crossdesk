/*
 * @Author: DI JUNKUN
 * @Date: 2024-05-29
 * Copyright (c) 2026 by DI JUNKUN, All Rights Reserved.
 */

#ifndef _LOCALIZATION_DATA_H_
#define _LOCALIZATION_DATA_H_

namespace crossdesk {
namespace localization {

namespace detail {

struct TranslationRow {
  const char* key;
  const void* zh;
  const void* en;
  const void* ru;
};

// Single source of truth for all UI strings.
#define CROSSDESK_LOCALIZATION_ALL(X)                                          \
  X(window_minimize, u8"最小化", "Minimize", u8"Свернуть") \
  X(window_maximize, u8"最大化", "Maximize", u8"Развернуть") \
  X(window_restore, u8"还原窗口", "Restore window", u8"Восстановить окно") \
  X(window_close, u8"关闭窗口", "Close window", u8"Закрыть окно") \
  X(save, u8"保存", "Save", u8"Сохранить") \
  X(copyright_notice, u8"© 2026 JUNKUN DI。保留所有权利。", "© 2026 by JUNKUN DI. All rights reserved.", u8"© 2026 JUNKUN DI. Все права защищены.") \
  X(license_notice, u8"依据 GNU GPL v3 许可发布。", "Licensed under GNU GPL v3.", u8"Распространяется по лицензии GNU GPL v3.") \
  X(messages, u8"消息", "Messages", u8"Сообщения") \
  X(message_unread, u8"未读", "Unread", u8"Не прочитано") \
  X(message_read, u8"已读", "Read", u8"Прочитано") \
  X(message_read_all, u8"全部已读", "Mark all as read", u8"Отметить всё прочитанным") \
  X(message_empty, u8"暂无公告", "No announcements", u8"Нет объявлений") \
  X(message_select, u8"选择公告查看详情", "Select an announcement to read", u8"Выберите объявление для просмотра") \
  X(message_loading, u8"正在加载…", "Loading…", u8"Загрузка…") \
  X(message_read_save_error, u8"无法保存本地阅读状态，请重试。", "Unable to save reading status locally. Please try again.", u8"Не удалось сохранить статус чтения локально. Повторите попытку.") \
  X(message_close, u8"关闭", "Close", u8"Закрыть") \
  X(message_delete, u8"删除", "Delete", u8"Удалить") \
  X(message_delete_title, u8"删除公告", "Delete announcement", u8"Удалить объявление") \
  X(message_delete_confirmation, u8"删除这条公告？", "Delete this announcement?", u8"Удалить это объявление?") \
  X(message_delete_error, u8"无法删除这条公告，请重试。", "Unable to delete this announcement. Please try again.", u8"Не удалось удалить объявление. Повторите попытку.") \
  X(console_cli_help, u8"Linux 无头运行选项：\n  --headless                  自动选择已有桌面并启用控制台\n  --headless-display DISPLAY  高级：指定已有显示会话\n  --headless-size WIDTHxHEIGHT 新建虚拟屏幕；默认1920x1080，偶数尺寸320..8192\n  --headless-session EXECUTABLE 启动桌面环境（startxfce4；none 仅启动裸屏幕）\n  --no-headless               禁用自动回退\n正常启动会自动选择已有桌面，无需填写显示编号。控制台输入 settings 管理设置。\n", "Linux headless options:\n  --headless                  Select an existing desktop and enable the console\n  --headless-display DISPLAY  Advanced: override desktop selection\n  --headless-size WIDTHxHEIGHT New virtual display; default 1920x1080, even 320..8192\n  --headless-session EXECUTABLE Start a desktop (startxfce4; none for a bare display)\n  --no-headless               Disable automatic fallback\nNormal startup selects your existing desktop automatically. Enter settings in the console to configure it.\n", u8"Параметры запуска без монитора:\n  --headless                  Выбрать рабочий стол и включить консоль\n  --headless-display DISPLAY  Дополнительно: указать сеанс дисплея\n  --headless-size WIDTHxHEIGHT Новый виртуальный экран; 1920x1080 по умолчанию, чётные размеры 320..8192\n  --headless-session EXECUTABLE Запустить рабочий стол (startxfce4; none — только экран)\n  --no-headless               Отключить автоматический резервный запуск\nРабочий стол выбирается автоматически. Команда settings открывает настройки консоли.\n") \
  X(console_title, u8"CrossDesk 无头控制台", "CrossDesk headless console", u8"Консоль CrossDesk без графического интерфейса") \
  X(console_log_file, u8"日志文件", "Log file", u8"Файл журнала") \
  X(console_help, u8"命令: status 查看 ID/密码/状态；settings 设置；password 修改密码；random 随机密码；help 帮助；quit 退出", "Commands: status — show ID/password/status; settings — configure; password — change password; random — random password; help — help; quit — exit", u8"Команды: status — ID/пароль/состояние; settings — настройки; password — сменить пароль; random — случайный пароль; help — справка; quit — выход") \
  X(console_status, u8"状态", "Status", u8"Состояние") \
  X(console_online, u8"在线", "Online", u8"В сети") \
  X(console_offline, u8"未连接服务器", "Disconnected from server", u8"Нет подключения к серверу") \
  X(console_id, u8"设备 ID", "Device ID", u8"ID устройства") \
  X(console_password, u8"连接密码", "Connection password", u8"Пароль подключения") \
  X(console_waiting, u8"等待分配", "Waiting for assignment", u8"Ожидание назначения") \
  X(console_password_hidden, u8"（仅在交互终端显示）", "(shown only in an interactive terminal)", u8"(виден только в интерактивном терминале)") \
  X(console_cancelled, u8"已取消修改。", "Change cancelled.", u8"Изменение отменено.") \
  X(console_password_invalid, u8"密码必须是6位英文字母或数字，未提交修改。", "Password must be 6 ASCII letters or digits; no change submitted.", u8"Пароль должен содержать 6 латинских букв или цифр. Изменение не отправлено.") \
  X(console_password_offline, u8"尚未连接服务器，暂时无法修改密码。", "Connect to the server before changing the password.", u8"Для смены пароля подключитесь к серверу.") \
  X(console_password_prompt, u8"输入新密码（6位英文字母或数字；输入 cancel 取消）:", "New password (6 ASCII letters or digits; cancel to abort):", u8"Новый пароль (6 латинских букв или цифр; cancel — отмена):") \
  X(console_quitting, u8"正在退出 CrossDesk……", "Exiting CrossDesk...", u8"Выход из CrossDesk...") \
  X(console_unknown_command, u8"未知命令，输入 help 查看帮助。", "Unknown command. Enter help for commands.", u8"Неизвестная команда. Введите help для справки.") \
  X(console_line_too_long, u8"命令过长，已忽略。", "Command too long; ignored.", u8"Команда слишком длинная и проигнорирована.") \
  X(console_random_failed, u8"无法生成随机密码，请重试。", "Unable to generate a random password; try again.", u8"Не удалось создать случайный пароль. Повторите попытку.") \
  X(console_password_pending, u8"密码修改正在处理中，请等待服务器确认。", "Password change pending; wait for server confirmation.", u8"Смена пароля выполняется. Дождитесь подтверждения сервера.") \
  X(console_password_stage_failed, u8"无法保存密码修改记录，未提交修改。请检查日志。", "Unable to save password recovery record; no change submitted. Check the log.", u8"Не удалось сохранить запись восстановления пароля. Изменение не отправлено. Проверьте журнал.") \
  X(console_password_send_failed, u8"发送密码修改请求失败，请等待连接恢复后重试。", "Failed to submit password change; retry after reconnection.", u8"Не удалось отправить запрос смены пароля. Повторите после подключения.") \
  X(console_password_submitted, u8"已提交密码修改，正在等待服务器确认……", "Password change submitted; waiting for server confirmation...", u8"Запрос смены пароля отправлен. Ожидание подтверждения...") \
  X(console_password_uncertain, u8"密码修改结果尚未确认，正在重新连接服务器核实。", "Password change not yet confirmed; reconnecting to verify.", u8"Смена пароля не подтверждена. Повторное подключение для проверки.") \
  X(console_password_rejected, u8"服务器未接受密码修改，原密码保持不变。详情见日志。", "Server rejected the password change; the original password remains. See the log.", u8"Сервер отклонил смену пароля. Старый пароль сохранён. См. журнал.") \
  X(console_password_confirmed, u8"服务器已确认密码修改，正在重新连接。", "Server confirmed the password change; reconnecting.", u8"Сервер подтвердил смену пароля. Повторное подключение.") \
  X(console_password_recovered, u8"重新连接确认：新密码已生效。", "Reconnection confirmed: the new password is active.", u8"Повторное подключение подтвердило новый пароль.") \
  X(console_password_retained, u8"重新连接确认：继续使用原密码。", "Reconnection confirmed: the original password remains active.", u8"Повторное подключение подтвердило прежний пароль.") \
  X(console_exited, u8"CrossDesk 已退出。", "CrossDesk exited.", u8"CrossDesk завершён.") \
  X(console_stopped, u8"CrossDesk 已停止，退出码", "CrossDesk stopped; exit code", u8"CrossDesk остановлен; код выхода") \
  X(console_settings_title, u8"设置（修改后立即保存）", "Settings (each change is saved immediately)", u8"Настройки (изменения сохраняются сразу)") \
  X(console_settings_prompt, u8"输入编号或选项名称修改；back 返回。也可使用 settings set 名称 值。", "Enter an item number or key; back to return. Shortcut: settings set KEY VALUE.", u8"Введите номер или имя параметра; back — назад. Команда: settings set KEY VALUE.") \
  X(console_settings_value, u8"输入新值（cancel 取消）", "Enter a new value (cancel to abort)", u8"Введите новое значение (cancel — отмена)") \
  X(console_settings_unavailable, u8"设置尚未初始化，请稍后重试。", "Settings are not ready; try again shortly.", u8"Настройки ещё не готовы. Повторите позже.") \
  X(console_settings_invalid, u8"无效的选项或值，未保存。请按菜单列出的格式输入。", "Invalid setting or value; not saved. Use the format shown in the menu.", u8"Неверный параметр или значение. Ничего не сохранено. Используйте указанный формат.") \
  X(console_settings_save_failed, u8"保存设置失败，原设置保持不变。请检查日志。", "Failed to save settings; the previous value is retained. Check the log.", u8"Не удалось сохранить настройки. Прежнее значение сохранено. Проверьте журнал.") \
  X(console_settings_busy, u8"有远程会话或密码修改正在进行，暂时不能更改连接设置。", "Connection settings cannot change during a remote session or password change.", u8"Нельзя менять параметры подключения во время удалённого сеанса или смены пароля.") \
  X(console_settings_saved, u8"已保存并生效。", "Saved and applied.", u8"Сохранено и применено.") \
  X(console_settings_unchanged, u8"设置未改变。", "Setting unchanged.", u8"Настройка не изменилась.") \
  X(console_settings_reconnect, u8"已保存，正在重新连接服务器。", "Saved; reconnecting to the server.", u8"Сохранено. Повторное подключение к серверу.") \
  X(console_settings_next_connection, u8"已保存，将用于后续连接。", "Saved; applies to subsequent connections.", u8"Сохранено. Будет применено к следующим подключениям.") \
  X(console_settings_next_file, u8"已保存，将用于后续接收的文件。", "Saved; applies to subsequently received files.", u8"Сохранено. Будет применено к следующим получаемым файлам.") \
  X(console_settings_autostart, u8"已保存，在下次图形桌面登录时生效。", "Saved; applies at the next graphical desktop login.", u8"Сохранено. Будет применено при следующем входе в графический сеанс.") \
  X(console_settings_daemon, u8"已保存，普通模式重启后生效；无头控制台仍保持前台运行。", "Saved for the next normal-mode start; the headless console stays in the foreground.", u8"Сохранено для следующего запуска в обычном режиме. Консоль без графического интерфейса остаётся на переднем плане.") \
  X(console_settings_server_required, u8"请先设置有效的服务器地址和端口，再启用自托管。", "Set a valid server host and port before enabling self-hosting.", u8"Укажите адрес и порт сервера перед включением собственного сервера.") \
  X(console_settings_path_invalid, u8"请选择已存在且可写的绝对目录路径；default 使用默认目录。", "Use an existing writable absolute directory, or default for the default location.", u8"Укажите существующий доступный для записи абсолютный путь к каталогу или default.") \
  X(console_settings_readonly, u8"该选项由系统或控制端管理，不可在此修改。", "This option is managed by the system or controller and is read-only here.", u8"Этот параметр управляется системой или клиентом и здесь недоступен для изменения.") \
  X(console_setting_language, u8"语言", "Language", u8"Язык") \
  X(console_setting_codec, u8"视频编码", "Video codec", u8"Видеокодек") \
  X(console_setting_turn, u8"中继模式", "TURN relay mode", u8"Режим ретрансляции TURN") \
  X(console_setting_self_hosted, u8"自托管", "Self-hosted server", u8"Собственный сервер") \
  X(console_setting_host, u8"服务器地址", "Server host", u8"Адрес сервера") \
  X(console_setting_port, u8"信令端口", "Signaling port", u8"Порт сигнализации") \
  X(console_setting_privacy, u8"连接时自动开启隐私屏", "Privacy screen on connection", u8"Приватный экран при подключении") \
  X(console_setting_files, u8"文件保存目录", "Received file directory", u8"Каталог получаемых файлов") \
  X(console_setting_autostart, u8"登录桌面时自启", "Start at desktop login", u8"Запуск при входе в систему") \
  X(console_setting_daemon, u8"守护进程（无头控制台中不生效）", "Daemon (inactive in headless console)", u8"Фоновый режим (не действует в консоли)") \
  X(save_remote_previews, u8"保存远程画面预览:", "Save remote screen previews:", u8"Сохранять превью удалённого экрана:") \
  X(unknown_platform, u8"未知平台", "Unknown platform", u8"Неизвестная платформа") \
  X(privacy_screen, u8"隐私屏", "Privacy screen", u8"Приватный экран") \
  X(privacy_enable, u8"开启隐私屏", "Enable privacy screen", u8"Включить приватный экран") \
  X(privacy_disable, u8"关闭隐私屏", "Disable privacy screen", u8"Выключить приватный экран") \
  X(privacy_on, u8"隐私屏：已开启", "Privacy screen: on", u8"Приватный экран: включён") \
  X(privacy_off, u8"隐私屏：已关闭", "Privacy screen: off", u8"Приватный экран: выключен") \
  X(privacy_unsupported, u8"隐私屏：不可用", "Privacy screen: unavailable", u8"Приватный экран: недоступен") \
  X(local_desktop, u8"本桌面", "Local Desktop", u8"Локальный рабочий стол")    \
  X(local_id, u8"本机 ID", "Local ID", u8"Локальный ID")                       \
  X(local_id_copied_to_clipboard, u8"已复制到剪贴板", "Copied to clipboard",   \
    u8"Скопировано в буфер обмена")                                            \
  X(password, u8"密码", "Password", u8"Пароль")                                \
  X(refresh_password, u8"刷新随机密码", "Refresh random password",            \
    u8"Обновить случайный пароль")                                           \
  X(password_format_invalid, u8"密码必须为 6 位英文字母或数字。", "Use exactly 6 letters (A–Z, a–z) or digits (0–9).", u8"Пароль должен содержать ровно 6 латинских букв или цифр.")    \
  X(remote_desktop, u8"远程桌面", "Remote Desktop",                            \
    u8"Удаленный рабочий стол")                                                \
  X(device_name, u8"设备名称", "Device Name", u8"Имя устройства")             \
  X(remote_id, u8"对端 ID", "Remote ID", u8"Удаленный ID")                     \
  X(connect, u8"连接", "Connect", u8"Подключиться")                            \
  X(recent_connections, u8"近期连接", "Recent Connections",                    \
    u8"Недавние подключения")                                                  \
  X(disconnect, u8"断开连接", "Disconnect", u8"Отключить")                     \
  X(stream_confirm_disconnect, u8"断开当前连接？", "Disconnect this session?", \
    u8"Отключить текущий сеанс?")                                              \
  X(stream_disconnect_description, u8"当前远程控制会话将结束。",               \
    "The current remote session will end.",                                  \
    u8"Текущий сеанс удалённого управления будет завершён.")                   \
  X(select_display, u8"选择显示器", "Select Display", u8"Выбрать дисплей")     \
  X(display_screen, u8"显示屏", "Display", u8"Экран")                        \
  X(expand_control_bar, u8"展开控制栏", "Expand Control Bar",                  \
    u8"Развернуть панель управления")                                          \
  X(collapse_control_bar, u8"收起控制栏", "Collapse Control Bar",              \
    u8"Свернуть панель управления")                                            \
  X(fullscreen, u8"全屏", "Fullscreen", u8"На весь экран")                     \
  X(show_net_traffic_stats, u8"显示网络状态", "Show network status", u8"Показать состояние сети")                                           \
  X(hide_net_traffic_stats, u8"隐藏网络状态", "Hide network status", u8"Скрыть состояние сети")                                             \
  X(video, u8"视频", "Video", u8"Видео")                                       \
  X(audio, u8"音频", "Audio", u8"Аудио")                                       \
  X(data, u8"数据", "Data", u8"Данные")                                        \
  X(total, u8"总计", "Total", u8"Итого")                                       \
  X(in, u8"输入", "In", u8"Вход")                                              \
  X(out, u8"输出", "Out", u8"Выход")                                           \
  X(loss_rate, u8"丢包率", "Loss Rate", u8"Потери пакетов")                    \
  X(exit_fullscreen, u8"退出全屏", "Exit fullscreen",                          \
    u8"Выйти из полноэкранного режима")                                        \
  X(control_mouse, u8"控制鼠标", "Control Mouse", u8"Управление мышью")        \
  X(release_mouse, u8"释放鼠标", "Release Mouse", u8"Освободить мышь")         \
  X(audio_capture, u8"播放声音", "Play audio", u8"Воспроизведение звука")   \
  X(mute, u8"静音", "Mute", u8"Без звука")                                   \
  X(send_shortcut, u8"发送组合键", "Send Shortcut", u8"Отправить сочетание клавиш")      \
  X(send_sas, u8"发送 SAS", "Send SAS", u8"Отправить SAS")                     \
  X(lock_remote, u8"锁定远端", "Lock Remote", u8"Заблокировать удалённое устройство")               \
  X(remote_password_box_visible, u8"远端密码框已出现", "Remote password box visible", u8"На удалённом устройстве появилось поле пароля")                \
  X(remote_lock_screen_hint, u8"远端处于锁屏封面，可发送 SAS", "Remote lock screen visible, send SAS", u8"Удалённый экран заблокирован, можно отправить SAS")                                       \
  X(remote_secure_desktop_active, u8"远端已进入安全桌面", "Remote secure desktop active", u8"На удалённом устройстве активен защищённый рабочий стол")       \
  X(remote_service_unavailable, u8"远端 Windows 服务不可用",                   \
    "Remote Windows service unavailable",                                      \
    u8"Служба Windows на удаленной стороне недоступна")                        \
  X(windows_service_setup_title, u8"安装 CrossDesk Service",                   \
    "Install CrossDesk Service", u8"Установить CrossDesk Service")             \
  X(windows_service_setup_message,                                             \
    u8"为支持该设备在锁屏状态下被远程控制，需要以管理员权限安装 CrossDesk "    \
    u8"Service。\n未安装该服务不影响 CrossDesk "                               \
    u8"正常使用，仅无法在锁屏状态下控制本机。",                                \
    "To support remote control of this device while it is locked, CrossDesk "  \
    "Service must be installed with administrator permission.\nWithout this "  \
    "service, CrossDesk still works normally; only lock-screen control of "    \
    "this computer is unavailable.",                                           \
    u8"Чтобы поддерживать удаленное управление этим устройством на экране "    \
    u8"блокировки, необходимо установить CrossDesk Service с правами "         \
    u8"администратора.\nБез этой службы CrossDesk продолжит работать "         \
    u8"нормально; будет недоступно только управление этим компьютером на "     \
    u8"экране блокировки.")                                                    \
  X(install_windows_service, u8"安装", "Install", u8"Установить")              \
  X(windows_service_settings_label, u8"锁屏控制服务:", "Lock-screen control service:", u8"Служба управления при блокировке:")                     \
  X(windows_service_installed, u8"已安装", "Installed", u8"Установлена")       \
  X(do_not_remind_again, u8"不再提醒", "Do not remind again",                  \
    u8"Больше не напоминать")                                                  \
  X(windows_service_prompt_suppressed_message,                                 \
    u8"已不再提醒。后续如需启用锁屏状态下被远程控制，可在设置中点击“安装”。",  \
    "You will not be reminded again. To enable remote control while locked "   \
    "later, click Install in Settings.",                                       \
    u8"Напоминание отключено. Чтобы позже включить удаленное управление на "   \
    u8"экране блокировки, нажмите «Установить» в настройках.")                 \
  X(installing_windows_service, u8"正在安装服务...", "Installing service...",  \
    u8"Установка службы...")                                                   \
  X(windows_service_install_success, u8"服务已安装并启动",                     \
    "Service installed and started", u8"Служба установлена и запущена")        \
  X(windows_service_install_failed,                                            \
    u8"服务安装失败。请确认 "                                                  \
    u8"CrossDesk.exe、crossdesk_service.exe、crossdesk_session_helper.exe "    \
    u8"位于同一便携目录中，并在系统弹窗中允许管理员权限。",                    \
    "Service installation failed. Make sure CrossDesk.exe, "                   \
    "crossdesk_service.exe, and crossdesk_session_helper.exe are in the same " \
    "portable folder, then approve the administrator prompt.",                 \
    u8"Не удалось установить службу. Убедитесь, что CrossDesk.exe, "           \
    u8"crossdesk_service.exe и crossdesk_session_helper.exe находятся в "      \
    u8"одной папке портативной версии, затем подтвердите запрос прав "         \
    u8"администратора.")                                                       \
  X(remote_unlock_requires_secure_desktop,                                     \
    u8"当前仍需要安全桌面专用采集/输入",                                       \
    "Secure desktop capture/input is still required",                          \
    u8"По-прежнему нужен отдельный захват/ввод для защищенного рабочего "      \
    u8"стола")                                                                 \
  X(settings, u8"设置", "Settings", u8"Настройки")                             \
  X(language, u8"语言:", "Language:", u8"Язык:")                               \
  X(screen_capture_method, u8"采集方式:", "Capture Method:",                  \
    u8"Способ захвата:")                                                      \
  X(screen_capture_method_auto, u8"自动", "Auto", u8"Авто")                 \
  X(video_settings, u8"画面设置", "Video settings", u8"Настройки видео") \
  X(remote_update_title, u8"建议升级被控端", "Remote update recommended", u8"Рекомендуем обновить удалённое устройство") \
  X(remote_update, u8"新版本包含功能改进和性能优化，建议将被控端升级至最新版本。", "Update the remote device to the latest version for improved features and performance.", u8"В новой версии улучшены функции и производительность. Рекомендуем обновить CrossDesk на удалённом устройстве до последней версии.") \
  X(remote_update_dismiss, u8"知道了", "Got it", u8"Понятно") \
  X(video_settings_pending, u8"正在应用…", "Applying…", u8"Применение…") \
  X(video_settings_failed, u8"调整失败，请重试。", "Could not apply settings. Try again.", u8"Не удалось применить настройки. Повторите попытку.") \
  X(video_settings_unavailable, u8"连接建立后可调整，需被控端支持。", "Available when connected to a supported remote device.", u8"Доступно при подключении к поддерживаемому устройству.") \
  X(video_quality, u8"画面质量:", "Video Quality:", u8"Качество видео:")       \
  X(video_frame_rate, u8"画面采集帧率:",                                       \
    "Video Capture Frame Rate:", u8"Частота захвата видео:")                   \
  X(video_adaptation_policy, u8"画面偏好:", "Video Preference:",              \
    u8"Режим видео:")                                                           \
  X(video_priority_frame_rate, u8"帧率优先", "Frame Rate",                   \
    u8"Частота кадров")                                                         \
  X(video_priority_quality, u8"画质优先", "Quality", u8"Качество")          \
  X(video_priority_balanced, u8"平衡", "Balanced", u8"Баланс")              \
  X(video_quality_high, u8"高", "High", u8"Высокое")                           \
  X(video_quality_medium, u8"中", "Medium", u8"Среднее")                       \
  X(video_quality_low, u8"低", "Low", u8"Низкое")                              \
  X(video_encode_format, u8"视频编码格式:", "Video codec:", u8"Видеокодек:")                          \
  X(av1, u8"AV1", "AV1", "AV1")                                                \
  X(h264, u8"H.264", "H.264", "H.264")                                         \
  X(force_relay, u8"强制中继连接:", "Force relay connection:", u8"Только через ретранслятор:")   \
  X(self_hosted_server_config, u8"自托管配置", "Self-hosted server", u8"Собственный сервер")                                              \
  X(self_hosted_server_settings, u8"自托管设置", "Self-hosted server settings", u8"Настройки своего сервера")                                                 \
  X(self_hosted_server_address, u8"服务器地址:",                               \
    "Server Address:", u8"Адрес сервера:")                                     \
  X(self_hosted_server_port, u8"信令服务端口:", "Signaling port:", u8"Порт сигнализации:")                     \
  X(self_hosted_address_invalid, u8"请输入有效的主机名、IP 或 HTTPS/WSS 信令地址，可包含路径。", "Enter a valid host, IP, or HTTPS/WSS signaling URL. Paths are supported.", u8"Введите имя хоста, IP или HTTPS/WSS-адрес сигнального сервиса. Путь допустим.") \
  X(self_hosted_port_invalid, u8"请输入 1–65535 范围内的信令端口。", "Enter a signaling port between 1 and 65535.", u8"Введите порт сигнального сервиса от 1 до 65535.") \
  X(self_hosted_save_failed, u8"配置保存失败，请检查配置目录是否可写后重试。", "Could not save settings. Check that the configuration directory is writable and retry.", u8"Не удалось сохранить настройки. Проверьте доступ на запись в каталог конфигурации и повторите попытку.") \
  X(ok, u8"确认", "OK", u8"ОК")                                                \
  X(cancel, u8"取消", "Cancel", u8"Отмена")                                    \
  X(new_password, u8"请输入六位密码:", "Enter a six-character password:", u8"Введите пароль из 6 символов:")     \
  X(input_password, u8"请输入密码:",                                           \
    "Please input password:", u8"Введите пароль:")                             \
  X(validate_password, u8"验证密码中...", "Validating password...", u8"Проверка пароля...")                                                    \
  X(reinput_password, u8"请重新输入密码", "Please input password again",       \
    u8"Повторно введите пароль")                                               \
  X(remember_password, u8"记住密码", "Remember password",                      \
    u8"Запомнить пароль")                                                      \
  X(signal_connected, u8"已连接服务器", "Connected to server", u8"Подключено к серверу") \
  X(signal_disconnected, u8"未连接服务器", "Disconnected from server", u8"Нет подключения к серверу")                                             \
  X(signal_reconnect, u8"重新连接", "Reconnect", u8"Переподключиться")             \
  X(signal_reconnecting, u8"正在重新连接...", "Reconnecting...",               \
    u8"Переподключение...")                                                    \
  X(signal_tls_cert_error, u8"证书验证失败，请重新安装自托管根证书", "Certificate verification failed. Reinstall the self-hosted root certificate.", u8"Ошибка проверки сертификата. Переустановите корневой сертификат собственного сервера.")      \
  X(p2p_connected, u8"对等连接已建立", "P2P Connected", u8"P2P подключено")    \
  X(p2p_disconnected, u8"对等连接已断开", "P2P Disconnected",                  \
    u8"P2P отключено")                                                         \
  X(p2p_connecting, u8"正在建立对等连接...", "P2P Connecting ...",             \
    u8"Подключение P2P...")                                                    \
  X(p2p_gathering, u8"正在收集候选地址...", "Gathering candidates ...",         \
    u8"Сбор кандидатов...")                                                    \
  X(receiving_screen, u8"画面接收中...", "Receiving screen...",                \
    u8"Получение изображения...")                                              \
  X(p2p_failed, u8"对等连接失败", "P2P Failed", u8"Сбой P2P")                  \
  X(connection_auth_throttled, u8"验证尝试过于频繁，请稍后重试。", "Too many authentication attempts. Try again later.", u8"Слишком много попыток аутентификации. Повторите позже.") \
  X(connection_service_busy, u8"认证服务繁忙，请稍后重试。", "Authentication service busy. Try again later.", u8"Сервис проверки занят. Повторите позже.") \
  X(connection_not_authenticated, u8"登录状态已失效，请重新连接。", "Signaling login expired. Please reconnect.", u8"Сеанс входа истёк. Подключитесь заново.") \
  X(connection_retry_after, u8"请在 %d 秒后重试。", "Retry in %d seconds.", u8"Повторите через %d сек.") \
  X(p2p_closed, u8"对等连接已关闭", "P2P closed", u8"P2P закрыто")             \
  X(no_such_id, u8"无此 ID", "No such ID", u8"ID не найден")                   \
  X(about, u8"关于", "About", u8"О программе")                                 \
  X(sponsorship_title, u8"赞助支持", "Sponsorship", u8"Поддержка проекта")    \
  X(sponsorship_message, u8"如果 CrossDesk 对你有帮助，欢迎赞助支持项目的持续开发、跨平台测试与日常维护。感谢你的支持！", \
    "If CrossDesk is useful to you, consider supporting its continued development, cross-platform testing, and maintenance. Thank you for your support!", \
    u8"Если CrossDesk вам полезен, поддержите его дальнейшую разработку, тестирование на разных платформах и сопровождение. Спасибо за вашу поддержку!") \
  X(sponsorship_dismiss_hint, u8"关闭后本版本不再提示，升级后再次显示。赞助入口仍在「关于」中。", \
    "Once closed, this prompt stays hidden until the version changes. Sponsorship links remain available in About.", \
    u8"После закрытия это окно появится только при смене версии. Ссылки для поддержки останутся в разделе «О программе».") \
  X(sponsor_via_afdian, u8"爱发电", "AFDIAN", "AFDIAN")                       \
  X(notification, u8"通知", "Notification", u8"Уведомление")                   \
  X(new_version, u8"新版本", "New version", u8"Новая версия")                 \
  X(new_version_available, u8"新版本可用", "New Version Available",            \
    u8"Доступна новая версия")                                                 \
  X(check_for_updates, u8"检查更新", "Check for updates",                     \
    u8"Проверить обновления")                                                   \
  X(checking_for_updates, u8"正在检查更新…", "Checking for updates…",         \
    u8"Проверка обновлений…")                                                  \
  X(up_to_date, u8"已是最新版本", "You're up to date",                         \
    u8"Установлена последняя версия")                                          \
  X(update_check_failed, u8"检查失败，请稍后重试",                             \
    "Check failed. Please try again later.",                                    \
    u8"Не удалось проверить обновления. Повторите позже.")                     \
  X(release_notes, u8"更新内容", "Release Notes", u8"Содержание обновления") \
  X(view_release_notes, u8"更新日志", "Release notes", u8"История изменений") \
  X(version, u8"版本", "Version", u8"Версия")                                  \
  X(release_date, u8"发布日期: ", "Release Date: ", u8"Дата релиза: ")         \
  X(access_website, u8"访问官网: ",                                            \
    "Access Website: ", u8"Официальный сайт: ")                                \
  X(update, u8"更新", "Update", u8"Обновить")                                  \
  X(install_update, u8"安装更新", "Install update", u8"Установить обновление") \
  X(manual_download, u8"手动下载", "Download manually", u8"Скачать вручную") \
  X(install_update_confirm, u8"下载完成后将关闭 CrossDesk 并安装更新，当前连接和文件传输会中断。安装需要系统授权，完成后重新启动。是否继续？", \
    "CrossDesk will close after downloading the update. Connections and file transfers will be interrupted. Installation requires system authorization, then CrossDesk will restart. Continue?", \
    u8"После загрузки CrossDesk закроется для обновления. Соединения и передача файлов будут прерваны. Потребуется разрешение системы, затем приложение перезапустится. Продолжить?") \
  X(update_preparing, u8"正在准备更新…", "Preparing update…", u8"Подготовка обновления…") \
  X(update_install_failed, u8"更新未完成，可重试或手动安装。", "Update did not complete. Retry or install manually.", u8"Обновление не завершено. Повторите или установите вручную.") \
  X(connection_alias, u8"修改名称", "Rename", u8"Переименовать")                                              \
  X(delete_connection, u8"删除连接", "Delete Connection",                      \
    u8"Удалить подключение")                                                   \
  X(connect_to_this_connection, u8"发起连接", "Connect", u8"Подключиться")                                                          \
  X(input_connection_alias, u8"请输入连接名称:",                               \
    "Please input connection name:", u8"Введите имя подключения:")             \
  X(confirm_delete_connection, u8"确认删除此连接", "Delete this connection?", u8"Удалить это подключение?")         \
  X(enable_autostart, u8"开机自启:", "Auto Start:", u8"Автозапуск:")           \
  X(enable_daemon, u8"启用守护进程:", "Enable Daemon:", u8"Включить демон:")   \
  X(privacy_on_connect, u8"被连接时开启隐私屏:", "Privacy screen on incoming connection:", u8"Приватный экран при подключении:")                                       \
  X(lock_on_disconnect, u8"连接断开后锁屏:", "Lock screen on disconnect:", \
    u8"Блокировка при отключении:")                                         \
  X(takes_effect_after_restart, u8"重启后生效", "Takes effect after restart",  \
    u8"Вступит в силу после перезапуска")                                      \
  X(select_file, u8"选择文件发送", "Select File to Send",                      \
    u8"Выбрать файл для отправки")                                             \
  X(file_transfer_progress, u8"文件传输进度", "File Transfer Progress",        \
    u8"Прогресс передачи файлов")                                              \
  X(file_sending_progress, u8"文件发送进度", "File Sending Progress",         \
    u8"Отправка файла")                                                       \
  X(file_receiving_progress, u8"文件接收进度", "File Receiving Progress",     \
    u8"Получение файла")                                                      \
  X(queued, u8"队列中", "Queued", u8"В очереди")                               \
  X(sending, u8"正在传输", "Sending", u8"Передача")                            \
  X(receiving, u8"正在接收", "Receiving", u8"Получение")                      \
  X(completed, u8"已完成", "Completed", u8"Завершено")                         \
  X(failed, u8"失败", "Failed", u8"Ошибка")                                    \
  X(cancelling, u8"正在取消", "Cancelling", u8"Отмена…")                       \
  X(cancelled, u8"已取消", "Cancelled", u8"Отменено")                          \
  X(controller, u8"控制端:", "Controller:", u8"Контроллер:")                   \
  X(server_controlled, u8"被控中", "Controlled", u8"Под управлением") \
  X(server_heading, u8"此设备被控中", "This device is controlled", u8"Устройство под управлением") \
  X(server_connected, u8"已连接", "Connected", u8"Подключено") \
  X(server_send_file, u8"发送文件", "Send file", u8"Отправить файл") \
  X(server_disconnect, u8"断开", "Disconnect", u8"Отключить") \
  X(server_disconnect_all, u8"全部断开", "Disconnect all", u8"Отключить всех") \
  X(server_confirm_disconnect, u8"断开所有控制端的连接？", "Disconnect all controllers?", u8"Отключить все контроллеры?") \
  X(file_transfer, u8"文件传输:", "File Transfer:", u8"Передача файлов:")      \
  X(connection_status, u8"连接状态:",                                          \
    "Connection Status:", u8"Состояние соединения:")                           \
  X(file_transfer_save_path, u8"文件接收保存路径:",                            \
    "File Transfer Save Path:", u8"Путь сохранения файлов:")                   \
  X(default_desktop, u8"桌面", "Desktop", u8"Рабочий стол")                    \
  X(resolution, u8"分辨率", "Res", u8"Разрешение")                             \
  X(video_latency, u8"视频延时", "Video latency", u8"Задержка видео") \
  X(connection_latency, u8"连接延时", "Connection latency", u8"Задержка соединения") \
  X(connection_mode, u8"连接模式", "Connection mode", u8"Режим подключения")                          \
  X(connection_mode_direct, u8"直连", "Direct", u8"Прямой")                    \
  X(connection_mode_relay, u8"中继", "Relay", u8"Ретрансляция")                    \
  X(transport_encryption, u8"加密传输", "Encryption", u8"Шифрование")         \
  X(remote_video_encoder, u8"被控端编码器", "Remote encoder", u8"Кодер удалённой стороны") \
  X(local_video_decoder, u8"控制端解码器", "Local decoder", u8"Локальный декодер") \
  X(transport_encryption_enabled, u8"已开启", "Enabled", u8"Включено")         \
  X(transport_encryption_disabled, u8"未开启", "Disabled", u8"Выключено")       \
  X(online, u8"在线", "Online", u8"В сети")                                    \
  X(offline, u8"离线", "Offline", u8"Не в сети")                                  \
  X(device_offline, u8"设备离线", "Device Offline", u8"Устройство не в сети")     \
  X(request_permissions, u8"权限请求", "Request Permissions",                  \
    u8"Запрос разрешений")                                                     \
  X(screen_recording_permission, u8"屏幕录制权限",                             \
    "Screen Recording Permission", u8"Разрешение на запись экрана")            \
  X(accessibility_permission, u8"辅助功能权限", "Accessibility Permission",    \
    u8"Разрешение специальных возможностей")                                   \
  X(permission_required_message, u8"该应用需要授权以下权限:",                  \
    "The application requires the following permissions:",                     \
    u8"Для работы приложения требуются следующие разрешения:")                 \
  X(wayland_notice_title, u8"Wayland 会话提示", "Wayland Session Notice",        \
    u8"Уведомление о сеансе Wayland")                                          \
  X(wayland_notice_subtitle, u8"检测到当前桌面会话使用 Wayland。",               \
    "The current desktop session uses Wayland.",                              \
    u8"Текущий сеанс рабочего стола использует Wayland.")                      \
  X(wayland_permission_title, u8"每次共享均需本机授权",                        \
    "Local approval required each time",                                      \
    u8"Разрешение требуется каждый раз")                                      \
  X(wayland_notice_message,                                                    \
    u8"受 Wayland 安全机制限制，每次启动远程桌面共享时，"                      \
    u8"均需在本机手动授权屏幕共享与远程控制，授权后方可正常使用。",             \
    "Wayland security restrictions require you to authorize screen sharing "  \
    "and remote control locally each time you start sharing this desktop. "   \
    "Remote access is available only after authorization.",                    \
    u8"Из-за ограничений безопасности Wayland при каждом запуске общего "       \
    u8"доступа к рабочему столу необходимо вручную разрешать на этом "           \
    u8"компьютере показ экрана и удалённое управление. Удалённый доступ "        \
    u8"возможен только после разрешения.")                                     \
  X(wayland_recommendation_title, u8"建议使用 X11（Xorg）",                      \
    "X11 (Xorg) is recommended", u8"Рекомендуем X11 (Xorg)")                    \
  X(wayland_recommendation_message,                                            \
    u8"为便于持续使用远程桌面，建议注销当前桌面会话，"                        \
    u8"在登录界面选择 X11（Xorg）会话后重新登录。",                            \
    "For ongoing remote desktop access, we recommend logging out, selecting " \
    "an X11 (Xorg) session on the login screen, and logging in again.",         \
    u8"Для постоянного удалённого доступа рекомендуем завершить сеанс, "        \
    u8"выбрать X11 (Xorg) на экране входа и войти снова.")                      \
  X(wayland_notice_acknowledge, u8"我知道了", "Got it", u8"Понятно")            \
  X(show_main_window, u8"显示主界面", "Show Main Window",                    \
    u8"Показать главное окно")                                                \
  X(privacy_local_unlock, u8"允许被控时解锁隐私屏:", "Allow privacy screen unlock while controlled:", u8"Разрешить разблокировку приватного экрана при удалённом управлении:") \
  X(privacy_screen_locked_hint, u8"隐私屏已开启，本机解锁已禁用", "Privacy screen is on. Local unlocking is disabled.", u8"Приватный экран включён. Локальная разблокировка отключена.") \
  X(privacy_unlocked_title, u8"隐私屏已被解锁", "Privacy screen unlocked", u8"Приватный экран разблокирован") \
  X(privacy_unlocked_message, u8"被控端已通过组合键解除隐私屏。点击“锁定”将重新开启隐私屏，并禁止本机使用组合键解锁。", "The remote privacy screen was unlocked using the local shortcut. Lock it again to restore privacy and disable local shortcut unlocking.", u8"Приватный экран отключён локальным сочетанием клавиш. Заблокируйте его снова, чтобы восстановить защиту и запретить локальную разблокировку.") \
  X(privacy_unlock_confirm, u8"确认", "Confirm", u8"Подтвердить") \
  X(privacy_unlock_lock, u8"锁定", "Lock", u8"Заблокировать") \
  X(privacy_screen_unlock_hint, u8"按下快捷键，解除隐私屏",                    \
    "To turn off the privacy screen, press",                                 \
    u8"Чтобы отключить приватный экран, нажмите")                             \
  X(exit_program, u8"退出", "Exit", u8"Выход")

inline constexpr TranslationRow kTranslationRows[] = {
#define CROSSDESK_DECLARE_TRANSLATION_ROW(name, zh, en, ru) {#name, zh, en, ru},
    CROSSDESK_LOCALIZATION_ALL(CROSSDESK_DECLARE_TRANSLATION_ROW)
#undef CROSSDESK_DECLARE_TRANSLATION_ROW
};

}  // namespace detail

}  // namespace localization
}  // namespace crossdesk

#endif
