#include "FrontendBehavior.h"
#include <mmsystem.h>

namespace ChiaKey::WindowsTsf {

void PlayTypingErrorSound(const FrontendSettings& settings) {
    if (!settings.playSoundOnTypingError) return;
    const auto& filename = settings.soundFilename;
    if (!filename.empty() && filename != "Default") {
        int length = MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.data(),
                                        static_cast<int>(filename.size()), nullptr, 0);
        if (length > 0) {
            std::wstring path(static_cast<size_t>(length), L'\0');
            MultiByteToWideChar(CP_UTF8, MB_ERR_INVALID_CHARS, filename.data(),
                               static_cast<int>(filename.size()), path.data(), length);
            if (PlaySoundW(path.c_str(), nullptr, SND_FILENAME | SND_ASYNC | SND_NODEFAULT)) return;
        }
    }
    MessageBeep(MB_OK);
}

} // namespace ChiaKey::WindowsTsf
