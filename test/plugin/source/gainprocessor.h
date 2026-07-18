//-----------------------------------------------------------------------------
// nst3 — Mock VST3 test plugin (Gain)
// A simple stereo gain plugin built on SingleComponentEffect.
//-----------------------------------------------------------------------------
#pragma once

// must always come first
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"

namespace Steinberg {
namespace Vst {

// Parameter IDs used by the GainProcessor plugin.
enum GainParamIds
{
    kGainId = 0  ///< Gain parameter (0..1, default 1.0, automatable)
};

//------------------------------------------------------------------------
// GainProcessor — combined component + controller (SingleComponentEffect)
//------------------------------------------------------------------------
class GainProcessor : public SingleComponentEffect
{
public:
    GainProcessor();

    static FUnknown* createInstance(void* /*context*/) { return (IAudioProcessor*)new GainProcessor; }

    //---from IComponent-----------------------
    tresult PLUGIN_API initialize(FUnknown* context) SMTG_OVERRIDE;
    tresult PLUGIN_API terminate() SMTG_OVERRIDE;
    tresult PLUGIN_API setActive(TBool state) SMTG_OVERRIDE;
    tresult PLUGIN_API setProcessing(TBool state) SMTG_OVERRIDE;
    tresult PLUGIN_API setupProcessing(ProcessSetup& newSetup) SMTG_OVERRIDE;
    tresult PLUGIN_API process(ProcessData& data) SMTG_OVERRIDE;
    tresult PLUGIN_API setState(IBStream* state) SMTG_OVERRIDE;
    tresult PLUGIN_API getState(IBStream* state) SMTG_OVERRIDE;
    tresult PLUGIN_API canProcessSampleSize(int32 symbolicSampleSize) SMTG_OVERRIDE;

    OBJ_METHODS(GainProcessor, SingleComponentEffect)
    REFCOUNT_METHODS(SingleComponentEffect)

private:
    // Apply gain to a buffer of the given sample type
    template <typename SampleType>
    void applyGain(SampleType** in, SampleType** out, int32 numChannels, int32 sampleFrames,
                   SampleType gain);

    float currentGain_;  // last applied gain (mirrors the parameter value)
};

//------------------------------------------------------------------------
}} // namespaces
