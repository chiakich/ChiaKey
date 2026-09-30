//
// UserPhraseStore.h
//
// The user's own phrases in SmartMandarinUserData.db, for a phrase editor.
// Ported from the mac PhraseEditor's PEUserPhraseStore, and speaking the same
// protocol with the input method (ChiaKeyUserPhraseCoordination.h): an editing
// lock that holds back the engine's own writes, and a dirty flag it reloads on.
//

#ifndef ChiaKeyCore_UserPhraseStore_h
#define ChiaKeyCore_UserPhraseStore_h

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

namespace ChiaKey {

struct UserPhrase {
  // stable for an editing session: neither side VACUUMs while one is open
  long long rowid = 0;
  std::string phrase;
  // composed Bopomofo, one syllable per character, comma separated
  std::string reading;
};

enum class UserPhraseOrder { Insertion, Phrase, Reading };

class UserPhraseStore {
 public:
  // writablePath is the Runtime's; the lexicon is read for default readings
  static std::unique_ptr<UserPhraseStore> Open(const std::string& writablePath,
                                               const std::string& lexiconDatabasePath,
                                               std::string* errorMessage = nullptr);

  ~UserPhraseStore();

  UserPhraseStore(const UserPhraseStore&) = delete;
  UserPhraseStore& operator=(const UserPhraseStore&) = delete;

  // A session suspends the engine's writes to the user database; refresh it
  // at least every few minutes, or the engine takes it for a crashed editor.
  void beginEditingSession();
  void refreshEditingSession();
  void endEditingSession();

  // filter: a substring of the phrase, or a Bopomofo prefix of the reading
  std::size_t count(const std::string& filter) const;
  std::vector<UserPhrase> phrases(const std::string& filter, UserPhraseOrder order,
                                  bool ascending, std::size_t offset,
                                  std::size_t limit) const;
  bool contains(const std::string& phrase) const;

  // An empty reading takes each character's most likely one; a given one has
  // to have a syllable per character.
  bool add(const std::string& phrase, const std::string& reading,
           UserPhrase* added = nullptr);
  // the reading is derived again for the new text
  bool setPhrase(long long rowid, const std::string& phrase);
  bool setReading(long long rowid, const std::string& reading);
  bool remove(const std::vector<long long>& rowids);

  // most probable first; never empty
  std::vector<std::string> readingsForCharacter(const std::string& character) const;
  std::string defaultReading(const std::string& phrase) const;

  // MJSR 1.0.0, the format the mac app and Yahoo! KeyKey read and write
  bool exportTo(const std::string& path) const;
  // Phrases already here are kept. Learning data in the file replaces this
  // machine's; learningDataRestored says whether that part went through.
  bool importFrom(const std::string& path, bool* learningDataRestored = nullptr);

 private:
  class Impl;
  explicit UserPhraseStore(std::unique_ptr<Impl> impl);
  std::unique_ptr<Impl> impl_;
};

}  // namespace ChiaKey

#endif
