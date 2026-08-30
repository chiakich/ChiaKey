//
//  PEUserPhraseStore.h
//  PhraseEditor
//
//  Data layer for the rewritten phrase editor. Talks to the user phrase
//  SQLite DB directly (WAL, rowid-carrying operations, windowed queries) and
//  coordinates with the running input method over distributed notifications.
//

#import <Foundation/Foundation.h>

// One user phrase row. Identity is the SQLite rowid, which is stable for the
// whole editing session because neither side VACUUMs while the editor runs.
@interface PEPhraseRecord : NSObject {
  long long _rowid;
  NSString *_phrase;
  NSString *_reading;  // composed Bopomofo, comma-separated syllables
}
@property(nonatomic, assign) long long rowid;
@property(nonatomic, retain) NSString *phrase;
@property(nonatomic, retain) NSString *reading;
@end

typedef NS_ENUM(NSInteger, PEPhraseSortKey) {
  PEPhraseSortKeyInsertion = 0,  // rowid
  PEPhraseSortKeyPhrase = 1,
  PEPhraseSortKeyReading = 2,  // qstring byte order
};

@interface PEUserPhraseStore : NSObject

+ (instancetype)sharedStore;

- (BOOL)isAvailable;

// NO when no main lexicon can be reached; derived readings then fall back to
// a placeholder, so callers may want to warn the user.
- (BOOL)isLexiconAvailable;

// Editing session; posts the coordination notifications. Begin on window
// open, end on app termination.
- (void)beginEditingSession;
- (void)endEditingSession;

#pragma mark Windowed queries

// filter: nil/empty for all rows; otherwise substring match on the phrase,
// plus reading-prefix match when the filter parses as composed Bopomofo.
- (NSUInteger)numberOfPhrasesMatchingFilter:(NSString *)filter;
- (void)invalidateCachedCounts;
- (NSArray *)phrasesInRange:(NSRange)range
                     filter:(NSString *)filter
                    sortKey:(PEPhraseSortKey)sortKey
                  ascending:(BOOL)ascending;
- (PEPhraseRecord *)phraseForRowid:(long long)rowid;

#pragma mark Mutations (rowid-carrying; each commit marks the DB dirty)

// Returns the new record (with default derived reading), or nil on failure.
- (PEPhraseRecord *)addPhrase:(NSString *)phrase;
// Batch insert in a single transaction; returns the inserted records.
- (NSArray *)addPhrases:(NSArray *)phrases;
- (BOOL)containsPhrase:(NSString *)phrase;
- (void)setPhrase:(NSString *)phrase forRowid:(long long)rowid;
- (void)setReading:(NSString *)reading forRowid:(long long)rowid;
// Batch delete in a single transaction.
- (void)deletePhrasesWithRowids:(NSArray *)rowids;

#pragma mark Reading derivation

// Per-character candidate readings, most probable first. Falls back to the
// Mandarin-bpmf-cin table, then to a placeholder, so it is never empty.
- (NSArray *)readingsForCharacter:(NSString *)character;
// Most probable reading for each character, comma-joined.
- (NSString *)defaultReadingForPhrase:(NSString *)phrase;

#pragma mark Import / export (full MJSR, including the learning-data blob)

- (BOOL)exportUserPhraseDBToFile:(NSString *)path;

// YES once the phrases are committed. The learning-data blob is a separate
// outcome: it can be refused for size or fail to restore long after the
// phrases have landed, and reporting that as a failed import sends the user
// back to re-import a file that is already in.
- (BOOL)importUserPhraseDBFromFile:(NSString *)path;
- (BOOL)importUserPhraseDBFromFile:(NSString *)path
              learningDataRestored:(BOOL *)learningDataRestored;
// Same format, but written by Yahoo! KeyKey: its numbers were estimated
// against a different lexicon, so imported phrases are renormalized to what a
// hand-added phrase gets, and learning-cache entries naming something this
// lexicon cannot produce are dropped.
- (BOOL)importLegacyUserPhraseDBFromFile:(NSString *)path;
- (BOOL)importLegacyUserPhraseDBFromFile:(NSString *)path
                    learningDataRestored:(BOOL *)learningDataRestored;

@end
