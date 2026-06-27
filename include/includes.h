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

#include "nlohmann/json.hpp"

using json = nlohmann::json;
namespace fs = std::filesystem;

#define APPS "./apps" // path to DLLs