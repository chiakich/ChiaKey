#include "../OVCTemporaryEnglishSession.h"
#include <cassert>
#include <cstdio>

int main() {
  OVCTemporaryEnglishSession session;
  const char *chiaKey = "com.chiakey.inputmethod.ChiaKeyDev.Hant";
  session.updateInputSource(chiaKey);
  session.activateApplication(100);
  assert(!session.enabled());
  session.setEnabled(true);

  // Excel advancing cells / Spotlight replacing its text client must retain
  // English, including multiple new controllers in the same application.
  session.activateApplication(100);
  session.activateApplication(100);
  assert(session.enabled());
  // Recorded Excel sequence: activate the next client, then receive two
  // selected-source notifications with the very same input-source ID.
  assert(!session.updateInputSource(chiaKey));
  assert(session.enabled());
  assert(!session.updateInputSource(chiaKey));
  assert(session.enabled());
  assert(!session.updateInputSource(nullptr));
  assert(!session.updateInputSource(""));
  assert(session.enabled());
  session.setEnabled(!session.enabled());
  assert(!session.enabled());

  session.setEnabled(true);
  session.activateApplication(200);
  assert(!session.enabled());
  session.setEnabled(true);
  session.deactivateApplication(100);  // outgoing app's late notification
  assert(session.enabled());
  session.activateApplication(100);
  assert(!session.enabled());  // returning does not restore English

  session.setEnabled(true);
  session.deactivateApplication(100);
  // An intervening app without a text client never calls activateApplication.
  session.activateApplication(100);
  assert(!session.enabled());

  session.setEnabled(true);
  assert(session.updateInputSource("com.apple.keylayout.ABC"));
  assert(!session.enabled());
  assert(session.updateInputSource(chiaKey));  // switch back to ChiaKey
  session.activateApplication(100);
  assert(!session.enabled());

  session.setEnabled(true);
  // A delayed duplicate after activation must not undo a fresh Shift toggle.
  assert(!session.updateInputSource(chiaKey));
  assert(session.enabled());

  // Recorded overlay sequence: the frontmost PID remains Excel throughout.
  session.activateApplication(100, "com.microsoft.Excel");
  session.setEnabled(true);
  session.activateApplication(100, "com.microsoft.Excel");
  assert(session.enabled());
  session.activateApplication(100, "com.apple.Spotlight");
  assert(!session.enabled());
  session.setEnabled(true);
  session.activateApplication(100, "com.apple.Spotlight");
  session.updateInputSource(chiaKey);
  assert(session.enabled());  // Spotlight's own client replacement
  session.activateApplication(100, nullptr);
  assert(session.enabled());  // unavailable identity is not an app switch
  session.activateApplication(100, "com.microsoft.Excel");
  assert(!session.enabled());  // closing Spotlight also starts in Chinese

  // Opting out keeps Shift English across apps and overlay clients, even
  // when an intervening app has no input controller.
  session.setEnabled(true);
  session.deactivateApplication(100, false);
  session.activateApplication(200, "com.apple.TextEdit", false);
  assert(session.enabled());
  session.deactivateApplication(100, false);  // late outgoing notification
  assert(session.enabled());
  session.activateApplication(200, "com.apple.Spotlight", false);
  assert(session.enabled());
  session.activateApplication(200, "com.apple.TextEdit", false);
  assert(session.enabled());
  session.deactivateApplication(200, false);
  session.activateApplication(200, "com.apple.TextEdit", false);
  assert(session.enabled());

  // Actual input-source changes still clear the temporary mode.
  assert(session.updateInputSource("com.apple.keylayout.ABC"));
  assert(!session.enabled());
  session.updateInputSource(chiaKey);
  session.setEnabled(true);
  session.activateApplication(300, "com.apple.Terminal", false);
  assert(session.enabled());
  // Turning reset back on takes effect on the next app switch.
  session.activateApplication(200, "com.apple.TextEdit");
  assert(!session.enabled());
  session.setEnabled(true);
  session.deactivateApplication(200);
  assert(!session.enabled());

  std::puts("Temporary English session tests passed.");
}
