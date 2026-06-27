#include "coreapi.h"
#include <iostream>
#include <string>
#include <map>

static const CoreApi* g_core = nullptr;

std::map<std::string, bool> pending = {}; // caller can use only one dtmf option

extern "C" __declspec(dllexport) void InitPlugin(const CoreApi* api) {
    g_core = api;
}

extern "C" __declspec(dllexport) int GetAppID() {
    return 800;
}

extern "C" __declspec(dllexport) void OnCallStart(const char* call_id) {
    g_core->PlayWav(call_id, "C:/VoIP_Assets/welcome_menu.wav");
    pending[call_id] = true;
}

extern "C" __declspec(dllexport) void OnHangUp(const char* call_id) {
    pending[call_id] = false;
}

extern "C" __declspec(dllexport) void OnDtmf(const char* call_id, const char* digit) {
    if (!pending[call_id]) return;

    pending[call_id] = false;

    if (strcmp(digit, "1") == 0) {
        g_core->MasqueradeTo(call_id, "lateroute/101");
    } else if (strcmp(digit, "2") == 0) {
        g_core->MasqueradeTo(call_id, "lateroute/102");
    } else if (strcmp(digit, "3") == 0) {
        g_core->PlayWav(call_id, "C:/VoIP_Assets/music.wav");
    }
}