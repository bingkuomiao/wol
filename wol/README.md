## 对应架构的可执行文件重新命名为wol放入wol目录下
### 定时任务
每10分钟检查一次 wol 进程是否在运行，如果该进程不存在，就执行 /data/wol/wol_update 脚本，并且不保留任何输出日志
*/10 * * * * test -z "$(pidof wol)" && /data/wol/wol_update >/dev/null 2>&1
### 定时任务添加路径
cat /etc/crontabs/root 
vi /etc/crontabs/root
### 服务启动停止
cat /etc/init.d/wol
/etc/init.d/wol reload
/etc/init.d/wol stop
### WOL格式：
http://192.168.31.1:8044/wol?mac=b6:6f:9c:cc:d7:99&ip=192.168.31.255
http://192.168.31.1:8044/wol?mac=b6-6f-9c-cc-d7-99&ip=192.168.31.255
### 代码仓库:
https://github.com/bingkuomiao/wol
### 抓包:
tcpdump -i any -n -X udp port 9
### 业务操作：
/etc/init.d/wol start
/etc/init.d/wol stop
/etc/init.d/wol reload
