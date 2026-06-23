# Lexicon release note: remove OneKey service data

Chiaki KeyKey has removed the legacy Yahoo KeyKey OneKey feature from the
modern macOS app.

OneKey was not part of the input lexicon. It was a Yahoo-era URL launcher:
the app loaded a plist of web services such as Yahoo Search, Taiwan stock
lookup, Wretch search, Yahoo Auction, and Yahoo Maps, then opened URLs with
the user's query. The database key for that service list was:

- `prepopulated_service_data.key = 'onekey_services'`
- historical builds may also mention `onekey_services_timestamp`

Modern Chiaki KeyKey no longer loads the OneKey module, no longer fetches or
merges OneKey plist data, no longer exposes OneKey preferences, and no longer
uses the backtick key (`) as a OneKey shortcut.

## What the lexicon repo should do

Future `Chiaki-KeyKey-Lexicon` releases should omit OneKey data.

Required action:

1. Do not generate or ship `onekey_services`.
2. Do not generate or ship `onekey_services_timestamp`.
3. Remove any CI assertion that requires those keys.
4. Keep the existing `prepopulated_service_data` table.
5. Keep `canned_messages` and `canned_messages_timestamp`; those are still used.
6. Keep the punctuation tables and symbol data validations unchanged.

Optional cleanup:

CI may fail a release if `onekey_services` is present, so new releases do not
accidentally keep obsolete Yahoo web-service data.

Compatibility note:

Older lexicon releases that still contain `onekey_services` are harmless in
newer Chiaki KeyKey builds. The app ignores the key. New releases should omit
it to keep the database contract focused on input data.
