// MakiUI: the "Interface" switch shared by the Makimchi effects.
// Full shows every parameter; Minimal hides everything but the essentials.
// Hiding goes through the AEGP stream suites, which is how After Effects lets an effect
// change the visibility of its own parameters (see the SDK's Supervisor sample).
#pragma once

#include "AE_Effect.h"
#include "AE_EffectCB.h"
#include "AEGP_SuiteHandler.h"

enum
{
    MakiUI_FULL = 1,
    MakiUI_MINIMAL,
    MakiUI_NUM_CHOICES = MakiUI_MINIMAL
};

#define MAKIUI_NAME "Interface"
#define MAKIUI_CHOICES "Full|Minimal"
// Supervised so a change reaches the effect at once; never animated.
#define MAKIUI_FLAGS (PF_ParamFlag_SUPERVISE | PF_ParamFlag_CANNOT_TIME_VARY | PF_ParamFlag_CANNOT_INTERP)

inline AEGP_PluginID& MakiUI_PluginId()
{
    static AEGP_PluginID id = 0;
    return id;
}

inline bool MakiUI_InPremiere(const PF_InData* in_data)
{
    return in_data->appl_id == 'PrMr';
}

// Call from PF_Cmd_GLOBAL_SETUP. The AEGP id is needed to reach the effect's streams.
inline void MakiUI_Register(PF_InData* in_data, const char* name)
{
    if (MakiUI_InPremiere(in_data))
        return;
    try
    {
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        suites.UtilitySuite6()->AEGP_RegisterWithAEGP(NULL, name, &MakiUI_PluginId());
    }
    catch (...)
    {
    }
}

// hidden[i] says whether parameter i (index into params[]) should be hidden; index 0 is the layer.
// Failures are ignored on purpose: visibility is cosmetic and must never break the effect.
inline void MakiUI_Apply(PF_InData* in_data, const bool* hidden, int count)
{
    if (MakiUI_InPremiere(in_data))
        return;
    try
    {
        AEGP_SuiteHandler suites(in_data->pica_basicP);
        AEGP_EffectRefH effectH = NULL;
        if (suites.PFInterfaceSuite1()->AEGP_GetNewEffectForEffect(MakiUI_PluginId(), in_data->effect_ref, &effectH) ||
            !effectH)
            return;
        for (int i = 1; i < count; ++i)
        {
            AEGP_StreamRefH streamH = NULL;
            if (!suites.StreamSuite6()->AEGP_GetNewEffectStreamByIndex(MakiUI_PluginId(), effectH, i, &streamH) &&
                streamH)
            {
                suites.DynamicStreamSuite4()->AEGP_SetDynamicStreamFlag(
                    streamH, AEGP_DynStreamFlag_HIDDEN, FALSE, hidden[i] ? TRUE : FALSE);
                suites.StreamSuite6()->AEGP_DisposeStream(streamH);
            }
        }
        suites.EffectSuite4()->AEGP_DisposeEffect(effectH);
    }
    catch (...)
    {
    }
}
