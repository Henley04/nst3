//-----------------------------------------------------------------------------
// nst3 — Mock VST3 test plugin (Gain)
// Implementation of the GainProcessor SingleComponentEffect.
//-----------------------------------------------------------------------------
#include "gainprocessor.h"
#include "cids.h"
#include "version.h"

#include "public.sdk/source/vst/vstaudioprocessoralgo.h"

#include "base/source/fstreamer.h"

#include "pluginterfaces/base/ibstream.h"
#include "pluginterfaces/base/ustring.h"  // for USTRING
#include "pluginterfaces/vst/ivstparameterchanges.h"

#include <cstring>

namespace Steinberg {
namespace Vst {

//------------------------------------------------------------------------
GainProcessor::GainProcessor()
: currentGain_(1.f)
{
}

//------------------------------------------------------------------------
tresult PLUGIN_API GainProcessor::initialize(FUnknown* context)
{
    tresult result = SingleComponentEffect::initialize(context);
    if (result != kResultOk)
        return result;

    //---create Audio In/Out busses (stereo)----------------
    addAudioInput(STR16("Stereo In"), SpeakerArr::kStereo);
    addAudioOutput(STR16("Stereo Out"), SpeakerArr::kStereo);

    //---create the Gain parameter--------------------------
    // Range 0..1, default 1.0, automatable.
    parameters.addParameter(USTRING("Gain"), nullptr, 0, 1.f,
                            ParameterInfo::kCanAutomate, kGainId);

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GainProcessor::terminate()
{
    return SingleComponentEffect::terminate();
}

//------------------------------------------------------------------------
tresult PLUGIN_API GainProcessor::setActive(TBool state)
{
    if (state)
        currentGain_ = 1.f;
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GainProcessor::setProcessing(TBool /*state*/)
{
    // The base SingleComponentEffect returns kNotImplemented; we accept the
    // transition so the host can enter/leave the processing state cleanly.
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GainProcessor::setupProcessing(ProcessSetup& newSetup)
{
    currentGain_ = 1.f;
    return SingleComponentEffect::setupProcessing(newSetup);
}

//------------------------------------------------------------------------
tresult PLUGIN_API GainProcessor::canProcessSampleSize(int32 symbolicSampleSize)
{
    if (symbolicSampleSize == kSample32)
        return kResultTrue;
    return kResultFalse;
}

//------------------------------------------------------------------------
template <typename SampleType>
void GainProcessor::applyGain(SampleType** in, SampleType** out, int32 numChannels,
                              int32 sampleFrames, SampleType gain)
{
    for (int32 i = 0; i < numChannels; i++)
    {
        SampleType* ptrIn = in[i];
        SampleType* ptrOut = out[i];
        if (!ptrIn || !ptrOut)
            continue;
        for (int32 s = 0; s < sampleFrames; s++)
        {
            ptrOut[s] = ptrIn[s] * gain;
        }
    }
}

//------------------------------------------------------------------------
tresult PLUGIN_API GainProcessor::process(ProcessData& data)
{
    //---1) Read parameter changes from the host------------
    if (IParameterChanges* paramChanges = data.inputParameterChanges)
    {
        int32 numParamsChanged = paramChanges->getParameterCount();
        for (int32 i = 0; i < numParamsChanged; i++)
        {
            if (IParamValueQueue* paramQueue = paramChanges->getParameterData(i))
            {
                if (paramQueue->getParameterId() == kGainId)
                {
                    int32 sampleOffset = 0;
                    ParamValue value = 0;
                    int32 numPoints = paramQueue->getPointCount();
                    if (numPoints > 0 &&
                        paramQueue->getPoint(numPoints - 1, sampleOffset, value) == kResultTrue)
                    {
                        currentGain_ = (float)value;
                    }
                }
            }
        }
    }

    //---2) Process audio----------------------------------
    if (data.numInputs == 0 || data.numOutputs == 0)
        return kResultOk;

    // (simplification) we expect one input and one output bus, both stereo.
    int32 numChannels = data.inputs[0].numChannels;
    if (numChannels > data.outputs[0].numChannels)
        numChannels = data.outputs[0].numChannels;

    uint32 sampleFramesSize = getSampleFramesSizeInBytes(processSetup, data.numSamples);
    void** in = getChannelBuffersPointer(processSetup, data.inputs[0]);
    void** out = getChannelBuffersPointer(processSetup, data.outputs[0]);

    //---check if input is silent---------------------------
    if (data.inputs[0].silenceFlags == getChannelMask(data.inputs[0].numChannels))
    {
        data.outputs[0].silenceFlags = data.inputs[0].silenceFlags;
        for (int32 i = 0; i < numChannels; i++)
        {
            if (in[i] != out[i])
                memset(out[i], 0, sampleFramesSize);
        }
        return kResultOk;
    }

    data.outputs[0].silenceFlags = 0;

    //---apply gain factor---------------------------------
    if (processSetup.symbolicSampleSize == kSample32)
    {
        applyGain<Sample32>((Sample32**)in, (Sample32**)out, numChannels, data.numSamples,
                            (Sample32)currentGain_);
    }
    else if (processSetup.symbolicSampleSize == kSample64)
    {
        applyGain<Sample64>((Sample64**)in, (Sample64**)out, numChannels, data.numSamples,
                            (Sample64)currentGain_);
    }

    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GainProcessor::setState(IBStream* state)
{
    if (!state)
        return kInvalidArgument;

    IBStreamer streamer(state, kLittleEndian);
    float savedGain = 0.f;
    if (streamer.readFloat(savedGain) == false)
        return kResultFalse;

    currentGain_ = savedGain;
    setParamNormalized(kGainId, savedGain);
    return kResultOk;
}

//------------------------------------------------------------------------
tresult PLUGIN_API GainProcessor::getState(IBStream* state)
{
    if (!state)
        return kInvalidArgument;

    IBStreamer streamer(state, kLittleEndian);
    if (streamer.writeFloat(currentGain_) == false)
        return kResultFalse;
    return kResultOk;
}

//------------------------------------------------------------------------
}} // namespaces
