#include "connection.h"

bool Connect(SOCKET& g_sock, const std::string& host, int port) {
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

void Disconnect(SOCKET& g_sock) {
    if (g_sock != INVALID_SOCKET) {
        closesocket(g_sock);
        g_sock = INVALID_SOCKET;
    }
    WSACleanup();
    std::cout << "Disconnected.\n";
}

bool Send(SOCKET& g_sock, const std::string& msg) {
    std::string framed = msg + "\n";
    int sent = send(g_sock, framed.c_str(), static_cast<int>(framed.size()), 0);
    if (sent == SOCKET_ERROR) {
        std::cerr << "send() failed: " << WSAGetLastError() << "\n";
        return false;
    }
    std::cout << "SEND: " << msg << "\n";
    return true;
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