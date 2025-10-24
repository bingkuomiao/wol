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
    "* { margin: 0; padding: 0; box-sizing: border-box; font-family: 'Segoe UI', Tahoma, Geneva, Verdana, sans-serif; }"
    "body { background: linear-gradient(135deg, #1a2a6c, #b21f1f, #fdbb2d); background-size: 400% 400%; animation: gradient 15s ease infinite; color: #fff; min-height: 100vh; padding: 20px; }"
    "@keyframes gradient { 0% { background-position: 0% 50%; } 50% { background-position: 100% 50%; } 100% { background-position: 0% 50%; } }"
    ".container { max-width: 800px; margin: 0 auto; background: rgba(0,0,0,0.7); border-radius: 15px; padding: 30px; box-shadow: 0 10px 30px rgba(0,0,0,0.5); backdrop-filter: blur(10px); }"
    "h1 { text-align: center; margin-bottom: 25px; font-size: 2.2rem; text-shadow: 0 2px 4px rgba(0,0,0,0.5); }"
    "h2 { color: #fdbb2d; margin-bottom: 15px; font-size: 1.4rem; border-bottom: 2px solid #fdbb2d; padding-bottom: 8px; }"
    ".card { background: rgba(255,255,255,0.1); border-radius: 12px; padding: 20px; margin-bottom: 20px; transition: transform 0.3s ease, box-shadow 0.3s ease; border: 1px solid rgba(255,255,255,0.1); }"
    ".card:hover { transform: translateY(-5px); box-shadow: 0 8px 20px rgba(0,0,0,0.3); }"
    ".form-group { margin-bottom: 18px; }"
    "label { display: block; margin-bottom: 8px; font-weight: 600; color: #fdbb2d; }"
    "input { width: 100%; padding: 12px; border: 2px solid rgba(255,255,255,0.2); border-radius: 8px; background: rgba(0,0,0,0.4); color: white; font-size: 16px; transition: all 0.3s ease; }"
    "input:focus { outline: none; border-color: #fdbb2d; box-shadow: 0 0 0 3px rgba(253, 187, 45, 0.3); background: rgba(0,0,0,0.6); }"
    "input::placeholder { color: rgba(255,255,255,0.5); }"
    ".btn-container { position: relative; margin-top: 10px; }"
    "button { background: linear-gradient(135deg, #fdbb2d, #e6ac00); color: #000; border: none; padding: 14px; border-radius: 8px; cursor: pointer; width: 100%; font-weight: bold; font-size: 16px; transition: all 0.3s ease; box-shadow: 0 4px 15px rgba(253, 187, 45, 0.4); position: relative; overflow: hidden; }"
    "button:hover { transform: translateY(-2px); box-shadow: 0 6px 20px rgba(253, 187, 45, 0.6); background: linear-gradient(135deg, #ffcc44, #f0b400); }"
    "button:active { transform: translateY(1px); box-shadow: 0 2px 10px rgba(253, 187, 45, 0.4); }"
    "button:disabled { background: #666; cursor: not-allowed; transform: none; box-shadow: none; }"
    "button::after { content: ''; position: absolute; top: 50%; left: 50%; width: 5px; height: 5px; background: rgba(255, 255, 255, 0.5); opacity: 0; border-radius: 100%; transform: scale(1, 1) translate(-50%); transform-origin: 50% 50%; }"
    "button:focus:not(:active)::after { animation: ripple 1s ease-out; }"
    "@keyframes ripple { 0% { transform: scale(0, 0); opacity: 0.5; } 20% { transform: scale(25, 25); opacity: 0.3; } 100% { transform: scale(40, 40); opacity: 0; } }"
    ".result { margin-top: 15px; padding: 15px; border-radius: 8px; display: none; animation: fadeIn 0.5s ease; font-weight: 500; }"
    "@keyframes fadeIn { from { opacity: 0; transform: translateY(-10px); } to { opacity: 1; transform: translateY(0); } }"
    ".success { background: rgba(46, 204, 113, 0.25); border-left: 4px solid #2ecc71; color: #2ecc71; }"
    ".error { background: rgba(231, 76, 60, 0.25); border-left: 4px solid #e74c3c; color: #e74c3c; }"
    ".instructions { line-height: 1.6; }"
    ".instructions p { margin-bottom: 12px; }"
    ".code { background: rgba(0,0,0,0.4); padding: 10px 15px; border-radius: 6px; font-family: monospace; margin: 10px 0; border-left: 3px solid #fdbb2d; }"
    ".footer { text-align: center; margin-top: 30px; color: rgba(255,255,255,0.7); font-size: 0.9rem; }"
    ".pulse { animation: pulse 2s infinite; }"
    "@keyframes pulse { 0% { box-shadow: 0 0 0 0 rgba(253, 187, 45, 0.7); } 70% { box-shadow: 0 0 0 10px rgba(253, 187, 45, 0); } 100% { box-shadow: 0 0 0 0 rgba(253, 187, 45, 0); } }"
    "</style>"
    "</head>"
    "<body>"
    "<div class=\"container\">"
    "<h1>🌐 网络唤醒(WoL)服务器</h1>"
    "<div class=\"card\">"
    "<h2>🚀 发送唤醒包</h2>"
    "<div class=\"form-group\">"
    "<label for=\"mac\">📶 MAC地址:</label>"
    "<input type=\"text\" id=\"mac\" placeholder=\"输入MAC地址 (如: b6:6f:9c:cc:d7:99)\" value=\"b6:6f:9c:cc:d7:99\">"
    "</div>"
    "<div class=\"form-group\">"
    "<label for=\"ip\">🌍 目标IP地址:</label>"
    "<input type=\"text\" id=\"ip\" placeholder=\"输入目标IP地址 (如: 192.168.31.255)\" value=\"192.168.31.255\">"
    "</div>"
    "<div class=\"btn-container\">"
    "<button id=\"wol-btn\" onclick=\"sendWoL()\" class=\"pulse\">🔔 发送唤醒包</button>"
    "</div>"
    "<div id=\"result\" class=\"result\"></div>"
    "</div>"
    "<div class=\"card\">"
    "<h2>📖 使用说明</h2>"
    "<div class=\"instructions\">"
    "<p>您可以直接通过浏览器访问以下URL格式来发送唤醒包：</p>"
    "<div class=\"code\">http://ip:port/wol?mac=XX:XX:XX:XX:XX:XX&ip=Y.Y.Y.Y</div>"
    "<div class=\"code\">http://ip:port/wol?mac=XX-XX-XX-XX-XX-XX&ip=Y.Y.Y.Y</div>"
    "<p>将上述URL中的MAC地址和IP地址替换为您需要的值即可。</p>"
    "<p><strong>提示：</strong>使用广播地址(如192.168.1.255)可以唤醒同一子网内的设备。</p>"
    "</div>"
    "</div>"
    "<div class=\"footer\">"
    "<p>Powered by C WoL Server | 网络唤醒工具</p>"
    "</div>"
    "</div>"
    "<script>"
    "function sendWoL() {"
    "  const btn = document.getElementById('wol-btn');"
    "  const originalText = btn.innerHTML;"
    "  const mac = document.getElementById('mac').value;"
    "  const ip = document.getElementById('ip').value;"
    "  "
    "  // 禁用按钮并显示加载状态"
    "  btn.disabled = true;"
    "  btn.innerHTML = '⏳ 发送中...';"
    "  btn.classList.remove('pulse');"
    "  "
    "  // 发送请求"
    "  fetch('/wol?mac=' + encodeURIComponent(mac) + '&ip=' + encodeURIComponent(ip))"
    "  .then(response => response.json())"
    "  .then(data => {"
    "    const result = document.getElementById('result');"
    "    if (data.success) {"
    "      result.textContent = '✅ ' + data.message;"
    "      result.className = 'result success';"
    "      // 成功时按钮短暂显示成功状态"
    "      btn.innerHTML = '✅ 发送成功';"
    "      btn.style.background = 'linear-gradient(135deg, #2ecc71, #27ae60)';"
    "      setTimeout(() => {"
    "        btn.innerHTML = originalText;"
    "        btn.style.background = '';"
    "      }, 2000);"
    "    } else {"
    "      result.textContent = '❌ 错误: ' + data.error;"
    "      result.className = 'result error';"
    "      // 失败时按钮短暂显示失败状态"
    "      btn.innerHTML = '❌ 发送失败';"
    "      btn.style.background = 'linear-gradient(135deg, #e74c3c, #c0392b)';"
    "      setTimeout(() => {"
    "        btn.innerHTML = originalText;"
    "        btn.style.background = '';"
    "      }, 2000);"
    "    }"
    "    result.style.display = 'block';"
    "  })"
    "  .catch(error => {"
    "    const result = document.getElementById('result');"
    "    result.textContent = '❌ 网络错误: ' + error.message;"
    "    result.className = 'result error';"
    "    result.style.display = 'block';"
    "    btn.innerHTML = '❌ 请求失败';"
    "    btn.style.background = 'linear-gradient(135deg, #e74c3c, #c0392b)';"
    "    setTimeout(() => {"
    "      btn.innerHTML = originalText;"
    "      btn.style.background = '';"
    "    }, 2000);"
    "  })"
    "  .finally(() => {"
    "    // 重新启用按钮"
    "    setTimeout(() => {"
    "      btn.disabled = false;"
    "      btn.classList.add('pulse');"
    "    }, 2000);"
    "  });"
    "}"
    "// 添加键盘支持"
    "document.addEventListener('keypress', function(event) {"
    "  if (event.key === 'Enter') {"
    "    sendWoL();"
    "  }"
    "});"
    "// 输入框获取焦点时移除按钮脉冲效果"
    "document.getElementById('mac').addEventListener('focus', function() {"
    "  document.getElementById('wol-btn').classList.remove('pulse');"
    "});"
    "document.getElementById('ip').addEventListener('focus', function() {"
    "  document.getElementById('wol-btn').classList.remove('pulse');"
    "});"
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
