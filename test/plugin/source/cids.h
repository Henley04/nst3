#pragma once

#include "pluginterfaces/base/funknown.h"

namespace Steinberg {
namespace Vst {

// {A1B2C3D4-E5F6-7890-ABCD-EF1234567890}
static const FUID GainProcessorUID(0xA1B2C3D4, 0xE5F67890, 0xABCDEF12, 0x34567890);
static const FUID GainControllerUID(0xB2C3D4E5, 0xF6789012, 0xBCDEF123, 0x45678901);

#define GainProcessorSubCategories "Fx"

}} // namespaces
