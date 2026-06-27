#pragma once
// shared header, included by both core and DLLs

extern "C" {
    typedef void (*PlayWavFn)(const char* call_id, const char* wav_path);
    typedef void (*HangUpFn)(const char* call_id);
    typedef bool (*SpeakToWavFn)(const char* text, const char* output_path);
    typedef bool (*HttpGetFn)(const char* host, const char* path, char* outBuffer, int bufferSize);
    typedef void (*MasqueradeToFn)(const char* call_id, const char* target);

    struct CoreApi {
        PlayWavFn PlayWav;
        HangUpFn HangUp;
        SpeakToWavFn SpeakToWav;
        HttpGetFn HttpGet;
        MasqueradeToFn MasqueradeTo;
    };

    typedef void (*InitPluginFn)(const CoreApi* api);
    typedef int (*GetAppIDFn)();

    typedef void (*OnCallStartFn)(const char* call_id);
    typedef void (*OnDtmfFn)(const char* call_id, const char* digit);
    typedef void (*OnCallEndFn)(const char* call_id);
    typedef void (*OnHangUpFn)(const char* call_id);
}