#include "loader.h"

#define APPS "./apps"
#define EXTENSIONS "./extensions"

int loadScripts(std::unordered_map<int, std::string>& scripts) {
    try {
        if (fs::exists(APPS) && fs::is_directory(APPS)) {
            for (const auto& entry : fs::directory_iterator(APPS)) {
                if (!fs::is_regular_file(entry.status())) continue;

                auto ext = entry.path().extension();
                if (ext != ".lmn") continue;

                std::string filename = entry.path().filename().string();
                std::string stem = entry.path().stem().string();

                // stem must be all digits to be a valid extension number
                if (stem.empty() || !std::all_of(stem.begin(), stem.end(), ::isdigit)) {
                    std::cerr << "Invalid extension name for " << filename << ", skipping" << std::endl;
                    continue;
                }

                int extension = std::stoi(stem);

                if (scripts.contains(extension)) {
                    std::cerr << "Duplicate extension " << extension << " for file " << filename << std::endl;
                    continue;
                }

                scripts[extension] = entry.path().string();

                std::cout << "Found script " << filename << " for extension " << extension << std::endl;
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
    return 0;
}

int compileScripts(std::unordered_map<int, std::string> scripts, std::unordered_map<std::string, IVRApp>& apps) {
    for (auto& it : scripts) {
        int ext = it.first;

        std::cout << "Compiling " << it.second << "...\n";

        CompilerData compilerData;
        std::unordered_map<std::string, uint32_t> rtMap;
        int status = compile(it.second, &compilerData, rtMap);

        if (status != 0) {
            std::cerr << "Compilation failed!\n";
            continue;
        }

        auto it1 = rtMap.find("onCall");
        if (it1 == rtMap.end()) {
            std::cerr << "onCall routine is not defined in script\n";
            continue;
        }
        uint32_t onCallPC = it1->second;

        auto it2 = rtMap.find("onDtmf");
        if (it2 == rtMap.end()) {
            std::cerr << "onDtmf routine is not defined in script\n";
            continue;
        }
        uint32_t onDtmfPC = it2->second;

        auto it3 = rtMap.find("onHangup");
        if (it3 == rtMap.end()) {
            std::cerr << "onHangup routine is not defined in script\n";
            continue;
        }
        uint32_t onHangupPC = it3->second;

        IVRApp app;
        app.extension = ext;

        // fill out VMProgramData struct
        app.progData.bytecode = std::move(compilerData.bytecode);
        app.progData.constPool = std::move(compilerData.constPool);
        app.progData.stringPool = std::move(compilerData.stringPool);
        app.progData.variableCount = compilerData.variableCount;

        // provide pointers to all routines
        app.onCallPC = onCallPC;
        app.onDtmfPC = onDtmfPC;
        app.onHangupPC = onHangupPC;

        apps.emplace(std::to_string(ext), std::move(app));

        std::cout << "Compiled successfully!\n";
    }

    return 0;
}

void loadExtensions(std::unordered_map<int, NativeFn>& funcMap,
    std::unordered_map<std::string, Function>& funcList,
    std::unordered_map<std::string, LoadedExtension>& loadedExtensions) {
    if (!fs::exists(EXTENSIONS) || !fs::is_directory(EXTENSIONS)) {
        std::cerr << "Extensions path does not exist or is not a directory: " << EXTENSIONS << std::endl;
        return;
    }

    for (const auto& entry : fs::directory_iterator(EXTENSIONS)) {
        if (!entry.is_regular_file()) continue;
        if (entry.path().extension() != ".dll") continue;

        std::string path = entry.path().string();
        std::string name = entry.path().stem().string();

        HMODULE handle = LoadLibraryA(path.c_str());
        if (!handle) {
            std::cerr << "Failed to load extension: " << path
                << " (error " << GetLastError() << ")" << std::endl;
            continue;
        }

        auto registerFn = reinterpret_cast<RegisterNativesFn>(
            GetProcAddress(handle, "RegisterNatives"));

        if (!registerFn) {
            std::cerr << "Extension " << name
                << " has no RegisterNatives export, skipping" << std::endl;
            FreeLibrary(handle);
            continue;
        }

        std::vector<NativeDescriptor> descriptors;
        registerFn(funcMap, descriptors);

        for (const auto& d : descriptors) {
            if (funcList.count(d.name)) {
                std::cerr << "Extension " << name << " redefines existing function '"
                    << d.name << "', overwriting" << std::endl;
            }
            funcList[d.name] = Function{ d.opcode, d.argCount };
        }

        loadedExtensions[name] = LoadedExtension{ name, handle, registerFn };

        std::cout << "Loaded extension: " << name
            << " (" << descriptors.size() << " functions)" << std::endl;
    }
}