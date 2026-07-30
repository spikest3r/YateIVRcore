#pragma once

#include "includes.h"
#include "vm-inc/types.h"

struct IVRApp {
    VMProgramData progData;
    uint32_t onCallPC;
    uint32_t onDtmfPC;
    uint32_t onHangupPC;
    int extension;
};