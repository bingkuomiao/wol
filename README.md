# WoL Server - ARM/x64 Wake-on-LAN Server

专为 ARM 和 x64 架构设计的轻量级网络唤醒服务器。

## 🎯 支持的平台

- **Linux x86_64** - 标准 64 位 Linux
- **Linux ARM64** - ARMv8 / aarch64 设备
- **Linux ARMv7** - ARMv7 设备（树莓派等）

## 🚀 快速开始

### 自动部署
```bash
./deploy.sh 你的服务器IP
# 下载对应架构的二进制文件
chmod +x wol-server*
./wol-server*
```
###  Web 界面
访问 http://你的服务器IP:8044
###  API 调用
curl "http://你的服务器IP:8044/wol?mac=b6-6f-9c-cc-d7-99&ip=192.168.31.255"

###  本地编译
gcc -std=c99 -O2 -static -o wol-server wol_server.c

### 抓包
tcpdump -i any -n -X udp port 9

### 自定义端口
# 使用默认端口
./wol_server*

# 使用指定端口
./wol_server* -p 8044
./wol_server* --port 8044

# 显示帮助信息
./wol_server* -h
./wol_server* --help
