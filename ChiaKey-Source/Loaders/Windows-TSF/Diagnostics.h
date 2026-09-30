#pragma once

namespace ChiaKey::WindowsTsf {

// never pass key codes or text: any debug listener would see every keystroke
void Trace(const char* format, ...);

}  // namespace ChiaKey::WindowsTsf
