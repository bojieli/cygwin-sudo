cygwin-sudo 是一个以管理员权限运行程序的工具。它需要以管理员权限启动一个 sudo-server，然后在以普通权限运行的 shell 中即可使用 sudo 提权。


用法
----

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


技术原理
--------

首先尝试的是 ShellExecute WinAPI（在uac.cpp中），倒是能弹出UAC对话框，不过只能用Windows的默认shell打开新窗口，无法在原cygwin窗口中输出。

目前采用类似 sshd 的方法，sudo-server 监听 12345 端口并以管理员权限运行。

sudo 启动 raw 终端模式，将命令行参数、环境变量、终端参数、窗口大小传给 sudo-server，server forkpty 并对所执行程序的输入输出做转发，这样 sudo 所在的终端看到的就是以管理员权限运行的 pty。

当窗口大小变化时，sudo 截获信号并发给 sudo-server，server 再通知 pty。
