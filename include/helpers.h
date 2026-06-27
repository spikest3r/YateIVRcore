#pragma once
#include "includes.h"

std::string EscapeColon(const std::string& path);
std::map<std::string, std::string> ParseParams(const std::string& raw);
bool SpeakToWav(const char* text, const char* output_path);
bool HttpGet(const char* host, const char* path, char* outBuffer, int bufferSize);