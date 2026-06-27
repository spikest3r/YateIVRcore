#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <ctime>

#pragma comment(lib, "ws2_32.lib")

SOCKET g_sock = INVALID_SOCKET;

bool Connect(const std::string& host, int port) {
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed\n";
        return false;
    }

    g_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_sock == INVALID_SOCKET) {
        std::cerr << "socket() failed: " << WSAGetLastError() << "\n";
        WSACleanup();
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(static_cast<u_short>(port));
    inet_pton(AF_INET, host.c_str(), &addr.sin_addr);

    if (connect(g_sock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "connect() failed: " << WSAGetLastError() << "\n";
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
        WSACleanup();
        return false;
    }

    std::cout << "Connected to " << host << ":" << port << "\n";
    return true;
}

void Disconnect() {
    if (g_sock != INVALID_SOCKET) {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
    }
    WSACleanup();
    std::cout << "Disconnected.\n";
}

bool Send(const std::string& msg) {
    std::string framed = msg + "\n";
    int sent = send(g_sock, framed.c_str(), static_cast<int>(framed.size()), 0);
    if (sent == SOCKET_ERROR) {
        std::cerr << "send() failed: " << WSAGetLastError() << "\n";
        return false;
    }
    std::cout << "SEND: " << msg << "\n";
    return true;
}

std::map<std::string, std::string> ParseParams(const std::string& raw) {
    std::map<std::string, std::string> params;
    size_t start = 0;
    while (start <= raw.size()) {
        size_t pos = raw.find(':', start);
        std::string p = (pos == std::string::npos) ? raw.substr(start) : raw.substr(start, pos - start);

        size_t eq = p.find('=');
        if (eq != std::string::npos) {
            std::string k = p.substr(0, eq);
            std::string v = p.substr(eq + 1);
            params[k] = v;
        }

        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return params;
}

bool RecvLoop() {
    char buffer[4096];
    std::string leftover;

    std::map<std::string, bool> pending = {};

    while (true) {
        int n = recv(g_sock, buffer, sizeof(buffer), 0);
        if (n == 0) {
            std::cout << "Engine disconnected.\n";
            return false;
        }
        if (n < 0) {
            std::cerr << "recv() failed: " << WSAGetLastError() << "\n";
            return false;
        }

        leftover.append(buffer, n);

        size_t pos;
        while ((pos = leftover.find('\n')) != std::string::npos) {
            std::string line = leftover.substr(0, pos);
            leftover.erase(0, pos + 1);
            if (!line.empty()) {
                std::cout << "RECV: " << line << "\n";

                // split line into parts
                std::string rest = line.substr(std::string("%%>message:").size());

                std::vector<std::string> parts;
                size_t start = 0, count = 0;
                while (count < 4) {
                    size_t pos = rest.find(':', start);
                    if (pos == std::string::npos) break;
                    parts.push_back(rest.substr(start, pos - start));
                    start = pos + 1;
                    ++count;
                }
                parts.push_back(rest.substr(start));

                if (parts.size() >= 4) {
                    auto msg_id = parts[0];
                    auto msg_name = parts[2];
                    auto params_raw = parts.size() > 4 ? parts[4] : "";
                    auto params = ParseParams(params_raw);

                    if (msg_name == "call.route") {
                        auto called = params["called"];
                        auto call_id = params["id"];
                        std::cout << "Calling: " << called << std::endl;

                        if (called == "801") {
                            // TODO: Transfer to core handler

                            // send answer message
                            char buf[4096];
                            const auto target = "wave/play/C%z/VoIP_Assets/welcome_menu_801.wav";
                            sprintf_s(buf, 4096, "%%%%<message:%s:true::%s:autoanswer=yes", msg_id.c_str(), target);
                            Send(buf);

                            // add to pending list
                            // TODO: Make this into state machine per call per bot
                            pending[call_id] = true;

                            // setup dtmf listener
                            sprintf_s(buf, 4096, "%%%%>install:10:chan.dtmf::id=%s", call_id.c_str());
                            Send(buf);
                        }
                        else {
                            // reject call from core
                            char buf[4096];
                            sprintf_s(buf, 4096, "%%%%<message:%s:false::", call_id.c_str());
                            Send(buf);
                        }
                    }
                    else if (msg_name == "chan.dtmf") {
                        // handle dtmf callbacks
                        auto call_id = params["id"];
                        auto digit = params["text"];

                        // confirm
                        char buf[4096];
                        sprintf_s(buf, 4096, "%%%%<message:%s:true::", call_id.c_str());
                        Send(buf);

                        if (pending[call_id]) {
                            pending[call_id] = false;

                            std::string mid = "ivr_exec_" + std::to_string(static_cast<long long>(std::time(nullptr)));
                            auto time_int = static_cast<long long>(std::time(nullptr));

                            if (digit == "1") {
                                sprintf_s(buf, 4096, "%%%%>message:%s:%lld:chan.masquerade::id=%s:message=call.execute:callto=%s",
                                    mid.c_str(), time_int, call_id.c_str(), "wave/play/C%z/VoIP_Assets/fetching.wav");
                                Send(buf);
                            }
                        }
                    }
                }
            }
        }
    }
}

bool ReloadRegexRoute() {
    SOCKET reloadSock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (reloadSock == INVALID_SOCKET) {
        std::cerr << "reload socket() failed: " << WSAGetLastError() << "\n";
        return false;
    }

    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(5038);
    inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);

    if (connect(reloadSock, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) == SOCKET_ERROR) {
        std::cerr << "reload connect() failed: " << WSAGetLastError() << "\n";
        closesocket(reloadSock);
        return false;
    }

    std::string cmd = "reload regexroute\r\n";
    send(reloadSock, cmd.c_str(), static_cast<int>(cmd.size()), 0);
    std::cout << "SEND (rmanager): reload regexroute\n";

    Sleep(1000);
    closesocket(reloadSock);
    return true;
}

int main() {
    const std::string HOST = "192.168.0.117";
    const int PORT = 5040;

    if (!Connect(HOST, PORT)) {
        return 1;
    }

    Send("%%>setlocal:restart:false");
    Send("%%>install:15:call.route");
    ReloadRegexRoute(); // reload routing to init ivr core

    RecvLoop();

    Disconnect();
    return 0;
}