//-----------------------------------------------------------------------------
// nst3 — VST3 Host for Node.js
// HostApplication implementation
//-----------------------------------------------------------------------------
#include "host_application.h"

#include "string_convert.h"

namespace nst3 {

NstHostApplication::NstHostApplication() = default;

NstHostApplication::~NstHostApplication() noexcept {
    // HostApplication base class dtor is responsible for releasing the
    // PlugInterfaceSupport. We just clear our raw pointer (the handler is
    // owned elsewhere).
    handler_ = nullptr;
}

Steinberg::tresult PLUGIN_API NstHostApplication::getName(Steinberg::Vst::String128 name) {
    if (!name) return Steinberg::kInvalidArgument;
    static const std::string kHostName = "Node.js VST3 Host";
    utf8ToString128(kHostName, name);
    return Steinberg::kResultTrue;
}

} // namespace nst3
