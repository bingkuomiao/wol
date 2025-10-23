#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/types.h>
#include <signal.h>
#include <ctype.h>

#define PORT 8044
#define BUFFER_SIZE 4096
#define MAGIC_PACKET_SIZE 102

// WoL魔术包结构
typedef struct {
    unsigned char sync[6];      // 6个0xFF
    unsigned char mac[16][6];   // 16次重复MAC地址
} magic_packet_t;

// 发送WoL魔术包
int send_wol_packet(const char *mac_str, const char *ip_str, int port) {
    int sockfd;
    struct sockaddr_in dest_addr;
    magic_packet_t packet;
    
    // 解析MAC地址
    unsigned char mac[6];
    if (sscanf(mac_str, "%2hhx:%2hhx:%2hhx:%2hhx:%2hhx:%2hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6 &&
        sscanf(mac_str, "%2hhx-%2hhx-%2hhx-%2hhx-%2hhx-%2hhx",
               &mac[0], &mac[1], &mac[2], &mac[3], &mac[4], &mac[5]) != 6) {
        return -1;
    }
    
    // 构建魔术包
    memset(&packet, 0xFF, sizeof(packet.sync));
    for (int i = 0; i < 16; i++) {
        memcpy(packet.mac[i], mac, 6);
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
    int result = sendto(sockfd, &packet, sizeof(packet), 0,
                       (struct sockaddr*)&dest_addr, sizeof(dest_addr));
    
    close(sockfd);
    return (result == sizeof(packet)) ? 0 : -1;
}

// 解析HTTP查询参数
char* get_param_value(const char *query, const char *param) {
    char *query_copy = strdup(query);
    char *token, *saveptr = NULL;
    char *result = NULL;
    
    token = strtok_r(query_copy, "&", &saveptr);
    while (token != NULL) {
        char *eq = strchr(token, '=');
        if (eq != NULL) {
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

// URL解码函数
void url_decode(char *dst, const char *src) {
    char a, b;
    while (*src) {
        if ((*src == '%') &&
            ((a = src[1]) && (b = src[2])) &&
            (isxdigit(a) && isxdigit(b))) {
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
    *dst++ = '\0';
}

// 发送HTTP响应
void send_response(int client_fd, int status_code, const char *content_type, const char *body) {
    char response[BUFFER_SIZE];
    int length = snprintf(response, sizeof(response),
                         "HTTP/1.1 %d OK\r\n"
                         "Content-Type: %s\r\n"
                         "Content-Length: %zu\r\n"
                         "Access-Control-Allow-Origin: *\r\n"
                         "Access-Control-Allow-Methods: GET, OPTIONS\r\n"
                         "Connection: close\r\n"
                         "\r\n"
                         "%s",
                         status_code, content_type, strlen(body), body);
    
    send(client_fd, response, length, 0);
}

// 发送JSON响应
void send_json_response(int client_fd, int status_code, const char *json) {
    send_response(client_fd, status_code, "application/json", json);
}

// HTML页面内容
const char* get_html_page() {
    return 
    "<!DOCTYPE html>"
    "<html lang=\"zh-CN\">"
    "<head>"
    "<meta charset=\"UTF-8\">"
    "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">"
    "<title>网络唤醒(WoL)服务器</title>"
    "<style>"
    "* { margin: 0; padding: 0; box-sizing: border-box; font-family: Arial, sans-serif; }"
    "body { background: #1a2a6c; color: #fff; min-height: 100vh; padding: 20px; }"
    ".container { max-width: 800px; margin: 0 auto; background: rgba(0,0,0,0.7); border-radius: 10px; padding: 20px; }"
    "h1 { text-align: center; margin-bottom: 20px; }"
    ".card { background: rgba(255,255,255,0.1); border-radius: 8px; padding: 15px; margin-bottom: 15px; }"
    ".form-group { margin-bottom: 10px; }"
    "label { display: block; margin-bottom: 5px; font-weight: bold; }"
    "input { width: 100%; padding: 8px; border: none; border-radius: 4px; }"
    "button { background: #fdbb2d; color: #000; border: none; padding: 10px; border-radius: 4px; cursor: pointer; width: 100%; font-weight: bold; }"
    ".result { margin-top: 10px; padding: 10px; border-radius: 4px; display: none; }"
    ".success { background: rgba(46, 204, 113, 0.3); border-left: 4px solid #2ecc71; }"
    ".error { background: rgba(231, 76, 60, 0.3); border-left: 4px solid #e74c3c; }"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"container\">"
    "<h1>网络唤醒(WoL)服务器 - C版本</h1>"
    "<div class=\"card\">"
    "<h2>发送唤醒包</h2>"
    "<div class=\"form-group\">"
    "<label for=\"mac\">MAC地址:</label>"
    "<input type=\"text\" id=\"mac\" value=\"b6-6f-9c-cc-d7-99\">"
    "</div>"
    "<div class=\"form-group\">"
    "<label for=\"ip\">目标IP地址:</label>"
    "<input type=\"text\" id=\"ip\" value=\"192.168.31.255\">"
    "</div>"
    "<button onclick=\"sendWoL()\">发送唤醒包</button>"
    "<div id=\"result\" class=\"result\"></div>"
    "</div>"
    "<div class=\"card\">"
    "<h2>使用说明</h2>"
    "<p>您可以直接通过浏览器访问以下URL格式来发送唤醒包：</p>"
    "<p>http://ip:port/wol?mac=XX-XX-XX-XX-XX-XX&ip=Y.Y.Y.Y</p>"
    "<p>将上述URL中的MAC地址和IP地址替换为您需要的值即可。</p>"
    "</div>"
    "</div>"
    "<script>"
    "function sendWoL() {"
    "const mac = document.getElementById('mac').value;"
    "const ip = document.getElementById('ip').value;"
    "fetch('/wol?mac=' + encodeURIComponent(mac) + '&ip=' + encodeURIComponent(ip))"
    ".then(r => r.json())"
    ".then(data => {"
    "const result = document.getElementById('result');"
    "result.textContent = data.success ? data.message : '错误: ' + data.error;"
    "result.className = 'result ' + (data.success ? 'success' : 'error');"
    "result.style.display = 'block';"
    "});"
    "}"
    "</script>"
    "</body>"
    "</html>";
}

// 处理HTTP请求
void handle_request(int client_fd, const char *request) {
    char method[16], path[256], protocol[16];
    char buffer[BUFFER_SIZE];
    
    // 解析请求行
    if (sscanf(request, "%15s %255s %15s", method, path, protocol) != 3) {
        send_json_response(client_fd, 400, "{\"success\":false,\"error\":\"Invalid request\"}");
        return;
    }
    
    // 处理预检请求
    if (strcmp(method, "OPTIONS") == 0) {
        send_response(client_fd, 200, "text/plain", "");
        return;
    }
    
    // 只处理GET请求
    if (strcmp(method, "GET") != 0) {
        send_json_response(client_fd, 405, "{\"success\":false,\"error\":\"Method not allowed\"}");
        return;
    }
    
    // 解析路径和查询参数
    char *query = strchr(path, '?');
    if (query != NULL) {
        *query = '\0';
        query++;
    }
    
    if (strcmp(path, "/wol") == 0 && query != NULL) {
        // 处理WoL请求
        char *mac = get_param_value(query, "mac");
        char *ip = get_param_value(query, "ip");
        
        if (mac == NULL) {
            send_json_response(client_fd, 400, "{\"success\":false,\"error\":\"MAC地址参数缺失\"}");
        } else {
            // URL解码参数
            char decoded_mac[64] = {0};
            char decoded_ip[64] = {0};
            url_decode(decoded_mac, mac);
            if (ip) {
                url_decode(decoded_ip, ip);
            }
            
            // 验证MAC地址格式
            int valid = 1;
            if (strlen(decoded_mac) != 17) valid = 0;  // XX-XX-XX-XX-XX-XX 或 XX:XX:XX:XX:XX:XX
            
            if (valid) {
                const char *default_ip = "255.255.255.255";
                const char *target_ip = ip ? decoded_ip : default_ip;
                int result = send_wol_packet(decoded_mac, target_ip, 9);
                
                if (result == 0) {
                    snprintf(buffer, sizeof(buffer), 
                            "{\"success\":true,\"message\":\"已发送唤醒包到 %s\",\"mac\":\"%s\",\"ip\":\"%s\"}",
                            decoded_mac, decoded_mac, target_ip);
                    send_json_response(client_fd, 200, buffer);
                    printf("WoL包发送成功: MAC=%s, IP=%s\n", decoded_mac, target_ip);
                } else {
                    send_json_response(client_fd, 500, "{\"success\":false,\"error\":\"发送WoL包失败\"}");
                    printf("WoL包发送失败: MAC=%s, IP=%s\n", decoded_mac, target_ip);
                }
            } else {
                send_json_response(client_fd, 400, "{\"success\":false,\"error\":\"MAC地址格式错误\"}");
            }
            
            free(mac);
            if (ip) free(ip);
        }
    } else if (strcmp(path, "/") == 0) {
        // 返回HTML页面
        send_response(client_fd, 200, "text/html", get_html_page());
    } else {
        send_json_response(client_fd, 404, "{\"success\":false,\"error\":\"Not found\"}");
    }
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int opt = 1;
    
    // 创建socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }
    
    // 设置socket选项 - 只使用 SO_REUSEADDR
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;
    address.sin_port = htons(PORT);
    
    // 绑定端口
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    // 开始监听
    if (listen(server_fd, 10) < 0) {
        perror("listen");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    
    printf("WoL服务器 (C语言版本) 运行在端口 %d\n", PORT);
    printf("访问 http://localhost:%d 使用Web界面\n", PORT);
    printf("按 Ctrl+C 停止服务器\n");
    
    // 主循环
    while (1) {
        if ((client_fd = accept(server_fd, (struct sockaddr *)&address, &addrlen)) < 0) {
            perror("accept");
            continue;
        }
        
        char buffer[BUFFER_SIZE] = {0};
        ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
        
        if (bytes_read > 0) {
            handle_request(client_fd, buffer);
        }
        
        close(client_fd);
    }
    
    close(server_fd);
    return 0;
}
