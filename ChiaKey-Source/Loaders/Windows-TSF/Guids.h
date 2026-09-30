#pragma once

#include <Windows.h>

namespace ChiaKey::WindowsTsf {

// {25CF860B-9F43-4247-BF94-1077848E5D84}
inline constexpr CLSID kTextServiceClsid = {
    0x25cf860b, 0x9f43, 0x4247, {0xbf, 0x94, 0x10, 0x77, 0x84, 0x8e, 0x5d, 0x84}};

// {6B099DE6-58BA-4E93-AE22-6483738FA15A}
inline constexpr GUID kTraditionalChineseProfileGuid = {
    0x6b099de6, 0x58ba, 0x4e93, {0xae, 0x22, 0x64, 0x83, 0x73, 0x8f, 0xa1, 0x5a}};

// New Phonetic style: the whole composition is dotted, the focused word solid.
// {52F7F3D8-E7EE-4862-AAD4-BAA2F6F5F46D}
inline constexpr GUID kInputDisplayAttributeGuid = {
    0x52f7f3d8, 0xe7ee, 0x4862, {0xaa, 0xd4, 0xba, 0xa2, 0xf6, 0xf5, 0xf4, 0x6d}};
// {C5C5539B-6BC4-466B-86E0-B4556D33E175}
inline constexpr GUID kFocusedDisplayAttributeGuid = {
    0xc5c5539b, 0x6bc4, 0x466b, {0x86, 0xe0, 0xb4, 0x55, 0x6d, 0x33, 0xe1, 0x75}};

// the symbol window's Ctrl+Alt+.
// {31271245-36F6-4472-83DB-D70934469652}
inline constexpr GUID kSymbolWindowKeyGuid = {
    0x31271245, 0x36f6, 0x4472, {0x83, 0xdb, 0xd7, 0x09, 0x34, 0x46, 0x96, 0x52}};

// the lexicon's Ctrl+Alt chords; the last byte is the chord's index
// {271B88E0-8F2F-49FF-9FAF-E7050E55D600}
inline constexpr GUID kPunctuationChordKeyGuidBase = {
    0x271b88e0, 0x8f2f, 0x49ff, {0x9f, 0xaf, 0xe7, 0x05, 0x0e, 0x55, 0xd6, 0x00}};

inline constexpr LANGID kTraditionalChineseLangId = 0x0404;
inline constexpr wchar_t kTextServiceDescription[] = L"千秋輸入法";
inline constexpr wchar_t kThreadingModel[] = L"Apartment";

}  // namespace ChiaKey::WindowsTsf
