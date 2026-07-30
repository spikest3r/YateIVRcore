#pragma once
#include "includes.h"
#include "ivrapp.h"
#include "vm-inc/types.h"
#include "vm-inc/helpers.h"
#include "vm-inc/compiler.h"
#include "vm-inc/vm.h"

struct NativeDescriptor {
    std::string name;
    uint8_t opcode;
    uint8_t argCount;
};

using RegisterNativesFn = void(*)(std::unordered_map<int, NativeFn>&,
    std::vector<NativeDescriptor>&);

struct LoadedExtension {
    std::string name;
    HMODULE handle;
    RegisterNativesFn registerNatives;
};

int loadScripts(std::unordered_map<int, std::string>& scripts);
int compileScripts(std::unordered_map<int, std::string> scripts, std::unordered_map<std::string, IVRApp>& apps);
void loadExtensions(std::unordered_map<int, NativeFn>& funcMap,
    std::unordered_map<std::string, Function>& funcList,
    std::unordered_map<std::string, LoadedExtension>& loadedExtensions);