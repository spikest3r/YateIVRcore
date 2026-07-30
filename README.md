# IVR Core for Yate

A lightweight IVR (Interactive Voice Response) engine for the [Yate](https://yate.ro) telephony engine, written in C++, that runs call-handling logic as [Lumen](https://github.com/spikest3r/LumenLang) scripts.

The core routes calls to the right app, embeds a Lumen VM instance per call, and exposes speech synthesis, WAV playback, HTTP, and other host functionality to scripts as native functions. Menu structure, DTMF handling, and call flow all live in a small `.lmn` script — no C++ required to write an IVR app.

```
Caller dials extension
        │
        ▼
   [ IVR Core ]──── owns the Yate extmodule socket connection
        │            routes call.route / chan.dtmf / chan.hangup
        │            to the right app by extension (filename match)
        ▼
   [ Lumen VM instance ]──── one per active call, alive for its lifetime
        │                    runs onCall / onDtmf / onHangup routines
        ▼
   Native functions: playWav · speakToWav · getDtmf · masqueradeTo · hangUp · …
        │
        ▼
   [ Extension DLLs ]──── optional, add more native functions to the VM
```

## Why Lumen scripts

Each extension is a `.lmn` script, matched by filename. Adding a bot means writing a script and dropping it in `apps/` — no compiling, no linking, no core changes.

Call isolation falls out of this naturally. Each call gets its own VM instance (`ExecutionData`), so "global" variables in a script are only global for the lifetime of that one call — there's no shared state to key by `call_id`, because there's nothing shared. The instance is created on `call.route` and destroyed on `chan.hangup`.

Most apps only need the built-in native functions. For host-specific functionality that doesn't belong in the VM core — sending a wake-on-LAN packet, controlling a Windows service — a native-function extension adds it without touching core or the compiler. See [Extension DLLs](#extension-dlls-native-function-extensions) below.

## Architecture

### Core responsibilities

- Owns the single TCP connection to Yate's `extmodule` interface
- Parses and dispatches Yate protocol messages (`call.route`, `chan.dtmf`, `chan.hangup`, etc.)
- Compiles and loads the `.lmn` script matching the dialed extension
- Creates a Lumen VM instance per active call, keyed by `call_id`, and tracks which extension owns it
- Dispatches into the script's `onCall` / `onDtmf` / `onHangup` routines as the matching Yate messages arrive
- Loads native-function extension DLLs from `extensions/` at startup and merges their functions into the VM

### App (`.lmn` script) responsibilities

- Define `onCall`, `onDtmf`, and `onHangup` routines
- Decide menu structure, DTMF handling, and call flow using ordinary Lumen control flow and globals
- Call native functions (built-in or DLL-provided) to actually affect the outside world — play audio, synthesize speech, query state, transfer or hang up the call

Scripts never touch the Yate socket directly and never parse protocol messages — all of that stays in the core.

## Built-in native functions

These are available to every app without any extension DLL:

| Function | Args | Purpose |
|---|---|---|
| `getDtmf` | `&out` | Writes the most recent DTMF digit into `out` |
| `speakToWav` | `text, output_path` | Synthesizes speech to a wav file (blocking) |
| `playWav` | `path` | Plays a wav file on the call |
| `masqueradeTo` | `target` | Masquerades the call onto an arbitrary Yate route |
| `hangUp` | — | Terminates the call |

Alongside Lumen's own stdlib natives (`println`, `str2int`, `int2str`, `strlen`, `substr`, `httpRequest`, etc.) — see the [Lumen docs](https://lumen.olehsheremeta.com) for the full list.

## Example app

```lumen
routine onCall
    playWav 'C:/VoIP_Assets/welcome_menu.wav'
endroutine

routine onDtmf
    getDtmf &str
    str2int str, &digit

    if digit == 1
        speakToWav 'Hello world', 'C:/VoIP_Assets/temp.wav'
        playWav 'C:/VoIP_Assets/temp.wav'
    else
        if digit == 2
            playWav 'C:/VoIP_Assets/music.wav'
        endif
    endif
endroutine

routine onHangup
    # empty
endroutine
```

## Extension DLLs (native function extensions)

An extension DLL adds new native functions to the VM without modifying core or the compiler. This is the mechanism for host-specific functionality — service control, wake-on-LAN, or anything else a script needs that isn't general enough to belong in the VM itself.

At startup, the core scans `extensions/` for `.dll` files, loads each one, and calls its `RegisterNatives` export. Each extension supplies both the runtime implementation and the metadata the compiler needs to resolve calls by name.

`RegisterNatives` receives two things to fill in: `funcMap`, mapping an opcode to the actual native implementation (matching the same signature every built-in native uses); and `descriptors`, a list telling the compiler that a given function name — as written in `.lmn` source — resolves to that opcode and expects a specific number of arguments. Together, these are the same two pieces of information a built-in native is declared with, just supplied at load time instead of compiled into the core.

A DLL missing the `RegisterNatives` export, or one that fails to load, is skipped; the core logs it and continues without it.

### Choosing an opcode

Opcodes are assigned by the extension author. There's currently no automatic collision detection between extensions or against the core's own native table — if you're writing more than one extension, keep track of which opcodes are already in use.

## Building

1. Create a new Visual Studio Empty C++ Project and import files from `src` and `include`.
2. Build the project.
3. In the output folder you'll find the `.exe` file — this is the core.

### Running

1. Create an `apps` folder next to the core executable, and an `extensions` folder alongside it.
2. Drop `.lmn` scripts into `apps/`, named after the extension they should handle (e.g. `800.lmn` for extension 800).
3. Drop any native-function extension DLLs into `extensions/`.
4. Locate `conf.d` in your Yate installation directory.
5. Create an `extmodule.conf` file there with:

```
[listener tcp]
type=tcp
addr=127.0.0.1
port=5040
role=global
```

6. Restart the Yate service.
7. Run the core executable. At this point, core and Yate are connected and synced. Call the extension matching one of your scripts to verify.

If the core can't connect to Yate after a service restart, try restarting your PC/server — this has resolved connection issues in practice.

### Writing an app

1. Create a `.lmn` script defining `onCall`, `onDtmf`, and `onHangup` routines.
2. Name the file after the extension it should handle (e.g. `801.lmn`).
3. Drop it in `apps/` and restart the core to load it.

### Writing a native-function extension

1. Create a new Visual Studio Empty C++ Project, set `Configuration Type` to `Dynamic Library (.dll)`.
2. Import `extension.cpp` from the `ivr_example` folder — this is where you add your native functions.
3. Build, then copy the resulting `.dll` into `extensions/` and restart the core to load it.