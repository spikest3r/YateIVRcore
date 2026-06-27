#include "helpers.h"

#pragma comment(lib, "winhttp.lib")

bool HttpGetInternal(const std::wstring& host, const std::wstring& path, std::string& outBody) {
    HINTERNET hSession = WinHttpOpen(L"IVRCore/1.0",
        WINHTTP_ACCESS_TYPE_DEFAULT_PROXY,
        WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    HINTERNET hConnect = WinHttpConnect(hSession, host.c_str(), INTERNET_DEFAULT_HTTPS_PORT, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", path.c_str(),
        nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
        WINHTTP_FLAG_SECURE);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    WinHttpSetTimeouts(hRequest, 10000, 10000, 10000, 10000);

    bool ok = WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0,
        WINHTTP_NO_REQUEST_DATA, 0, 0, 0);
    if (ok) ok = WinHttpReceiveResponse(hRequest, nullptr);

    if (ok) {
        char buffer[4096];
        DWORD bytesRead = 0;
        while (WinHttpReadData(hRequest, buffer, sizeof(buffer), &bytesRead) && bytesRead > 0) {
            outBody.append(buffer, bytesRead);
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return ok;
}

bool HttpGet(const char* host, const char* path, char* outBuffer, int bufferSize) {
    std::wstring whost(host, host + strlen(host));
    std::wstring wpath(path, path + strlen(path));

    std::string body;
    if (!HttpGetInternal(whost, wpath, body)) return false;

    if ((int)body.size() >= bufferSize) return false; // truncation guard
    strcpy_s(outBuffer, bufferSize, body.c_str());
    return true;
}