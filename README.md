用法：

1. 编译
 - make
 - cp sudo-server.exe /usr/local/bin/
 - cp sudo.exe /usr/local/bin/

2. 开机自动运行
 - 打开“任务计划” (Task Scheduler)
 - 新建任务
 - 在“常规”选项卡勾选“使用最高权限运行”
 - 在“触发器”选项卡添加“登录时触发”
 - 在“操作”选项卡添加“启动程序”，填写 sudo-server.exe 的路径

3. 使用：在 cygwin 中输入 sudo command 即可以管理员权限执行此命令，直接输入 sudo 即可进入管理员权限的 shell。


首先尝试的是ShellExecute WinAPI（在uac.cpp中），倒是能弹出UAC对话框，不过只能用Windows的默认shell打开新窗口，无法在原cygwin窗口中输出。
