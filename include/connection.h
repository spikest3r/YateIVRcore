#pragma once
#include "includes.h"

bool Connect(SOCKET& g_sock, const std::string& host, int port);
void Disconnect(SOCKET& g_sock);
bool Send(SOCKET& g_sock, const std::string& msg);
bool ReloadRegexRoute();