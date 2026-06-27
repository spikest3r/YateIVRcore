# IVR Core for Yate

A lightweight, plugin-based IVR (Interactive Voice Response) engine for the [Yate](https://yate.ro) telephony engine, written in C++.

The core routes calls to the right IVR application, and exposes a small set of helpers to interact with the user like speech syntesis, WAV playback etc. Everything else like menu structure, what happens on each DTMF digit, what gets said, when to hang up lives in plugins.

Below is simplified visualization of the core architecture.

```
Caller dials extension
        │
        ▼
   [ IVR Core ]──── owns the Yate extmodule socket connection
        │            routes call.route / chan.dtmf / chan.hangup
        │            to the right plugin by extension (GetAppID)
        ▼
   [ Plugin DLL ]──── decides what happens on each digit
        │             calls back into Core for: speech, playback, HTTP
        ▼
   Core helpers: SpeakToWav · PlayWav · HttpGet
```

## Why a plugin architecture

A single hardcoded script can run one bot, on one extension, with one fixed set of menu options. Adding a second bot means reworking the whole thing — there is no straightforward and easy way to implement multiple bots.

This project flips that: the core defines a stable plugin boundary, and adding a new bot means writing a new DLL and dropping it in a folder. No core changes required. Two bots on two different extensions can run side by side, each handling its own calls independently, sharing the same core connection to Yate.

## Architecture

### Core responsibilities

- Owns the single TCP connection to Yate's `extmodule` interface
- Parses and dispatches Yate protocol messages (`call.route`, `chan.dtmf`, `chan.hangup`, etc.)
- Loads plugin DLLs and routes incoming calls to the correct plugin based on the dialed extension
- Tracks active calls (`call_id` ↔ caller) to prevent a single caller from holding multiple simultaneous sessions
- Exposes a small, stable C API (`CoreApi`) that plugins use to actually do things

### Plugin responsibilities

- Decide what happens when a call starts (`OnCallStart`)
- Decide what happens on each DTMF digit (`OnDtmf`)
- Decide what happens when a call ends (`OnCallEnd`)
- Declare which extension(s) it owns (`GetAppID`)

Plugins never touch the Yate socket directly, never parse protocol messages, and never link against anything beyond plain C types at the boundary.

## CoreApi reference

The core hands every plugin a `CoreApi` struct at load time, via `InitPlugin`. These are the only functions a plugin can use to affect the outside world.

```cpp
extern "C" {

    // Send a raw line to the Yate extmodule socket.
    typedef void (*SendFn)(const char* msg);

    // Masquerade the given call onto an arbitrary Yate route
    // (e.g. "tone/silence", "lateroute/102", "wave/play/C%z/path/file.wav").
    // Handles colon-escaping internally — pass raw paths, including drive letters.
    typedef void (*MasqueradeToFn)(const char* call_id, const char* target);

    // Play a wav file on the given call. Thin wrapper similar to MasqueradeTo
    // with the wave/play/ scheme applied automatically.
    typedef void (*PlayWavFn)(const char* call_id, const char* wav_path);

    // Terminate the given call.
    typedef void (*HangupFn)(const char* call_id);

    // Synthesize speech from text to a wav file (8kHz mono PCM, matching
    // Yate's expected playback format). Blocking call.
    typedef bool (*SpeakToWavFn)(const char* text, const char* output_path);

    // Perform an HTTPS GET request. Response body is copied into outBuffer;
    // returns false if the request fails or the response exceeds bufferSize.
    typedef bool (*HttpGetFn)(const char* host, const char* path, char* outBuffer, int bufferSize);

    struct CoreApi {
        SendFn Send;
        MasqueradeToFn MasqueradeTo;
        PlayWavFn PlayWav;
        HangupFn Hangup;
        SpeakToWavFn SpeakToWav;
        HttpGetFn HttpGet;
    };

    // Called once by the core after LoadLibrary, before any call routing begins.
    typedef void (*InitPluginFn)(const CoreApi* api);

    // Declares which extension this plugin handles (e.g. 801).
    // Used by the core to route incoming calls to the right plugin.
    typedef int (*GetAppIDFn)();

    // Called when a call for this plugin's extension is answered.
    typedef void (*OnCallStartFn)(const char* call_id);

    // Called for every DTMF digit pressed during the call.
    typedef void (*OnDtmfFn)(const char* call_id, const char* digit);

    // Called when the call ends (chan.hangup received for this call_id).
    typedef void (*OnCallEndFn)(const char* call_id);
}
```

### Required plugin exports

Every plugin DLL must export, via `extern "C" __declspec(dllexport)`:

| Export | Signature | Purpose |
|---|---|---|
| `InitPlugin` | `void(const CoreApi*)` | Receives the core's function table |
| `GetAppID` | `int()` | Declares the extension this plugin owns |
| `OnCallStart` | `void(const char*)` | Called when a call for this extension is answered |
| `OnDtmf` | `void(const char*, const char*)` | Called on each DTMF digit |
| `OnCallEnd` | `void(const char*)` | Called on hangup |

A plugin missing any of these is rejected at load time and not registered for routing.

## Building 

1. Create new Visual Studio Empty C++ Project and import files from ```src``` and ```include```.
2. Build the project.
3. In the output folder you will find ```.exe``` file. This is the core.

### Running

1. Create ```apps``` folder in the directory where your ```.exe``` is located.
2. Copy DLL plugins into the folder
3. Locate ```conf.d``` in Yate installation directory.
4. Create ```extmodule.conf``` file and paste following:

```
[listener tcp]
type=tcp
addr=127.0.0.1
port=5040
role=global
```

5. Restart Yate service.

6. Run core executable. At this point, core and Yate are connected and synced. Try calling number you assigned to the plugin to verify.

After service restart, if core cannot connect to Yate, try restarting your PC/server. In my case, this helped.

### Writing a plugin

1. Create new Visual Studio Empty C++ Project and import ```main.cpp``` from ```ivr_example``` folder and ```coreapi.h``` from ```include``` folder.
2. Right-click on the project, then Properties -> Configuration Properties -> General -> Set ```Configuration Type``` to ```Dynamic Library (.dll)```.
3. Build project. In the output folder you will find ```.dll``` file. This is the plugin. Copy it to the ```apps``` folder and restart core to load it.

If plugin fails to load, make sure all methods required are exported.