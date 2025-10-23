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
int send_wol_packet(const char *mac_str, const char *ip_str, int port) {
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
    dest_addr.sin_port = htons(port);
    
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

// 解析查询参数
char* get_param_value(const char *query, const char *param) {
    char *query_copy = strdup(query);
    char *token, *saveptr = NULL;
    char *result = NULL;
    
    token = strtok_r(query_copy, "&", &saveptr);
    while (token) {
        char *eq = strchr(token, '=');
        if (eq) {
            *eq = '\0';
            if (strcmp(token, param) == 0) {
                result = strdup(eq + 1);
                break;
            }
        }
        token = strtok_r(NULL, "&", &saveptr);
    }
    
    free(query_copy);
    return result;
}

// 发送HTTP响应
void send_response(int client_fd, int status, const char *content_type, const char *body) {
    char header[512];
    int header_len = snprintf(header, sizeof(header),
        "HTTP/1.1 %d OK\r\n"
        "Content-Type: %s\r\n"
        "Content-Length: %zu\r\n"
        "Access-Control-Allow-Origin: *\r\n"
        "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
        "Connection: close\r\n\r\n",
        status, content_type, strlen(body));
    
    send(client_fd, header, header_len, 0);
    send(client_fd, body, strlen(body), 0);
}

// HTML页面
const char* html_page = 
"<!DOCTYPE html>"
"<html>"
"<head><meta charset=\"UTF-8\"><title>WoL Server</title>"
"<style>"
"body{font-family:Arial,sans-serif;margin:40px;background:#f0f0f0;}"
".container{max-width:600px;margin:0 auto;background:white;padding:20px;border-radius:8px;box-shadow:0 2px 10px rgba(0,0,0,0.1);}"
"h1{color:#333;text-align:center;}"
".form-group{margin-bottom:15px;}"
"label{display:block;margin-bottom:5px;font-weight:bold;}"
"input{width:100%;padding:8px;border:1px solid #ddd;border-radius:4px;}"
"button{background:#007cba;color:white;border:none;padding:10px 20px;border-radius:4px;cursor:pointer;width:100%;}"
".result{margin-top:15px;padding:10px;border-radius:4px;}"
".success{background:#d4edda;border:1px solid #c3e6cb;color:#155724;}"
".error{background:#f8d7da;border:1px solid #f5c6cb;color:#721c24;}"
"</style></head>"
"<body><div class=\"container\">"
"<h1>Wake-on-LAN Server</h1>"
"<div class=\"form-group\"><label>MAC Address:</label><input type=\"text\" id=\"mac\" value=\"b6-6f-9c-cc-d7-99\"></div>"
"<div class=\"form-group\"><label>IP Address:</label><input type=\"text\" id=\"ip\" value=\"192.168.31.255\"></div>"
"<button onclick=\"sendWoL()\">Send WoL Packet</button>"
"<div id=\"result\" class=\"result\"></div>"
"<p><strong>Direct URL:</strong> /wol?mac=XX-XX-XX-XX-XX-XX&amp;ip=Y.Y.Y.Y</p>"
"</div>"
"<script>"
"function sendWoL(){"
"var mac=document.getElementById('mac').value;"
"var ip=document.getElementById('ip').value;"
"fetch('/wol?mac='+encodeURIComponent(mac)+'&ip='+encodeURIComponent(ip))"
".then(r=>r.json()).then(data=>{"
"var r=document.getElementById('result');"
"r.textContent=data.success?data.message:'Error: '+data.error;"
"r.className='result '+(data.success?'success':'error');"
"});}"
"</script></body></html>";

int main() {
    int server_fd, client_fd;
    struct sockaddr_in addr;
    socklen_t addrlen = sizeof(addr);
    int opt = 1;
    
    // 创建socket
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("socket");
        return 1;
    }
    
    // 设置选项
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    
    // 绑定端口
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(PORT);
    
    if (bind(server_fd, (struct sockaddr*)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(server_fd);
        return 1;
    }
    
    // 监听
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        return 1;
    }
    
    printf("WoL Server running on port %d\n", PORT);
    
    while (1) {
        client_fd = accept(server_fd, (struct sockaddr*)&addr, &addrlen);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }
        
        char buffer[BUFFER_SIZE] = {0};
        int bytes_read = read(client_fd, buffer, sizeof(buffer)-1);
        
        if (bytes_read > 0) {
            char method[16], path[256];
            sscanf(buffer, "%15s %255s", method, path);
            
            // 处理OPTIONS预检请求
            if (strcmp(method, "OPTIONS") == 0) {
                send_response(client_fd, 200, "text/plain", "");
            }
            // 处理WoL请求
            else if (strcmp(method, "GET") == 0) {
                char *query = strchr(path, '?');
                if (query) {
                    *query++ = '\0';
                }
                
                if (strcmp(path, "/wol") == 0 && query) {
                    char *mac = get_param_value(query, "mac");
                    char *ip = get_param_value(query, "ip");
                    
                    if (mac) {
                        char decoded_mac[64], decoded_ip[64] = "255.255.255.255";
                        url_decode(decoded_mac, mac);
                        if (ip) {
                            url_decode(decoded_ip, ip);
                        }
                        
                        int result = send_wol_packet(decoded_mac, decoded_ip, 9);
                        char json[256];
                        
                        if (result == 0) {
                            snprintf(json, sizeof(json), 
                                "{\"success\":true,\"message\":\"Sent to %s\",\"mac\":\"%s\",\"ip\":\"%s\"}",
                                decoded_mac, decoded_mac, decoded_ip);
                            send_response(client_fd, 200, "application/json", json);
                            printf("WoL sent: %s -> %s\n", decoded_mac, decoded_ip);
                        } else {
                            send_response(client_fd, 500, "application/json", 
                                "{\"success\":false,\"error\":\"Failed to send WoL packet\"}");
                        }
                        
                        free(mac);
                        if (ip) free(ip);
                    } else {
                        send_response(client_fd, 400, "application/json", 
                            "{\"success\":false,\"error\":\"MAC address required\"}");
                    }
                } else {
                    send_response(client_fd, 200, "text/html", html_page);
                }
            }
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}
