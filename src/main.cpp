#include "includes.h"
#include "connection.h"
#include "helpers.h"
#include "coreapi.h"
#include "ivrapp.h"

#pragma comment(lib, "ws2_32.lib")

SOCKET g_sock = INVALID_SOCKET;

std::map<std::string, IVRApp> apps;
std::map<std::string, std::string> activeCallers;

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

                        if (apps.contains(called)) {
                            // app is registered

                            // send answer message
                            char buf[4096];
                            sprintf_s(buf, 4096, "%%%%<message:%s:true::%s:autoanswer=yes", msg_id.c_str(), "tone/silence");
                            Send(g_sock, buf);

                            // install handlers
                            // dtmf
                            sprintf_s(buf, 4096, "%%%%>install:10:chan.dtmf::id=%s", call_id.c_str());
                            Send(g_sock, buf);
                            // hangup
                            sprintf_s(buf, 4096, "%%%%>install:10:chan.hangup::id=%s", call_id.c_str());
                            Send(g_sock, buf);

                            // call app api function
                            apps[called].api.OnCallStart(call_id.c_str());

                            activeCallers[call_id] = called;
                        }
                        else {
                            // reject call from core
                            char buf[4096];
                            sprintf_s(buf, 4096, "%%%%<message:%s:false::", msg_id.c_str());
                            Send(g_sock, buf);
                        }
                    }
                    else if (msg_name == "chan.dtmf") {
                        // handle dtmf callbacks
                        auto call_id = params["id"];
                        auto digit = params["text"];

                        // confirm
                        char buf[4096];
                        sprintf_s(buf, 4096, "%%%%<message:%s:true::", call_id.c_str());
                        Send(g_sock, buf);

                        auto it = activeCallers.find(call_id);
                        if (it != activeCallers.end()) {
                            apps[activeCallers[call_id]].api.OnDtmf(call_id.c_str(), digit.c_str());
                        }
                        else {
                            // caller not found / no active session
                            std::cerr << "Undefined caller" << std::endl;
                        }

                    }
                    else if (msg_name == "chan.hangup") {
                        auto call_id = params["id"];
                        auto it = activeCallers.find(call_id);
                        if (it != activeCallers.end()) {
                            apps[activeCallers[call_id]].api.OnHangUp(call_id.c_str());
                        }
                        else {
                            // caller not found / no active session
                            std::cerr << "Undefined caller" << std::endl;
                        }
                        activeCallers.erase(call_id);
                    }
                }
            }
        }
    }
}

void PlayWav(const char* call_id, const char* wav_path) {
    char buf[4096];

    std::string mid = "ivr_exec_" + std::to_string(static_cast<long long>(std::time(nullptr)));
    auto time_int = static_cast<long long>(std::time(nullptr));
    
    auto target = "wave/play/" + EscapeColon(wav_path);

    sprintf_s(buf, 4096, "%%%%>message:%s:%lld:chan.masquerade::id=%s:message=call.execute:callto=%s",
        mid.c_str(), time_int, call_id, target.c_str());

    Send(g_sock, buf);
}

void HangUp(const char* call_id) {
    char buf[4096];

    std::string mid = "ivr_exec_" + std::to_string(static_cast<long long>(std::time(nullptr)));
    auto time_int = static_cast<long long>(std::time(nullptr));

    sprintf_s(buf, 4096, "%%%%>message:%s:%lld:call.drop::id=%s",
        mid.c_str(), time_int, call_id);
    Send(g_sock, buf);
}

void MasqueradeTo(const char* call_id, const char* target) {
    auto time_int = static_cast<long long>(std::time(nullptr));
    std::string mid = "ivr_exec_" + std::to_string(time_int);

    auto str = EscapeColon(target);

    char buf[4096];
    sprintf_s(buf, 4096, "%%%%>message:%s:%lld:chan.masquerade::id=%s:message=call.execute:callto=%s",
        mid.c_str(), time_int, call_id, str.c_str());

    Send(g_sock, buf);
}

int main() {
    try {
        if (fs::exists(APPS) && fs::is_directory(APPS)) {
            for (const auto& entry : fs::directory_iterator(APPS)) {
                if (fs::is_regular_file(entry.status())) {
                    auto ext = entry.path().extension();
                    if (ext != ".dll") continue;

                    std::string file = entry.path().string();
                    auto pluginName = entry.path().filename().string();
                    HMODULE dll = LoadLibraryA(file.c_str());

                    if (!dll) {
                        std::cerr << "Failed to load " << pluginName << " (1)" << std::endl;
                        continue;
                    }

                    auto InitPlugin = (InitPluginFn)GetProcAddress(dll, "InitPlugin");
                    auto OnCallStart = (OnCallStartFn)GetProcAddress(dll, "OnCallStart");
                    auto OnDtmf = (OnDtmfFn)GetProcAddress(dll, "OnDtmf");
                    auto OnHangUp = (OnHangUpFn)GetProcAddress(dll, "OnHangUp");
                    auto GetAppID = (GetAppIDFn)GetProcAddress(dll, "GetAppID");

                    if (!InitPlugin || !OnCallStart || !OnDtmf || !GetAppID || !OnHangUp) {
                        std::cerr << "Failed to load " << pluginName << " (2)" << std::endl;
                        FreeLibrary(dll);
                        continue;
                    }

                    auto appId = std::to_string(GetAppID());

                    if (apps.contains(appId)) {
                        std::cerr << "Duplicate App ID " << appId << " for plugin " << pluginName << std::endl;
                        FreeLibrary(dll);
                        continue;
                    }

                    CoreApi api;
                    api.PlayWav = &PlayWav;
                    api.HangUp = &HangUp;
                    api.HttpGet = &HttpGet;
                    api.SpeakToWav = &SpeakToWav;
                    api.MasqueradeTo = &MasqueradeTo;

                    InitPlugin(&api);

                    AppAPI pluginApi;
                    pluginApi.OnCallStart = OnCallStart;
                    pluginApi.OnHangUp = OnHangUp;
                    pluginApi.OnDtmf = OnDtmf;

                    IVRApp app;
                    app.api = pluginApi;
                    app.dll = dll;

                    apps[appId] = app;

                    std::cout << "Loaded " << entry.path().filename().string() << std::endl;
                }
            }
        }
        else {
            std::cerr << "Provided path is not a valid directory.\n";
            return -1;
        }
    }
    catch (const fs::filesystem_error& e) {
        std::cerr << "Error: " << e.what() << '\n';
        return -1;
    }

    const std::string HOST = "192.168.0.117";
    const int PORT = 5040;

    if (!Connect(g_sock, HOST, PORT)) {
        return 1;
    }

    Send(g_sock, "%%>setlocal:restart:false");
    Send(g_sock, "%%>install:15:call.route");
    ReloadRegexRoute(); // reload routing to init ivr core

    RecvLoop();

    Disconnect(g_sock);

    for (const auto& app : apps) {
        FreeLibrary(app.second.dll);
    }

    return 0;
}