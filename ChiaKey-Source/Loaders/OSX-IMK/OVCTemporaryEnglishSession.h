#pragma once
#include <string>

// Shared by all IMK controllers. Text clients may disappear or be replaced
// within one app, so their lifetime must not determine the language mode.
class OVCTemporaryEnglishSession {
 public:
  OVCTemporaryEnglishSession() : _application(0), _enabled(false) {}

  bool enabled() const { return _enabled; }
  void setEnabled(bool enabled) { _enabled = enabled; }

  void activateApplication(int application, const char *clientBundle = nullptr,
                           bool resetOnApplicationSwitch = true) {
    if (_application != application) {
      if (resetOnApplicationSwitch) _enabled = false;
      _application = application;
      _clientBundle.clear();
    }
    // Spotlight owns the text client while the workspace still reports the
    // underlying app as frontmost. Track both identities, not client pointers.
    if (clientBundle && *clientBundle) {
      if (!_clientBundle.empty() && _clientBundle != clientBundle) {
        if (resetOnApplicationSwitch) _enabled = false;
      }
      _clientBundle = clientBundle;
    }
  }

  void deactivateApplication(int application,
                             bool resetOnApplicationSwitch = true) {
    // A late notification from the previous app must not reset the new app.
    if (_application == application) {
      if (resetOnApplicationSwitch) _enabled = false;
      _application = 0;
      _clientBundle.clear();
    }
  }

  // Excel emits selection notifications when changing text clients even if
  // the selected source is unchanged. A notification alone is not a switch.
  bool updateInputSource(const char *source) {
    if (!source || !*source || _inputSource == source) return false;
    _inputSource = source;
    _enabled = false;
    return true;
  }

 private:
  int _application;
  bool _enabled;
  std::string _inputSource;
  std::string _clientBundle;
};
