//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// ComponentHandler — implements IComponentHandler{,2,3} so the plugin's edit
// controller can talk back to the host (begin/perform/endEdit, restartComponent,
// requestOpenEditor, etc.). Restarts are forwarded to a TSFN for JS delivery.
//-----------------------------------------------------------------------------
#pragma once

#include <atomic>
#include <functional>
#include <mutex>
#include <unordered_set>

#include "pluginterfaces/base/funknownimpl.h"
#include "pluginterfaces/vst/ivsteditcontroller.h"
#include "pluginterfaces/vst/ivstcontextmenu.h"

namespace nst3 {

// Forward declarations
class PluginInstance;

// Interface used by ComponentHandler to forward performEdit() calls back to
// the owning PluginInstance so that the change is added to the input
// IParameterChanges queue for the next process() call.
struct IPerformEditSink {
    virtual ~IPerformEditSink() = default;
    // Apply a controller-driven parameter change: set the controller value and
    // queue a point at sample offset 0 for the next process call.
    virtual void onPerformEdit(Steinberg::Vst::ParamID id,
                              Steinberg::Vst::ParamValue valueNormalized) = 0;
};

// Callback signature for restart notifications delivered to JS.
using RestartCallback = std::function<void(int32_t flags)>;

// Callback invoked BEFORE the JS restart event is emitted, so the host can
// re-query the affected SDK state (latency, bus info, etc.) for the given
// flags. See PluginInstance::ApplyRestartFlags. Must be safe to call from any
// thread (typically the controller thread, which is the JS thread here).
using ApplyRestartCallback = std::function<void(int32_t flags)>;

// A plugin→host event delivered to JS via the host-event TSFN. The payload
// type is discriminated by `isBool` / `hasPayload`:
//   - 'dirty'         → hasPayload=true, isBool=true,  value=0/1
//   - 'beginGesture'  → hasPayload=true, isBool=false, value=paramId
//   - 'endGesture'    → hasPayload=true, isBool=false, value=paramId
//   - 'startGroup'    → hasPayload=false
//   - 'finishGroup'   → hasPayload=false
// `name` points at a string literal with static storage duration.
struct HostEvent {
    const char* name;
    double value;
    bool isBool;
    bool hasPayload;
};
using HostEventCallback = std::function<void(const HostEvent&)>;

// ComponentHandler implements IComponentHandler, IComponentHandler2, and
// IComponentHandler3. It is held by PluginInstance (which retains a strong
// reference). The handler dispatches restart notifications asynchronously
// to a callback set by the PluginInstance (which wraps a TSFN).
class ComponentHandler final
    : public Steinberg::U::Implements<
          Steinberg::U::Directly<Steinberg::Vst::IComponentHandler,
                                 Steinberg::Vst::IComponentHandler2,
                                 Steinberg::Vst::IComponentHandler3>> {
public:
    ComponentHandler();
    ~ComponentHandler() noexcept override;

    // Set the sink that receives performEdit() calls. The sink is owned
    // externally (typically the PluginInstance). Set to nullptr to disable.
    void setPerformEditSink(IPerformEditSink* sink) { performEditSink_ = sink; }

    // Set the PluginInstance that owns this handler. Used to forward
    // beginEdit/performEdit/endEdit to the parameter change queue.
    void setPluginInstance(PluginInstance* instance) { instance_ = instance; }

    // Set the callback invoked when restartComponent is called. The callback
    // must be safe to call from any thread (typically a TSFN-blocking call).
    void setRestartCallback(RestartCallback cb) { restartCb_ = std::move(cb); }

    // Set the callback invoked BEFORE the restart JS event is emitted, so the
    // host can refresh cached SDK state (latency, bus info, etc.) for the
    // restart flags. The callback is invoked synchronously from
    // restartComponent() and must be safe to call from any thread. If unset,
    // restartComponent() only emits the JS event (prior behavior).
    void setApplyRestartCallback(ApplyRestartCallback cb) { applyRestartCb_ = std::move(cb); }

    // Set the callback invoked when host-event methods are called by the
    // plugin (setDirty, beginEdit, endEdit, startGroupEdit, finishGroupEdit).
    // The callback must be safe to call from any thread (typically it
    // dispatches via a TSFN owned by PluginInstance).
    void setHostEventCallback(HostEventCallback cb) { hostEventCb_ = std::move(cb); }

    // --- IComponentHandler ---
    Steinberg::tresult PLUGIN_API beginEdit(Steinberg::Vst::ParamID id) override;
    Steinberg::tresult PLUGIN_API performEdit(Steinberg::Vst::ParamID id,
                                              Steinberg::Vst::ParamValue valueNormalized) override;
    Steinberg::tresult PLUGIN_API endEdit(Steinberg::Vst::ParamID id) override;
    Steinberg::tresult PLUGIN_API restartComponent(Steinberg::int32 flags) override;

    // --- IComponentHandler2 ---
    Steinberg::tresult PLUGIN_API setDirty(Steinberg::TBool state) override;
    Steinberg::tresult PLUGIN_API requestOpenEditor(Steinberg::FIDString name) override;
    Steinberg::tresult PLUGIN_API startGroupEdit() override;
    Steinberg::tresult PLUGIN_API finishGroupEdit() override;

    // --- IComponentHandler3 ---
    Steinberg::Vst::IContextMenu* PLUGIN_API
    createContextMenu(Steinberg::IPlugView* plugView, const Steinberg::Vst::ParamID* paramID) override;

private:
    PluginInstance* instance_ = nullptr;
    IPerformEditSink* performEditSink_ = nullptr;
    RestartCallback restartCb_;
    ApplyRestartCallback applyRestartCb_;
    std::atomic<int32_t> lastRestartFlags_{0};

    // Active gesture tracking: parameter IDs with an open beginEdit gesture.
    // Used by future automation recording to attribute performEdit points to
    // their enclosing gesture. Guarded by gesturesMutex_ because begin/endEdit
    // can be invoked from the controller thread OR from a thread spawned by
    // the plugin itself (e.g. UI thread, MIDI clock thread) — the VST3 spec
    // does not constrain the calling thread for these methods.
    std::unordered_set<Steinberg::Vst::ParamID> activeGestures_;
    mutable std::mutex gesturesMutex_;

    // Plugin→host event callback (dirty, beginGesture, endGesture, startGroup,
    // finishGroup). Invoked from any thread; the callback is responsible for
    // thread-safe dispatch (typically via a TSFN).
    HostEventCallback hostEventCb_;
};

} // namespace nst3
