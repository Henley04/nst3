//-----------------------------------------------------------------------------
// nst3 — Mock VST3 test plugin (Gain)
// A simple stereo gain plugin built on SingleComponentEffect.
//
// This fixture also implements IUnitInfo (one top-level "Root" unit + one
// "Presets" program list with two programs: "Init" and "Bright") and
// INoteExpressionController (one volume expression) so the host can exercise
// those code paths without needing a third-party plugin.
//-----------------------------------------------------------------------------
#pragma once

// must always come first
#include "public.sdk/source/vst/vstsinglecomponenteffect.h"

#include "pluginterfaces/vst/ivstunits.h"
#include "pluginterfaces/vst/ivstnoteexpression.h"

namespace Steinberg {
namespace Vst {

// Parameter IDs used by the GainProcessor plugin.
enum GainParamIds
{
    kGainId    = 0,  ///< Gain parameter (0..1, default 1.0, automatable)
    kProgramId = 1   ///< Program-change parameter (2 steps: 0=Init, 1=Bright)
                     ///< Tagged with kIsProgramChange so the host can switch
                     ///< presets via selectProgram(unitId, programIndex).
};

//------------------------------------------------------------------------
// GainProcessor — combined component + controller (SingleComponentEffect)
//   also implements IUnitInfo and INoteExpressionController for testing.
//------------------------------------------------------------------------
class GainProcessor : public SingleComponentEffect,
                      public IUnitInfo,
                      public INoteExpressionController
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

    //---from IUnitInfo-----------------------
    int32 PLUGIN_API getUnitCount() SMTG_OVERRIDE;
    tresult PLUGIN_API getUnitInfo(int32 unitIndex, UnitInfo& info) SMTG_OVERRIDE;
    int32 PLUGIN_API getProgramListCount() SMTG_OVERRIDE;
    tresult PLUGIN_API getProgramListInfo(int32 listIndex, ProgramListInfo& info) SMTG_OVERRIDE;
    tresult PLUGIN_API getProgramName(ProgramListID listId, int32 programIndex,
                                      String128 name) SMTG_OVERRIDE;
    tresult PLUGIN_API getProgramInfo(ProgramListID listId, int32 programIndex,
                                      CString attributeId, String128 attributeValue) SMTG_OVERRIDE;
    tresult PLUGIN_API hasProgramPitchNames(ProgramListID listId, int32 programIndex) SMTG_OVERRIDE;
    tresult PLUGIN_API getProgramPitchName(ProgramListID listId, int32 programIndex,
                                           int16 midiPitch, String128 name) SMTG_OVERRIDE;
    tresult PLUGIN_API selectUnit(UnitID unitId) SMTG_OVERRIDE;
    UnitID PLUGIN_API getSelectedUnit() SMTG_OVERRIDE;
    tresult PLUGIN_API getUnitByBus(MediaType mediaType, BusDirection dir, int32 busIndex,
                                    int32 channel, UnitID& unitId) SMTG_OVERRIDE;
    tresult PLUGIN_API setUnitProgramData(int32 listOrUnitId, int32 programIndex,
                                          IBStream* data) SMTG_OVERRIDE;

    //---from INoteExpressionController-------
    int32 PLUGIN_API getNoteExpressionCount(int32 busIndex, int16 channel) SMTG_OVERRIDE;
    tresult PLUGIN_API getNoteExpressionInfo(int32 busIndex, int16 channel,
                                             int32 noteExpressionIndex,
                                             NoteExpressionTypeInfo& info) SMTG_OVERRIDE;
    tresult PLUGIN_API getNoteExpressionStringByValue(int32 busIndex, int16 channel,
                                                      NoteExpressionTypeID id,
                                                      NoteExpressionValue valueNormalized,
                                                      String128 string) SMTG_OVERRIDE;
    tresult PLUGIN_API getNoteExpressionValueByString(int32 busIndex, int16 channel,
                                                      NoteExpressionTypeID id,
                                                      const TChar* string,
                                                      NoteExpressionValue& valueNormalized) SMTG_OVERRIDE;

    //---Interface declaration----------------
    // Multiple-interface support: replace OBJ_METHODS with DEFINE_INTERFACES
    // so queryInterface advertises IUnitInfo and INoteExpressionController
    // in addition to the SingleComponentEffect base interfaces.
    DEFINE_INTERFACES
        DEF_INTERFACE(IUnitInfo)
        DEF_INTERFACE(INoteExpressionController)
    END_DEFINE_INTERFACES(SingleComponentEffect)
    REFCOUNT_METHODS(SingleComponentEffect)

private:
    // Apply gain to a buffer of the given sample type
    template <typename SampleType>
    void applyGain(SampleType** in, SampleType** out, int32 numChannels, int32 sampleFrames,
                   SampleType gain);

    // Resolve the current gain from the program-change parameter (if set)
    // and the gain parameter. When the program parameter is at step 1
    // ("Bright"), the effective gain is 0.7; otherwise the gain parameter
    // value is used as-is.
    float resolveGain() const;

    float currentGain_;      // last applied gain (mirrors the parameter value)
    UnitID selectedUnit_;    // currently selected unit (always 0 for this fixture)
};

//------------------------------------------------------------------------
}} // namespaces
