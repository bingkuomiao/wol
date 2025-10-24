#define _GNU_SOURCE  // 启用GNU扩展，包括strdup等函数
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

#define DEFAULT_PORT 8044
#define BUFFER_SIZE 8192
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
    "* { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }"
    "body { background: linear-gradient(135deg, #1a2a6c, #b21f1f, #fdbb2d); color: #fff; min-height: 100vh; padding: 20px; display: flex; flex-direction: column; align-items: center; }"
    ".container { max-width: 800px; width: 100%; background: rgba(0, 0, 0, 0.7); border-radius: 15px; padding: 30px; box-shadow: 0 10px 30px rgba(0, 0, 0, 0.5); margin-top: 20px; }"
    "h1 { text-align: center; margin-bottom: 20px; font-size: 2.5rem; text-shadow: 2px 2px 4px rgba(0, 0, 0, 0.5); }"
    "h2 { margin: 25px 0 15px; color: #fdbb2d; border-bottom: 2px solid #fdbb2d; padding-bottom: 8px; }"
    ".card { background: rgba(255, 255, 255, 0.1); border-radius: 10px; padding: 20px; margin-bottom: 20px; box-shadow: 0 4px 15px rgba(0, 0, 0, 0.2); }"
    ".form-group { margin-bottom: 15px; }"
    "label { display: block; margin-bottom: 8px; font-weight: bold; }"
    "input { width: 100%; padding: 12px; border: none; border-radius: 5px; background: rgba(255, 255, 255, 0.9); font-size: 16px; }"
    "button { background: #fdbb2d; color: #1a2a6c; border: none; padding: 12px 25px; border-radius: 5px; cursor: pointer; font-size: 16px; font-weight: bold; transition: all 0.3s; margin-top: 10px; width: 100%; }"
    "button:hover { background: #ffcc44; transform: translateY(-2px); box-shadow: 0 5px 15px rgba(0, 0, 0, 0.3); }"
    "button:disabled { background: #cccccc; cursor: not-allowed; transform: none; box-shadow: none; }"
    ".result { margin-top: 20px; padding: 15px; border-radius: 5px; background: rgba(255, 255, 255, 0.1); display: none; }"
    ".success { background: rgba(46, 204, 113, 0.2); border-left: 5px solid #2ecc71; }"
    ".error { background: rgba(231, 76, 60, 0.2); border-left: 5px solid #e74c3c; }"
    ".info { background: rgba(52, 152, 219, 0.2); border-left: 5px solid #3498db; }"
    ".url-example { background: rgba(255, 255, 255, 0.1); padding: 15px; border-radius: 5px; margin-top: 15px; font-family: monospace; word-break: break-all; }"
    ".status { display: flex; align-items: center; margin-bottom: 10px; }"
    ".status-indicator { width: 12px; height: 12px; border-radius: 50%; margin-right: 10px; }"
    ".online { background: #2ecc71; animation: pulse 2s infinite; }"
    "@keyframes pulse { 0% { opacity: 1; } 50% { opacity: 0.5; } 100% { opacity: 1; } }"
    ".footer { margin-top: 30px; text-align: center; font-size: 0.9rem; color: rgba(255, 255, 255, 0.7); }"
    ".history { max-height: 200px; overflow-y: auto; }"
    ".history-item { padding: 8px; border-bottom: 1px solid rgba(255, 255, 255, 0.1); font-family: monospace; font-size: 0.9rem; }"
    ".timestamp { color: #fdbb2d; margin-right: 10px; }"
    "</style>"
    "</head>"
    "<body>"
    "<h1>网络唤醒(WoL)服务器</h1>"
    
    "<div class=\"container\">"
    "<div class=\"card\">"
    "<h2>服务器状态</h2>"
    "<div class=\"status\">"
    "<div class=\"status-indicator online\"></div>"
    "<span>服务器正在运行</span>"
    "</div>"
    "<p>服务器已启动并正在监听WoL请求。您可以使用下面的表单或直接通过URL发送唤醒包。</p>"
    "</div>"
    
    "<div class=\"card\">"
    "<h2>发送唤醒包</h2>"
    "<div class=\"form-group\">"
    "<label for=\"mac\">MAC地址:</label>"
    "<input type=\"text\" id=\"mac\" placeholder=\"例如: b6-6f-9c-cc-d7-99\" value=\"b6-6f-9c-cc-d7-99\">"
    "</div>"
    
    "<div class=\"form-group\">"
    "<label for=\"ip\">目标IP地址 (广播地址):</label>"
    "<input type=\"text\" id=\"ip\" placeholder=\"例如: 192.168.31.255\" value=\"192.168.31.255\">"
    "</div>"
    
    "<div class=\"form-group\">"
    "<label for=\"port\">端口 (默认: 9):</label>"
    "<input type=\"number\" id=\"port\" placeholder=\"例如: 9\" value=\"9\" min=\"1\" max=\"65535\">"
    "</div>"
    
    "<button id=\"sendBtn\">发送唤醒包</button>"
    
    "<div id=\"result\" class=\"result\"></div>"
    "</div>"
    
    "<div class=\"card\">"
    "<h2>直接URL访问</h2>"
    "<p>您可以直接通过浏览器访问以下URL格式来发送唤醒包：</p>"
    "<div class=\"url-example\">"
    "http://<span id=\"current-host\">192.168.31.1</span>/wol?mac=<span id=\"url-mac\">b6-6f-9c-cc-d7-99</span>&ip=<span id=\"url-ip\">192.168.31.255</span>&port=<span id=\"url-port\">9</span>"
    "</div>"
    "<div class=\"url-example\">"
    "http://<span id=\"current-host\">192.168.31.1</span>/wol?mac=<span id=\"url-mac\">b6:6f:9c:cc:d7:99</span>&ip=<span id=\"url-ip\">192.168.31.255</span>&port=<span id=\"url-port\">9</span>"
    "</div>"
    "<p>将上述URL中的MAC地址和IP地址替换为您需要的值即可。</p>"
    "</div>"
    
    "<div class=\"card\">"
    "<h2>发送历史</h2>"
    "<div id=\"history\" class=\"history\"></div>"
    "</div>"
    
    "<div class=\"footer\">"
    "<p>网络唤醒服务器 &copy; 2025</p>"
    "</div>"
    "<script>"
    "document.getElementById('current-host').textContent = window.location.hostname;"
    
    "document.getElementById('mac').addEventListener('input', updateUrlExample);"
    "document.getElementById('ip').addEventListener('input', updateUrlExample);"
    "document.getElementById('port').addEventListener('input', updateUrlExample);"
    
    "function updateUrlExample() {"
    "document.getElementById('url-mac').textContent = document.getElementById('mac').value;"
    "document.getElementById('url-ip').textContent = document.getElementById('ip').value;"
    "document.getElementById('url-port').textContent = document.getElementById('port').value;"
    "}"
    
    "document.getElementById('sendBtn').addEventListener('click', function() {"
    "const mac = document.getElementById('mac').value;"
    "const ip = document.getElementById('ip').value;"
    "const port = document.getElementById('port').value;"
    
    "if (!mac || !ip) {"
    "showResult('请填写MAC地址和IP地址', 'error');"
    "return;"
    "}"
    
    "sendWoL(mac, ip, port);"
    "});"
    
    "function sendWoL(mac, ip, port) {"
    "const sendBtn = document.getElementById('sendBtn');"
    "sendBtn.disabled = true;"
    "sendBtn.textContent = '发送中...';"
    
    "showResult('正在发送唤醒包...', 'info');"
    
    "const params = new URLSearchParams({ mac: mac, ip: ip, port: port });"
    "const url = '/wol?' + params.toString();"
    
    "fetch(url)"
    ".then(response => response.json())"
    ".then(data => {"
    "if (data.success) {"
    "showResult(data.message, 'success');"
    "addToHistory(data);"
    "} else {"
    "showResult('错误: ' + data.error, 'error');"
    "}"
    "})"
    ".catch(error => {"
    "console.error('请求失败:', error);"
    "showResult('请求失败: ' + error.message, 'error');"
    "})"
    ".finally(() => {"
    "sendBtn.disabled = false;"
    "sendBtn.textContent = '发送唤醒包';"
    "});"
    "}"
    
    "function showResult(message, type) {"
    "const resultDiv = document.getElementById('result');"
    "resultDiv.textContent = message;"
    "resultDiv.className = 'result ' + type;"
    "resultDiv.style.display = 'block';"
    
    "if (type === 'success' || type === 'info') {"
    "setTimeout(() => {"
    "resultDiv.style.display = 'none';"
    "}, 3000);"
    "}"
    "}"
    
    "function addToHistory(data) {"
    "const historyDiv = document.getElementById('history');"
    "const historyItem = document.createElement('div');"
    "historyItem.className = 'history-item';"
    
    "const timestamp = new Date().toLocaleTimeString();"
    "const status = data.success ? '成功' : '失败';"
    "const statusClass = data.success ? 'success' : 'error';"
    
    "historyItem.innerHTML = '<span class=\"timestamp\">' + timestamp + '</span><span class=\"status ' + statusClass + '\">' + status + '</span><span>MAC: ' + data.mac + ', IP: ' + data.ip + '</span>';"
    
    "historyDiv.insertBefore(historyItem, historyDiv.firstChild);"
    
    "if (historyDiv.children.length > 10) {"
    "historyDiv.removeChild(historyDiv.lastChild);"
    "}"
    "}"
    
    "updateUrlExample();"
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
        char *port_str = get_param_value(query, "port");
        
        if (mac == NULL) {
            send_json_response(client_fd, 400, "{\"success\":false,\"error\":\"MAC地址参数缺失\"}");
        } else {
            // URL解码参数
            char decoded_mac[64] = {0};
            char decoded_ip[64] = {0};
            char decoded_port[16] = {0};
            url_decode(decoded_mac, mac);
            if (ip) {
                url_decode(decoded_ip, ip);
            }
            if (port_str) {
                url_decode(decoded_port, port_str);
            }
            
            // 验证MAC地址格式
            int valid = 1;
            if (strlen(decoded_mac) != 17) valid = 0;  // XX-XX-XX-XX-XX-XX 或 XX:XX:XX:XX:XX:XX
            
            if (valid) {
                const char *default_ip = "255.255.255.255";
                const char *target_ip = ip ? decoded_ip : default_ip;
                int port = port_str ? atoi(decoded_port) : 9;
                int result = send_wol_packet(decoded_mac, target_ip, port);
                
                if (result == 0) {
                    snprintf(buffer, sizeof(buffer), 
                            "{\"success\":true,\"message\":\"已发送唤醒包到 %s\",\"mac\":\"%s\",\"ip\":\"%s\",\"port\":%d}",
                            decoded_mac, decoded_mac, target_ip, port);
                    send_json_response(client_fd, 200, buffer);
                    printf("WoL包发送成功: MAC=%s, IP=%s, Port=%d\n", decoded_mac, target_ip, port);
                } else {
                    send_json_response(client_fd, 500, "{\"success\":false,\"error\":\"发送WoL包失败\"}");
                    printf("WoL包发送失败: MAC=%s, IP=%s, Port=%d\n", decoded_mac, target_ip, port);
                }
            } else {
                send_json_response(client_fd, 400, "{\"success\":false,\"error\":\"MAC地址格式错误\"}");
            }
            
            free(mac);
            if (ip) free(ip);
            if (port_str) free(port_str);
        }
    } else if (strcmp(path, "/") == 0) {
        // 返回HTML页面
        send_response(client_fd, 200, "text/html", get_html_page());
    } else {
        send_json_response(client_fd, 404, "{\"success\":false,\"error\":\"Not found\"}");
    }
}

// 显示使用帮助
void print_usage(const char *program_name) {
    printf("用法: %s [-p port]\n", program_name);
    printf("选项:\n");
    printf("  -p, --port PORT    指定服务器端口 (默认: %d)\n", DEFAULT_PORT);
    printf("  -h, --help         显示此帮助信息\n");
}

int main(int argc, char *argv[]) {
    int server_fd, client_fd;
    struct sockaddr_in address;
    socklen_t addrlen = sizeof(address);
    int opt = 1;
    int port = DEFAULT_PORT;
    
    // 解析命令行参数
    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-p") == 0 || strcmp(argv[i], "--port") == 0) {
            if (i + 1 < argc) {
                port = atoi(argv[i + 1]);
                if (port <= 0 || port > 65535) {
                    fprintf(stderr, "错误: 端口号必须在1-65535之间\n");
                    return 1;
                }
                i++; // 跳过下一个参数（端口值）
            } else {
                fprintf(stderr, "错误: %s 选项需要参数\n", argv[i]);
                print_usage(argv[0]);
                return 1;
            }
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            print_usage(argv[0]);
            return 0;
        } else {
            fprintf(stderr, "错误: 未知选项 %s\n", argv[i]);
            print_usage(argv[0]);
            return 1;
        }
    }
    
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
    address.sin_port = htons(port);
    
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
    
    printf("WoL服务器 (C语言版本) 运行在端口 %d\n", port);
    printf("访问 http://localhost:%d 使用Web界面\n", port);
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