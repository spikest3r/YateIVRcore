#pragma once
#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <vector>
#include <map>
#include <ctime>
#include <filesystem>
#include <sapi.h>
#include <sphelper.h>
#include <winhttp.h>
#include <fstream>
#include <iomanip>
#include <unordered_map>
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <variant>
#include <cmath>
#include <functional>
#include <chrono>
#include <thread>
#include <unordered_set>

#include "nlohmann/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;