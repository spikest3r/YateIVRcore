#include "helpers.h"

#pragma comment(lib, "sapi.lib")

std::wstring ToWString(const char* str) {
    if (!str) return L"";
    int size = MultiByteToWideChar(CP_UTF8, 0, str, -1, nullptr, 0);
    std::wstring result(size, 0);
    MultiByteToWideChar(CP_UTF8, 0, str, -1, &result[0], size);
    result.resize(size - 1); // strip the null terminator MultiByteToWideChar counts
    return result;
}

bool SpeakToWav(const char* text, const char* output_path) {
    HRESULT hr = CoInitialize(nullptr);
    if (FAILED(hr)) return false;

    ISpVoice* pVoice = nullptr;
    hr = CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, (void**)&pVoice);
    if (FAILED(hr)) {
        CoUninitialize();
        return false;
    }

    // Set up a WAV file stream as the audio output instead of speakers
    ISpStream* pStream = nullptr;
    hr = CoCreateInstance(CLSID_SpStream, nullptr, CLSCTX_ALL, IID_ISpStream, (void**)&pStream);
    if (FAILED(hr)) {
        pVoice->Release();
        CoUninitialize();
        return false;
    }

    CSpStreamFormat fmt;
    fmt.AssignFormat(SPSF_8kHz16BitMono); // 8kHz mono pcm_s16le target

    hr = pStream->BindToFile(
        ToWString(output_path).c_str(),
        SPFM_CREATE_ALWAYS,
        &fmt.FormatId(),
        fmt.WaveFormatExPtr(),
        SPFEI_ALL_EVENTS
    );

    if (FAILED(hr)) {
        pStream->Release();
        pVoice->Release();
        CoUninitialize();
        return false;
    }

    hr = pVoice->SetOutput(pStream, TRUE);

    hr = pVoice->Speak(ToWString(text).c_str(), SPF_DEFAULT, nullptr);

    pStream->Close();
    pStream->Release();
    pVoice->Release();
    CoUninitialize();

    return SUCCEEDED(hr);
}