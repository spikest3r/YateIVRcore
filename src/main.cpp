#include "includes.h"
#include "connection.h"
#include "vm-inc/helpers.h"
#include "vm-inc/vm.h"
#include "ivrapp.h"
#include "loader.h"

#pragma comment(lib, "ws2_32.lib")

SOCKET g_sock = INVALID_SOCKET;

std::unordered_map<std::string, IVRApp> apps;
std::unordered_map<std::string, std::string> activeCallers;
std::unordered_map<std::string, VMExecutionData> appVM;
std::unordered_map<std::string, LoadedExtension> loadedExtensions;

VMExecutionData InitVM(VMProgramData* progData) {
    VMExecutionData data;
    data.halt = false;
    data.stack.clear();
    data.pcStack.clear();
    data.PC = 0;
    data.variables.resize(progData->variableCount);
    return std::move(data);
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

                            VMProgramData* data = &apps[called].progData;
                            appVM[call_id] = std::move(InitVM(data));
                            appVM[call_id].callInfo.call_id = call_id;

                            // call app api function
                            auto pc = apps[called].onCallPC;
                            run(pc, data, &appVM[call_id]);

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
                            auto called = activeCallers[call_id];
                            VMProgramData* data = &apps[called].progData;
                            VMExecutionData* execData = &appVM[call_id];
                            execData->callInfo.lastDtmfCommand = digit[0];
                            auto pc = apps[called].onDtmfPC;
                            run(pc, data, execData);
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
                            auto called = activeCallers[call_id];
                            VMProgramData* data = &apps[called].progData;
                            auto pc = apps[called].onHangupPC;
                            run(pc, data, &appVM[call_id]);

                            appVM.erase(call_id);
                            activeCallers.erase(call_id);
                        }
                        else {
                            // caller not found / no active session
                            std::cerr << "Undefined caller" << std::endl;
                        }
                    }
                }
            }
        }
    }
}

int main() {
    // load all core extensions
    std::unordered_map<std::string, LoadedExtension> loadedExtensions;
    loadExtensions(funcMap, funcList, loadedExtensions);

    // find all scripts and map them to extensions
    std::unordered_map<int, std::string> scripts;
    int status = loadScripts(scripts);
    if (status != 0) return status;

    // Connect to Yate
    const std::string HOST = "127.0.0.1";
    const int PORT = 5040;

    if (!Connect(g_sock, HOST, PORT)) {
        return 1;
    }

    // compile all scripts
    compileScripts(scripts, apps);

    Send(g_sock, "%%>setlocal:restart:false");
    Send(g_sock, "%%>install:15:call.route");
    ReloadRegexRoute(); // reload routing to init ivr core

    RecvLoop();

    Disconnect(g_sock);

    // Unload all extensions
    for (auto& [name, ext] : loadedExtensions) {
        if (ext.handle) {
            FreeLibrary(ext.handle);
        }
    }
    loadedExtensions.clear();

    return 0;
}