#!/bin/bash
set -e

SERVER_IP="$1"
BINARY_URL="$2"  # GitHub release 二进制文件URL

if [ -z "$SERVER_IP" ]; then
    echo "Usage: $0 <server_ip> [binary_url]"
    echo "Example: $0 192.168.1.100"
    exit 1
fi

# 检测架构
ARCH=$(ssh root@$SERVER_IP 'uname -m')
echo "Target architecture: $ARCH"

# 根据架构选择二进制文件
case $ARCH in
    "x86_64")
        BINARY="wol-server-linux-x64"
        ;;
    "aarch64")
        BINARY="wol-server-linux-arm64"
        ;;
    "armv7l")
        BINARY="wol-server-linux-armv7"
        ;;
    "armv6l")
        BINARY="wol-server-linux-armv6"
        ;;
    *)
        echo "Unsupported architecture: $ARCH"
        exit 1
        ;;
esac

echo "Using binary: $BINARY"

# 如果提供了URL，下载二进制文件
if [ -n "$BINARY_URL" ]; then
    echo "Downloading binary from: $BINARY_URL"
    wget -O $BINARY "$BINARY_URL"
else
    echo "Please provide binary URL or ensure $BINARY exists locally"
    exit 1
fi

# 上传到服务器
echo "Uploading to server..."
scp $BINARY root@$SERVER_IP:/tmp/

# 部署
ssh root@$SERVER_IP << EOF
set -e

# 创建目录
mkdir -p /opt/wol-server

# 移动二进制文件
mv /tmp/$BINARY /opt/wol-server/wol-server
chmod +x /opt/wol-server/wol-server

# 创建systemd服务
cat > /etc/systemd/system/wol-server.service << 'SERVICE_EOF'
[Unit]
Description=Wake-on-LAN Server
After=network.target

[Service]
Type=simple
ExecStart=/opt/wol-server/wol-server
WorkingDirectory=/opt/wol-server
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal

[Install]
WantedBy=multi-user.target
SERVICE_EOF

# 启动服务
systemctl daemon-reload
systemctl enable wol-server
systemctl start wol-server

# 防火墙设置
if command -v ufw >/dev/null 2>&1; then
    ufw allow 8044/tcp
    ufw reload
elif command -v firewall-cmd >/dev/null 2>&1; then
    firewall-cmd --permanent --add-port=8044/tcp
    firewall-cmd --reload
fi

echo "Deployment complete!"
echo "WoL Server running on: http://\$(hostname -I | awk '{print \$1}'):8044"
EOF

echo "Deployment to $SERVER_IP completed!"
