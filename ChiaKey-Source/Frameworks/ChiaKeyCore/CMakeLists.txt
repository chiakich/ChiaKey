cmake_minimum_required(VERSION 3.21)

# Builds ChiaKeyCore and the engine chain it wraps for non-Xcode hosts.
# The macOS IMK host does not use this file; it builds from Takao.xcodeproj.
project(ChiaKeyCore LANGUAGES C CXX)

include(CTest)

get_filename_component(CHIAKEY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CHIAKEY_ROOT "${CHIAKEY_SOURCE}/.." ABSOLUTE)

set(CHIAKEY_LEXICON_DATABASE
    "${CHIAKEY_SOURCE}/Distributions/Takao/CookedDatabase/ChiaKeySource.db"
    CACHE FILEPATH "ChiaKeySource.db used by the smoke test")

# --- third-party -------------------------------------------------------------

add_library(chiakey_third_party STATIC
    "${CHIAKEY_SOURCE}/ExternalLibraries/sqlite/sqlite3.c"
    "${CHIAKEY_SOURCE}/ExternalLibraries/expat/lib/xmlparse.c"
    "${CHIAKEY_SOURCE}/ExternalLibraries/expat/lib/xmlrole.c"
    "${CHIAKEY_SOURCE}/ExternalLibraries/expat/lib/xmltok.c"
)
target_include_directories(chiakey_third_party PUBLIC
    "${CHIAKEY_SOURCE}/ExternalLibraries/sqlite"
    "${CHIAKEY_SOURCE}/ExternalLibraries/expat/lib"
)
target_compile_definitions(chiakey_third_party
    PRIVATE
        SQLITE_THREADSAFE=1
        SQLITE_OMIT_LOAD_EXTENSION
        _CRT_SECURE_NO_WARNINGS
    PUBLIC
        XML_STATIC
)
if(WIN32)
    # Selects expat's winconfig.h, which carries the settings below.
    target_compile_definitions(chiakey_third_party PRIVATE COMPILED_FROM_DSP)
else()
    # Everywhere else expat expects the autotools-generated expat_config.h,
    # which this tree does not ship, so pass its contents directly.
    if(CMAKE_CXX_BYTE_ORDER STREQUAL "BIG_ENDIAN")
        set(CHIAKEY_EXPAT_BYTEORDER 4321)
    else()
        set(CHIAKEY_EXPAT_BYTEORDER 1234)
    endif()
    target_compile_definitions(chiakey_third_party PRIVATE
        HAVE_MEMMOVE
        XML_NS=1
        XML_DTD=1
        XML_CONTEXT_BYTES=1024
        BYTEORDER=${CHIAKEY_EXPAT_BYTEORDER}
    )
endif()
set_target_properties(chiakey_third_party PROPERTIES POSITION_INDEPENDENT_CODE ON)

# --- core --------------------------------------------------------------------

add_library(chiakey_core STATIC
    "${CHIAKEY_SOURCE}/Frameworks/ChiaKeyCore/Source/ChiaKeyCore.cpp"
    "${CHIAKEY_SOURCE}/Frameworks/ChiaKeyCore/Source/ChiaKeyCoreC.cpp"
    "${CHIAKEY_SOURCE}/Frameworks/OpenVanilla/Source/OVFrameworkInfo.cpp"
    "${CHIAKEY_SOURCE}/Frameworks/PlainVanilla/Source/PVPropertyListExpat.cpp"
    "${CHIAKEY_SOURCE}/Frameworks/Formosa/Source/Mandarin.cpp"
    "${CHIAKEY_SOURCE}/Frameworks/Manjusri/Source/Node.cpp"
    "${CHIAKEY_SOURCE}/ModulePackages/OVIMMandarin/OVIMSmartMandarin.cpp"
    "${CHIAKEY_SOURCE}/ModulePackages/OVIMMandarin/OVIMTraditionalMandarin.cpp"
    "${CHIAKEY_SOURCE}/ModulePackages/OVIMMandarin/OVAFAssociatedPhrase.cpp"
)
target_compile_features(chiakey_core PUBLIC cxx_std_17)
target_include_directories(chiakey_core PUBLIC
    "${CHIAKEY_SOURCE}/Frameworks/ChiaKeyCore/Headers"
    "${CHIAKEY_SOURCE}/Frameworks/OpenVanilla/Headers"
    "${CHIAKEY_SOURCE}/Frameworks/PlainVanilla/Headers"
    "${CHIAKEY_SOURCE}/Frameworks/Formosa/Headers"
    "${CHIAKEY_SOURCE}/Frameworks/Manjusri/Headers"
    "${CHIAKEY_SOURCE}/ModulePackages/OVIMMandarin"
)
# Must match Takao.xcodeproj: these name the lexicon's table prefixes, so a
# mismatch compiles cleanly and then finds no data at runtime.
target_compile_definitions(chiakey_core PUBLIC
    OV_USE_SQLITE
    OVIMMANDARIN_IDENTIFIER=\"Mandarin\"
    OVIMMANDARIN_PUNCTUATIONS_TABLE_PREFIX=\"Punctuations\"
    OVIMSMARTMANDARIN_IDENTIFIER=\"SmartMandarin\"
    OVIMTRADITIONALMANDARIN_IDENTIFIER=\"TraditionalMandarin\"
    OVIMTRADITIONALMANDARIN_USE_ABSOLUTE_ORDER_QUERY_STRING=1
    OVAFASSOCIATEDPHRASE_IDENTIFIER=\"AssociatedPhrase\"
)
target_link_libraries(chiakey_core PUBLIC chiakey_third_party)
set_target_properties(chiakey_core PROPERTIES POSITION_INDEPENDENT_CODE ON)

if(WIN32)
    # The engine branches on WIN32, which MSVC does not predefine (it defines
    # only _WIN32). Without this the POSIX branches compile and fail on dirent.h.
    target_compile_definitions(chiakey_core PUBLIC
        WIN32
        UNICODE
        _UNICODE
        WIN32_LEAN_AND_MEAN
        NOMINMAX
        _CRT_SECURE_NO_WARNINGS
    )
    # SHGetFolderPathW, used by OVDirectoryHelper for the user data directory.
    target_link_libraries(chiakey_core PUBLIC shell32)
elseif(APPLE)
    # Under __APPLE__ the headers include each other framework-style, as
    # <OpenVanilla/...>; the shim directory is a tree of symlinks serving that.
    target_include_directories(chiakey_core PUBLIC
        "${CHIAKEY_SOURCE}/Frameworks/HeaderShims")
else()
    message(WARNING
        "OVFileHelper.h and PVLoaderPolicy.h still have #error stops for Linux.")
endif()

if(MSVC)
    # The sources carry UTF-8 literals; without /utf-8 MSVC reads them as ANSI.
    target_compile_options(chiakey_core PUBLIC /utf-8 /bigobj)
    # Left at the CMake default. A TSF DLL wants the static runtime, so whoever
    # links this must set CMAKE_MSVC_RUNTIME_LIBRARY for both or neither.
endif()

# --- smoke test --------------------------------------------------------------

if(BUILD_TESTING)
    add_executable(chiakey_core_smoke
        "${CHIAKEY_SOURCE}/Frameworks/ChiaKeyCore/Tests/ChiaKeyCoreSmoke.cpp")
    target_link_libraries(chiakey_core_smoke PRIVATE chiakey_core)

    if(EXISTS "${CHIAKEY_LEXICON_DATABASE}")
        # The engine learns into the writable path, so a leftover one would
        # make the run depend on the previous one.
        add_test(NAME chiakey_core_smoke_reset
            COMMAND "${CMAKE_COMMAND}" -E rm -rf
                    "${CMAKE_CURRENT_BINARY_DIR}/smoke-writable")
        set_tests_properties(chiakey_core_smoke_reset PROPERTIES
            FIXTURES_SETUP chiakey_core_smoke_writable)

        add_test(NAME chiakey_core_smoke
            COMMAND chiakey_core_smoke
                    "${CHIAKEY_ROOT}"
                    "${CMAKE_CURRENT_BINARY_DIR}/smoke-writable"
                    "${CHIAKEY_LEXICON_DATABASE}")
        set_tests_properties(chiakey_core_smoke PROPERTIES
            FIXTURES_REQUIRED chiakey_core_smoke_writable)
    else()
        message(WARNING
            "ChiaKeySource.db not found, so the smoke test is built but not "
            "registered. It is not in git; copy it in or pass "
            "-DCHIAKEY_LEXICON_DATABASE=<path>. Looked at: "
            "${CHIAKEY_LEXICON_DATABASE}")
    endif()
endif()
