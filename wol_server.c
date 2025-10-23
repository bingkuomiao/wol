#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <ctype.h>

#define PORT 8044
#define BUFFER_SIZE 4096

// 发送WoL魔术包
int send_wol_packet(const char *mac_str, const char *ip_str) {
    int sockfd;
    struct sockaddr_in dest_addr;
    unsigned char packet[102];
    unsigned char mac[6];
    
    // 解析MAC地址
    if (sscanf(mac_str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6 &&
        sscanf(mac_str, "%2hhx-%2hhx-%2hhx-%2hhx-%2hhx-%2hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        return -1;
    }
    
    // 构建魔术包
    memset(packet, 0xFF, 6);
    for (int i = 0; i < 16; i++) {
        memcpy(&packet[6 + i * 6], mac, 6);
    }
    
    // 创建UDP socket
    if ((sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0) {
        return -1;
    }
    
    // 设置广播选项
    int broadcast = 1;
    if (setsockopt(sockfd, SOL_SOCKET, SO_BROADCAST, &broadcast, sizeof(broadcast)) < 0) {
        close(sockfd);
        return -1;
    }
    
    // 设置目标地址
    memset(&dest_addr, 0, sizeof(dest_addr));
    dest_addr.sin_family = AF_INET;
    dest_addr.sin_port = htons(9);
    
    if (inet_pton(AF_INET, ip_str, &dest_addr.sin_addr) <= 0) {
        close(sockfd);
        return -1;
    }
    
    // 发送魔术包
    int result = sendto(sockfd, packet, sizeof(packet), 0,
                       (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    close(sockfd);
    return (result == sizeof(packet)) ? 0 : -1;
}

// URL解码
void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') && (a = src[1]) && (b = src[2]) && isxdigit(a) && isxdigit(b)) {
            if (a >= 'a') a -= 'a'-'A';
            if (a >= 'A') a -= ('A' - 10);
            else a -= '0';
            if (b >= 'a') b -= 'a'-'A';
            if (b >= 'A') b -= ('A' - 10);
            else b -= '0';
            *dst++ = 16*a+b;
            src += 3;
        } else if (*src == '+') {
            *dst++ = ' ';
            src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

// 发送HTTP响应
void send_response(int client_fd, const char *body, const char *content_type) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 200 OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
        "Connection: close\r\n\r\n",
        content_type, strlen(body));
    
    send(client_fd, header, header_len, 0);
    send(client_fd, body, strlen(body), 0);
}

// HTML页面
const char* html_page = 
"<!DOCTYPE html>"
"<html><head><meta charset=\"UTF-8\"><title>WoL Server</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:20px;background:#f5f5f5;}"
".container{max-width:500px;margin:0 auto;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
"h1{color:#333;text-align:center;margin-bottom:20px;}"
".form-group{margin-bottom:15px;}"
"label{display:block;margin-bottom:5px;font-weight:bold;color:#555;}"
"input{width:100%;padding:10px;border:1px solid #ddd;border-radius:4px;box-sizing:border-box;}"
"button{background:#007cba;color:white;border:none;padding:12px;border-radius:4px;cursor:pointer;width:100%;font-size:16px;font-weight:bold;}"
"button:hover{background:#005a87;}"
".result{margin-top:15px;padding:12px;border-radius:4px;display:none;}"
".success{background:#d4edda;border:1px solid #c3e6cb;color:#155724;}"
".error{background:#f8d7da;border:1px solid #f5c6cb;color:#721c24;}"
".info{background:#d1ecf1;border:1px solid #bee5eb;color:#0c5460;margin-bottom:15px;}"
"</style></head>"
"<body><div class=\"container\">"
"<h1>🔌 Wake-on-LAN Server</h1>"
"<div class=\"info\">"
"<strong>📋 使用说明：</strong><br>"
"1. 填写目标设备的MAC地址<br>"
"2. 填写广播IP地址（通常是子网广播地址）<br>"
"3. 点击发送唤醒包"
"</div>"
"<div class=\"form-group\"><label>MAC地址：</label><input type=\"text\" id=\"mac\" placeholder=\"b6-6f-9c-cc-d7-99\" value=\"\"></div>"
"<div class=\"form-group\"><label>IP地址：</label><input type=\"text\" id=\"ip\" placeholder=\"192.168.31.255\" value=\"\"></div>"
"<button onclick=\"sendWoL()\">🚀 发送唤醒包</button>"
"<div id=\"result\" class=\"result\"></div>"
"<div style=\"margin-top:20px;padding-top:15px;border-top:1px solid #eee;font-size:12px;color:#666;\">"
"<strong>🌐 直接访问：</strong><br>"
"<code>/wol?mac=XX-XX-XX-XX-XX-XX&amp;ip=Y.Y.Y.Y</code>"
"</div>"
"</div>"
"<script>"
"function sendWoL(){"
"var mac=document.getElementById('mac').value;"
"var ip=document.getElementById('ip').value;"
"if(!mac||!ip){alert('请填写MAC地址和IP地址');return;}"
"var btn=event.target;btn.disabled=true;btn.innerHTML='⏳ 发送中...';"
"fetch('/wol?mac='+encodeURIComponent(mac)+'&ip='+encodeURIComponent(ip))"
".then(r=>r.json()).then(data=>{"
"var r=document.getElementById('result');"
"r.textContent=data.success?data.message:'错误: '+data.error;"
"r.className='result '+(data.success?'success':'error');"
"r.style.display='block';"
"btn.disabled=false;btn.innerHTML='🚀 发送唤醒包';"
"}).catch(err=>{"
"var r=document.getElementById('result');"
"r.textContent='请求失败: '+err;"
"r.className='result error';"
"r.style.display='block';"
"btn.disabled=false;btn.innerHTML='🚀 发送唤醒包';"
"});}"
"</script></body></html>";

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int opt = 1;
    
    printf("🚀 WoL Server starting...\n");
    
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("❌ socket failed");
        return 1;
    }
    
    // 设置选项
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("❌ setsockopt failed");
        close(server_fd);
        return 1;
    }
    
    // 绑定端口
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("❌ bind failed");
        close(server_fd);
        return 1;
    }
    
    // 监听
    if (listen(server_fd, 10) < 0) {
        perror("❌ listen failed");
        close(server_fd);
        return 1;
    }
    
    printf("✅ WoL Server running on port %d\n", PORT);
    printf("📧 Web interface: http://localhost:%d\n", PORT);
    printf("⏹️  Press Ctrl+C to stop\n");
    
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
        if (client_fd < 0) {
            perror("❌ accept failed");
            continue;
        }
        
        char buffer[BUFFER_SIZE] = {0};
        int bytes_read = read(client_fd, buffer, sizeof(buffer)-1);
        
        if (bytes_read > 0) {
            char method[16], path[256];
            sscanf(buffer, "%15s %255s", method, path);
            
            // 处理OPTIONS预检请求
            if (strcmp(method, "OPTIONS") == 0) {
                send_response(client_fd, "", "text/plain");
            }
            // 处理WoL请求
            else if (strcmp(method, "GET") == 0) {
                if (strstr(path, "/wol?") != NULL) {
                    char *query = strchr(path, '?');
                    if (query) {
                        query++; // 跳过'?'
                        
                        // 简单解析查询参数
                        char *mac_start = strstr(query, "mac=");
                        char *ip_start = strstr(query, "ip=");
                        
                        if (mac_start) {
                            char mac[64] = {0};
                            char ip[64] = "255.255.255.255";
                            char decoded_mac[64] = {0};
                            char decoded_ip[64] = {0};
                            
                            // 提取MAC地址
                            sscanf(mac_start + 4, "%63[^&]", mac);
                            url_decode(decoded_mac, mac);
                            
                            // 提取IP地址
                            if (ip_start) {
                                sscanf(ip_start + 3, "%63[^&]", ip);
                                url_decode(decoded_ip, ip);
                            }
                            
                            // 发送WoL包
                            int result = send_wol_packet(decoded_mac, 
                                ip_start ? decoded_ip : "255.255.255.255");
                            
                            char json[512];
                            if (result == 0) {
                                snprintf(json, sizeof(json), 
                                    "{\"success\":true,\"message\":\"✅ 已发送唤醒包到 %s\",\"mac\":\"%s\",\"ip\":\"%s\"}",
                                    decoded_mac, decoded_mac, 
                                    ip_start ? decoded_ip : "255.255.255.255");
                                send_response(client_fd, json, "application/json");
                                printf("📤 WoL packet sent: MAC=%s, IP=%s\n", 
                                    decoded_mac, ip_start ? decoded_ip : "255.255.255.255");
                            } else {
                                snprintf(json, sizeof(json), 
                                    "{\"success\":false,\"error\":\"❌ 发送WoL包失败\"}");
                                send_response(client_fd, json, "application/json");
                                printf("❌ WoL packet failed: MAC=%s\n", decoded_mac);
                            }
                        } else {
                            send_response(client_fd, 
                                "{\"success\":false,\"error\":\"❌ 缺少MAC地址参数\"}", 
                                "application/json");
                        }
                    }
                } else {
                    // 返回HTML页面
                    send_response(client_fd, html_page, "text/html");
                }
            }
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}
