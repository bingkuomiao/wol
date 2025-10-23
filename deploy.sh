#!/bin/bash
set -e

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 显示用法
show_usage() {
    echo "Usage: $0 <server_ip> [github_release_url]"
    echo "Example: $0 192.168.1.100"
    echo "Example: $0 192.168.1.100 \"https://github.com/user/repo/releases/latest/download\""
    exit 1
}

# 参数检查
if [ $# -lt 1 ]; then
    show_usage
fi

SERVER_IP="$1"
RELEASE_URL="$2"

# 如果没有提供Release URL，使用默认值
if [ -z "$RELEASE_URL" ]; then
    RELEASE_URL="https://github.com/$(git remote get-url origin | sed 's/.*github.com[:/]\(.*\)\.git/\1/')/releases/latest/download"
    echo -e "${YELLOW}ℹ️  Using default release URL: $RELEASE_URL${NC}"
fi

echo -e "${GREEN}🚀 Starting WoL Server deployment...${NC}"

# 检测目标架构
echo -e "${YELLOW}🔍 Detecting target architecture...${NC}"
ARCH=$(ssh root@$SERVER_IP 'uname -m')
echo -e "Target architecture: ${GREEN}$ARCH${NC}"

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
    *)
        echo -e "${RED}❌ Unsupported architecture: $ARCH${NC}"
        echo "Supported architectures: x86_64, aarch64, armv7l"
        exit 1
        ;;
esac

BINARY_URL="$RELEASE_URL/$BINARY"
echo -e "Selected binary: ${GREEN}$BINARY${NC}"
echo -e "Download URL: ${YELLOW}$BINARY_URL${NC}"

# 下载二进制文件
echo -e "${YELLOW}📥 Downloading binary...${NC}"
if ! wget -q --spider "$BINARY_URL"; then
    echo -e "${RED}❌ Binary not found at: $BINARY_URL${NC}"
    echo "Please check the release URL and binary name"
    exit 1
fi

wget -O "$BINARY" "$BINARY_URL"
chmod +x "$BINARY"
echo -e "${GREEN}✅ Binary downloaded successfully${NC}"

# 显示二进制信息
echo -e "${YELLOW}📊 Binary information:${NC}"
file "$BINARY"
ls -lh "$BINARY"

# 部署到服务器
echo -e "${YELLOW}🚀 Deploying to server $SERVER_IP...${NC}"

ssh root@$SERVER_IP << EOF
set -e

echo "🛠️  Creating directory structure..."
mkdir -p /opt/wol-server

echo "📦 Installing WoL Server..."
cat > /opt/wol-server/wol-server.service << 'SERVICE_EOF'
[Unit]
Description=Wake-on-LAN Server
After=network.target
Wants=network.target

[Service]
Type=simple
ExecStart=/opt/wol-server/wol-server
WorkingDirectory=/opt/wol-server
Restart=always
RestartSec=10
StandardOutput=journal
StandardError=journal
SyslogIdentifier=wol-server

# Security settings
NoNewPrivileges=yes
PrivateTmp=yes
ProtectSystem=strict
ProtectHome=yes
ReadWritePaths=/opt/wol-server

[Install]
WantedBy=multi-user.target
SERVICE_EOF

echo "🔧 Setting up service..."
systemctl daemon-reload
systemctl enable wol-server

echo "🔒 Configuring firewall..."
if command -v ufw >/dev/null 2>&1; then
    ufw allow 8044/tcp comment "WoL Server"
    ufw reload
    echo "✅ UFW configured"
elif command -v firewall-cmd >/dev/null 2>&1; then
    firewall-cmd --permanent --add-port=8044/tcp
    firewall-cmd --reload
    echo "✅ Firewalld configured"
else
    echo "⚠️  No firewall manager found, please configure manually if needed"
fi

echo "🎉 Deployment completed successfully!"
EOF

# 上传二进制文件
echo -e "${YELLOW}📤 Uploading binary to server...${NC}"
scp "$BINARY" root@$SERVER_IP:/opt/wol-server/wol-server

# 启动服务
echo -e "${YELLOW}🔌 Starting WoL Server service...${NC}"
ssh root@$SERVER_IP 'chmod +x /opt/wol-server/wol-server && systemctl start wol-server'

# 验证部署
echo -e "${YELLOW}🔍 Verifying deployment...${NC}"
sleep 2
if ssh root@$SERVER_IP 'systemctl is-active --quiet wol-server'; then
    echo -e "${GREEN}✅ WoL Server is running${NC}"
else
    echo -e "${RED}❌ WoL Server failed to start${NC}"
    ssh root@$SERVER_IP 'journalctl -u wol-server -n 10 --no-pager'
    exit 1
fi

# 显示访问信息
SERVER_URL="http://$SERVER_IP:8044"
echo -e "\n${GREEN}🎊 Deployment completed successfully!${NC}"
echo -e "🌐 ${GREEN}Web interface: $SERVER_URL${NC}"
echo -e "🔧 ${GREEN}API endpoint: $SERVER_URL/wol?mac=XX-XX-XX-XX-XX-XX&ip=Y.Y.Y.Y${NC}"
echo -e "📋 ${GREEN}Service status: ssh root@$SERVER_IP 'systemctl status wol-server'${NC}"
echo -e "📊 ${GREEN}View logs: ssh root@$SERVER_IP 'journalctl -u wol-server -f'${NC}"

# 清理临时文件
rm -f "$BINARY"
echo -e "${GREEN}🧹 Temporary files cleaned up${NC}"
