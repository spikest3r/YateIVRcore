#include "helpers.h"

std::string EscapeColon(const std::string& path) {
    std::string result = path;
    size_t pos = 0;
    while ((pos = result.find(':', pos)) != std::string::npos) {
        result.replace(pos, 1, "%z");
        pos += 2; // skip past the replacement
    }
    return result;
}

std::map<std::string, std::string> ParseParams(const std::string& raw) {
    std::map<std::string, std::string> params;
    size_t start = 0;
    while (start <= raw.size()) {
        size_t pos = raw.find(':', start);
        std::string p = (pos == std::string::npos) ? raw.substr(start) : raw.substr(start, pos - start);

        size_t eq = p.find('=');
        if (eq != std::string::npos) {
            std::string k = p.substr(0, eq);
            std::string v = p.substr(eq + 1);
            params[k] = v;
        }

        if (pos == std::string::npos) break;
        start = pos + 1;
    }
    return params;
}