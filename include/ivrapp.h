#pragma once

#include "includes.h"
#include "coreapi.h"

struct AppAPI {
    OnCallStartFn OnCallStart;
    OnDtmfFn OnDtmf;
    OnHangUpFn OnHangUp;
};

struct IVRApp {
    HMODULE dll;
    AppAPI api;
};