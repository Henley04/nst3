//-----------------------------------------------------------------------------
// nst3 — Mock VST3 test plugin (Gain)
// Factory entry point — registers the GainProcessor class with the VST3
// module factory.
//-----------------------------------------------------------------------------
#include "public.sdk/source/main/pluginfactory.h"

#include "version.h"
#include "cids.h"
#include "gainprocessor.h"

//------------------------------------------------------------------------
// VST Plug-in Entry — defines GetPluginFactory()
// NOTE: kVstAudioEffectClass is a #define macro, not a namespaced symbol,
//       so do not prefix it with Steinberg::Vst::.
//------------------------------------------------------------------------
BEGIN_FACTORY_DEF(stringOriginalName, stringOriginalVendorWeb, stringOriginalEmail)

    DEF_CLASS2(INLINE_UID_FROM_FUID(Steinberg::Vst::GainProcessorUID),
               PClassInfo::kManyInstances,
               kVstAudioEffectClass,
               stringOriginalName,
               0, // single-component effects cannot be distributed — classFlags = 0
               GainProcessorSubCategories,
               FULL_VERSION_STR,
               kVstVersionString,
               Steinberg::Vst::GainProcessor::createInstance)

END_FACTORY
