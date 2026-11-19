// Bloom Forge — multi-scale glow effect for After Effects (SmartFX, 8/16/32 bpc).

#pragma once

#ifndef BLOOMFORGE_H
    #define BLOOMFORGE_H

    #define PF_DEEP_COLOR_AWARE 1

    #include "AEConfig.h"

    #ifdef AE_OS_WIN
        #ifndef NOMINMAX
            #define NOMINMAX
        #endif
        #include <Windows.h>
    #endif

    #include "entry.h"
    #include "AE_Effect.h"
    #include "AE_EffectCB.h"
    #include "AE_Macros.h"
    #include "Param_Utils.h"
    #include "AE_EffectCBSuites.h"
    #include "AE_GeneralPlug.h"
    #include "AEGP_SuiteHandler.h"
    #include "AEFX_SuiteHelper.h"
    #include "Smart_Utils.h"

    #define BF_NAME "Makiglow"
    #define BF_MATCH_NAME "MG BloomForge"
    #define BF_CATEGORY "Makimchi"
    #define BF_SUPPORT_URL "https://example.com"

    #define MAJOR_VERSION 1
    #define MINOR_VERSION 0
    #define BUG_VERSION 0
    #define STAGE_VERSION PF_Stage_DEVELOP
    #define BUILD_VERSION 1

enum
{
    BF_INPUT = 0,
    BF_THRESHOLD,
    BF_SOFTNESS,
    BF_RADIUS,
    BF_EXPOSURE,
    BF_FALLOFF,
    BF_ASPECT,
    BF_TINT_COLOR,
    BF_TINT_AMOUNT,
    BF_COMPOSITE,
    BF_LINEAR,
    BF_NUM_PARAMS
};

// Disk IDs must never change once the effect has been saved in a project.
enum
{
    THRESHOLD_DISK_ID = 1,
    SOFTNESS_DISK_ID,
    RADIUS_DISK_ID,
    EXPOSURE_DISK_ID,
    FALLOFF_DISK_ID,
    ASPECT_DISK_ID,
    TINT_COLOR_DISK_ID,
    TINT_AMOUNT_DISK_ID,
    COMPOSITE_DISK_ID,
    LINEAR_DISK_ID
};

enum
{
    Composite_ADD = 1,
    Composite_SCREEN,
    Composite_GLOW_ONLY,
    Composite_NUM
};

// Passed from PreRender to SmartRender: where the input and output buffers sit in layer space.
struct BloomRects
{
    PF_LRect in_rect;
    PF_LRect out_rect;
};

extern "C"
{
    DllExport PF_Err EffectMain(
        PF_Cmd cmd, PF_InData* in_data, PF_OutData* out_data, PF_ParamDef* params[], PF_LayerDef* output, void* extra);
}

#endif // BLOOMFORGE_H
