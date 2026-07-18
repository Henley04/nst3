//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// ComponentHandler implementation
//-----------------------------------------------------------------------------
#include "component_handler.h"

#include "pluginterfaces/vst/ivsthostapplication.h" // for kLatencyChanged etc.

namespace nst3 {

ComponentHandler::ComponentHandler() = default;
ComponentHandler::~ComponentHandler() noexcept {
    instance_ = nullptr;
}

Steinberg::tresult PLUGIN_API ComponentHandler::beginEdit(Steinberg::Vst::ParamID id) {
    // beginEdit marks the start of a user-driven automation gesture. The host
    // does not need to do anything special here; we just acknowledge.
    (void)id;
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::performEdit(
    Steinberg::Vst::ParamID id, Steinberg::Vst::ParamValue valueNormalized) {
    // performEdit is called when the controller wants the host to record a
    // parameter automation point. We forward it to the PluginInstance so it
    // can update the controller value and add a point to the input parameter
    // change queue for the next process call (per VST3 spec).
    if (performEditSink_) {
        performEditSink_->onPerformEdit(id, valueNormalized);
    }
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::endEdit(Steinberg::Vst::ParamID id) {
    (void)id;
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::restartComponent(Steinberg::int32 flags) {
    lastRestartFlags_.store(flags, std::memory_order_release);
    if (restartCb_) {
        restartCb_(flags);
    }
    return Steinberg::kResultTrue;
}

// IComponentHandler2 — minimal stub implementations
Steinberg::tresult PLUGIN_API ComponentHandler::setDirty(Steinberg::TBool state) {
    (void)state;
    return Steinberg::kResultTrue;
}

Steinberg::tresult PLUGIN_API ComponentHandler::requestOpenEditor(Steinberg::FIDString name) {
    (void)name;
    // We don't support plugin editors in this version.
    return Steinberg::kResultFalse;
}

Steinberg::tresult PLUGIN_API ComponentHandler::startGroupEdit() {
    return Steinberg::kNotImplemented;
}

Steinberg::tresult PLUGIN_API ComponentHandler::finishGroupEdit() {
    return Steinberg::kNotImplemented;
}

Steinberg::Vst::IContextMenu* PLUGIN_API
ComponentHandler::createContextMenu(Steinberg::IPlugView* plugView,
                                    const Steinberg::Vst::ParamID* paramID) {
    (void)plugView; (void)paramID;
    return nullptr; // No context menu support
}

} // namespace nst3
