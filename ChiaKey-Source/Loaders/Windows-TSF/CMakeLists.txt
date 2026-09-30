cmake_minimum_required(VERSION 3.21)

project(ChiaKeyWindowsTsf VERSION 0.1.0 LANGUAGES C CXX)

if(NOT WIN32)
    message(FATAL_ERROR "The Windows TSF front end can only be built on Windows.")
endif()

# The TIP is loaded into every process that types, so it cannot depend on an
# installed VC redist; the core has to use the same runtime.
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

include(CTest)

get_filename_component(CHIAKEY_SOURCE "${CMAKE_CURRENT_SOURCE_DIR}/../.." ABSOLUTE)
get_filename_component(CHIAKEY_ROOT "${CHIAKEY_SOURCE}/.." ABSOLUTE)
add_subdirectory("${CHIAKEY_SOURCE}/Frameworks/ChiaKeyCore" core)

add_library(chiakey_tsf_engine STATIC
    ChiaKeyEngine.cpp
    Diagnostics.cpp
)
target_include_directories(chiakey_tsf_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(chiakey_tsf_engine PUBLIC chiakey_core advapi32 ole32 shell32)

add_library(ChiaKeyTsf SHARED
    CandidateWindow.cpp
    ComServer.cpp
    LangBarButton.cpp
    TextService.cpp
    ChiaKeyTsf.rc
    ChiaKeyTsf.def
)
target_compile_definitions(ChiaKeyTsf PRIVATE
    CHIAKEY_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CHIAKEY_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CHIAKEY_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    CHIAKEY_VERSION_STRING="${PROJECT_VERSION}"
)
target_compile_options(ChiaKeyTsf PRIVATE /W4 /permissive-)
target_link_libraries(ChiaKeyTsf PRIVATE chiakey_tsf_engine gdi32 oleaut32 user32 uuid)
set_target_properties(ChiaKeyTsf PROPERTIES PREFIX "")

if(EXISTS "${CHIAKEY_LEXICON_DATABASE}")
    add_custom_command(TARGET ChiaKeyTsf POST_BUILD
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                "${CHIAKEY_LEXICON_DATABASE}"
                "$<TARGET_FILE_DIR:ChiaKeyTsf>/ChiaKeySource.db"
        VERBATIM)
else()
    message(WARNING "CHIAKEY_LEXICON_DATABASE not set: ChiaKeySource.db has to be "
                    "copied next to ChiaKeyTsf.dll by hand.")
endif()

if(BUILD_TESTING AND EXISTS "${CHIAKEY_LEXICON_DATABASE}")
    add_executable(chiakey_tsf_engine_test EngineTest.cpp)
    target_compile_options(chiakey_tsf_engine_test PRIVATE /W4 /permissive-)
    target_link_libraries(chiakey_tsf_engine_test PRIVATE chiakey_tsf_engine)

    add_test(NAME chiakey_tsf_engine_reset
        COMMAND "${CMAKE_COMMAND}" -E rm -rf "${CMAKE_CURRENT_BINARY_DIR}/tsf-writable")
    set_tests_properties(chiakey_tsf_engine_reset PROPERTIES
        FIXTURES_SETUP chiakey_tsf_writable)
    add_test(NAME chiakey_tsf_engine
        COMMAND chiakey_tsf_engine_test
                "${CHIAKEY_SOURCE}"
                "${CMAKE_CURRENT_BINARY_DIR}/tsf-writable"
                "${CHIAKEY_LEXICON_DATABASE}")
    set_tests_properties(chiakey_tsf_engine PROPERTIES
        FIXTURES_REQUIRED chiakey_tsf_writable)
endif()
