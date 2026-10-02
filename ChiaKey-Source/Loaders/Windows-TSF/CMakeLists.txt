cmake_minimum_required(VERSION 3.21)

project(ChiaKeyWindowsTsf VERSION 0.1.0 LANGUAGES C CXX)

set(CHIAKEY_PRERELEASE "" CACHE STRING "Release suffix, e.g. beta.1")
set(CHIAKEY_DISPLAY_VERSION "${PROJECT_VERSION}")
if(CHIAKEY_PRERELEASE)
    string(APPEND CHIAKEY_DISPLAY_VERSION "-${CHIAKEY_PRERELEASE}")
endif()

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
    UpdateLexicon.cpp
)
target_include_directories(chiakey_tsf_engine PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(chiakey_tsf_engine PUBLIC chiakey_core advapi32 ole32 shell32)

# TIP icons use Icons/*.svg; the settings/installer icon uses the Mac app artwork.
set(CHIAKEY_ICON_DIR "${CMAKE_CURRENT_BINARY_DIR}/icons")
set(CHIAKEY_ICON_RESOURCES
    "IDI_CHIAKEY:badge"
    "IDI_CHINESE:chinese-on-light" "IDI_CHINESE_DARK:chinese-on-dark"
    "IDI_ENGLISH:english-on-light" "IDI_ENGLISH_DARK:english-on-dark"
    "IDI_FULL_WIDTH:full-on-light" "IDI_FULL_WIDTH_DARK:full-on-dark"
    "IDI_HALF_WIDTH:half-on-light" "IDI_HALF_WIDTH_DARK:half-on-dark"
    "IDI_ZHUYIN:zhuyin-on-light" "IDI_ZHUYIN_DARK:zhuyin-on-dark"
    "IDI_CANGJIE:cangjie-on-light" "IDI_CANGJIE_DARK:cangjie-on-dark"
    "IDI_SIMPLEX:simplex-on-light" "IDI_SIMPLEX_DARK:simplex-on-dark")
set(CHIAKEY_ICON_FILES)
list(APPEND CHIAKEY_ICON_FILES "${CHIAKEY_ICON_DIR}/app.ico" "${CHIAKEY_ICON_DIR}/phrase-editor.ico")
set(CHIAKEY_PHRASE_ICON "${CHIAKEY_SOURCE}/Utilities/PhraseEditor/OSX/Images/PhraseEditor.icns")
set(CHIAKEY_APP_ICON "${CHIAKEY_SOURCE}/Loaders/OSX-IMK/Images/ChiaKey.icns")
file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/IconIds.h" icon_ids_path)
string(REPLACE "\\" "\\\\" icon_ids_path "${icon_ids_path}")
set(CHIAKEY_ICON_RC "#include \"${icon_ids_path}\"\n")
foreach(entry IN LISTS CHIAKEY_ICON_RESOURCES)
    string(REPLACE ":" ";" entry "${entry}")
    list(GET entry 0 icon_id)
    list(GET entry 1 icon_name)
    list(APPEND CHIAKEY_ICON_FILES "${CHIAKEY_ICON_DIR}/${icon_name}.ico")
    file(TO_NATIVE_PATH "${CHIAKEY_ICON_DIR}/${icon_name}.ico" icon_path)
    string(REPLACE "\\" "\\\\" icon_path "${icon_path}")
    string(APPEND CHIAKEY_ICON_RC "${icon_id} ICON \"${icon_path}\"\n")
endforeach()
file(CONFIGURE OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ChiaKeyIcons.rc"
     CONTENT "${CHIAKEY_ICON_RC}" @ONLY)
file(GLOB CHIAKEY_ICON_SVGS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/Icons/*.svg")
add_custom_command(
    OUTPUT ${CHIAKEY_ICON_FILES}
    COMMAND powershell -NoProfile -STA -ExecutionPolicy Bypass
            -File "${CMAKE_CURRENT_SOURCE_DIR}/Icons/build-icons.ps1"
            -SvgDir "${CMAKE_CURRENT_SOURCE_DIR}/Icons" -OutDir "${CHIAKEY_ICON_DIR}"
            -AppIconPath "${CHIAKEY_APP_ICON}" -PhraseIconPath "${CHIAKEY_PHRASE_ICON}"
    DEPENDS ${CHIAKEY_ICON_SVGS} "${CMAKE_CURRENT_SOURCE_DIR}/Icons/build-icons.ps1" "${CHIAKEY_APP_ICON}" "${CHIAKEY_PHRASE_ICON}"
    COMMENT "Building the TIP icons from Icons/*.svg"
    VERBATIM)
set_source_files_properties("${CMAKE_CURRENT_BINARY_DIR}/ChiaKeyIcons.rc" PROPERTIES
    OBJECT_DEPENDS "${CHIAKEY_ICON_FILES}")

add_library(ChiaKeyTsf SHARED
    CandidateWindow.cpp
    ComServer.cpp
    LangBarButton.cpp
    PhraseEditorExports.cpp
    SymbolWindow.cpp
    TextService.cpp
    ChiaKeyTsf.rc
    "${CMAKE_CURRENT_BINARY_DIR}/ChiaKeyIcons.rc"
    ${CHIAKEY_ICON_FILES}
    ChiaKeyTsf.def
)
target_compile_definitions(ChiaKeyTsf PRIVATE
    CHIAKEY_VERSION_MAJOR=${PROJECT_VERSION_MAJOR}
    CHIAKEY_VERSION_MINOR=${PROJECT_VERSION_MINOR}
    CHIAKEY_VERSION_PATCH=${PROJECT_VERSION_PATCH}
    CHIAKEY_VERSION_STRING="${CHIAKEY_DISPLAY_VERSION}"
)
target_compile_options(ChiaKeyTsf PRIVATE /W4 /permissive-)
target_link_libraries(ChiaKeyTsf PRIVATE chiakey_tsf_engine comctl32 gdi32 msimg32 oleaut32 user32
                                         uuid uxtheme)
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

# Every Windows 10 and 11 has .NET Framework 4.8 and its C# 5 compiler, so the
# settings app needs neither a .NET SDK nor a runtime installer.
find_program(CHIAKEY_CSC csc
    PATHS "$ENV{WINDIR}/Microsoft.NET/Framework64/v4.0.30319"
          "$ENV{WINDIR}/Microsoft.NET/Framework/v4.0.30319"
    NO_DEFAULT_PATH)
if(CHIAKEY_CSC)
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(CHIAKEY_MANAGED_PLATFORM x64)
    else()
        set(CHIAKEY_MANAGED_PLATFORM x86)
    endif()
    set(CHIAKEY_SETTINGS_EXE "${CMAKE_CURRENT_BINARY_DIR}/ChiaKeySettings.exe")
    configure_file(Settings/AssemblyInfo.cs.in "${CMAKE_CURRENT_BINARY_DIR}/AssemblyInfo.cs" @ONLY)
    set(CHIAKEY_SETTINGS_SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Settings/Plist.cs"
        "${CMAKE_CURRENT_SOURCE_DIR}/Settings/SettingsForm.cs"
        "${CMAKE_CURRENT_SOURCE_DIR}/Settings/PhraseEditor.cs"
        "${CMAKE_CURRENT_SOURCE_DIR}/Settings/UpdateService.cs"
        "${CMAKE_CURRENT_SOURCE_DIR}/Settings/UpdatePane.cs"
        "${CMAKE_CURRENT_BINARY_DIR}/AssemblyInfo.cs")
    set(CHIAKEY_SETTINGS_ICONS "${CHIAKEY_SOURCE}/PreferenceApplications/OSX/Images")
    # csc reads forward slashes as option prefixes, so every path goes native
    set(CHIAKEY_SETTINGS_ARGUMENTS)
    foreach(icon general phonetic cangjie simplex generic phrase plugin update)
        file(TO_NATIVE_PATH "${CHIAKEY_SETTINGS_ICONS}/${icon}.tiff" icon_path)
        list(APPEND CHIAKEY_SETTINGS_ARGUMENTS "/resource:${icon_path},${icon}.tiff")
    endforeach()
    # Yahoo! KeyKey's phrase editor toolbar
    set(CHIAKEY_SETTINGS_IMAGES)
    foreach(image add remove editPhrase editReading)
        set(image_source "${CMAKE_CURRENT_SOURCE_DIR}/Settings/Images/${image}.ico")
        list(APPEND CHIAKEY_SETTINGS_IMAGES "${image_source}")
        file(TO_NATIVE_PATH "${image_source}" image_path)
        list(APPEND CHIAKEY_SETTINGS_ARGUMENTS "/resource:${image_path},${image}.ico")
    endforeach()
    foreach(source IN LISTS CHIAKEY_SETTINGS_SOURCES)
        file(TO_NATIVE_PATH "${source}" source_path)
        list(APPEND CHIAKEY_SETTINGS_ARGUMENTS "${source_path}")
    endforeach()
    file(TO_NATIVE_PATH "${CHIAKEY_SETTINGS_EXE}" settings_exe_path)
    file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Settings/ChiaKeySettings.manifest"
         settings_manifest_path)
    file(TO_NATIVE_PATH "${CHIAKEY_ICON_DIR}/app.ico" settings_icon_path)
    # Load the window icon from the assembly, avoiding the shell's icon cache.
    file(TO_NATIVE_PATH "${CHIAKEY_ICON_DIR}/phrase-editor.ico" phrase_icon_path)
    list(APPEND CHIAKEY_SETTINGS_ARGUMENTS "/resource:${phrase_icon_path},phrase-editor.ico")
    list(APPEND CHIAKEY_SETTINGS_ARGUMENTS "/resource:${settings_icon_path},app.ico")
    add_custom_command(
        OUTPUT "${CHIAKEY_SETTINGS_EXE}"
        # the LIB that vcvars sets lists MSVC folders csc warns about
        COMMAND "${CMAKE_COMMAND}" -E env --unset=LIB
                "${CHIAKEY_CSC}" /nologo /target:winexe /optimize+ /utf8output
                "/platform:${CHIAKEY_MANAGED_PLATFORM}"
                "/out:${settings_exe_path}" "/win32manifest:${settings_manifest_path}"
                "/win32icon:${settings_icon_path}"
                /r:System.dll /r:System.Drawing.dll /r:System.Windows.Forms.dll /r:System.Xml.dll
                /r:System.Web.Extensions.dll
                ${CHIAKEY_SETTINGS_ARGUMENTS}
        DEPENDS ${CHIAKEY_SETTINGS_SOURCES} ${CHIAKEY_SETTINGS_IMAGES}
                "${CMAKE_CURRENT_SOURCE_DIR}/Settings/ChiaKeySettings.manifest"
                "${CHIAKEY_ICON_DIR}/app.ico" "${CHIAKEY_ICON_DIR}/phrase-editor.ico"
        COMMENT "Compiling ChiaKeySettings.exe"
        VERBATIM)
    add_custom_target(ChiaKeySettings ALL
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${CHIAKEY_SETTINGS_EXE}"
                "$<TARGET_FILE_DIR:ChiaKeyTsf>/ChiaKeySettings.exe"
        DEPENDS "${CHIAKEY_SETTINGS_EXE}"
        VERBATIM)
    add_dependencies(ChiaKeySettings ChiaKeyTsf)
    if(BUILD_TESTING AND EXISTS "${CHIAKEY_LEXICON_DATABASE}")
        file(TO_NATIVE_PATH "${CMAKE_CURRENT_BINARY_DIR}/ChiaKeyUpdateTests.exe" update_tests_exe)
        file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Settings/Plist.cs" update_tests_plist)
        file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Settings/UpdateService.cs" update_tests_service)
        file(TO_NATIVE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/Settings/UpdateTests.cs" update_tests_source)
        add_custom_command(
            OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/ChiaKeyUpdateTests.exe"
            COMMAND "${CMAKE_COMMAND}" -E env --unset=LIB "${CHIAKEY_CSC}" /nologo /utf8output
                    /target:exe "/out:${update_tests_exe}" /r:System.dll /r:System.Xml.dll
                    "/platform:${CHIAKEY_MANAGED_PLATFORM}"
                    /r:System.Windows.Forms.dll /r:System.Web.Extensions.dll
                    "${update_tests_plist}" "${update_tests_service}" "${update_tests_source}"
            DEPENDS Settings/Plist.cs Settings/UpdateService.cs Settings/UpdateTests.cs
            VERBATIM)
        add_custom_target(ChiaKeyUpdateTests ALL
            COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                    "${CMAKE_CURRENT_BINARY_DIR}/ChiaKeyUpdateTests.exe"
                    "$<TARGET_FILE_DIR:ChiaKeyTsf>/ChiaKeyUpdateTests.exe"
            DEPENDS "${CMAKE_CURRENT_BINARY_DIR}/ChiaKeyUpdateTests.exe" ChiaKeyTsf
            VERBATIM)
        add_test(NAME chiakey_windows_updates
            COMMAND "$<TARGET_FILE_DIR:ChiaKeyTsf>/ChiaKeyUpdateTests.exe"
                    "${CMAKE_CURRENT_BINARY_DIR}" "${CHIAKEY_LEXICON_DATABASE}")
    endif()
else()
    message(WARNING "csc.exe from .NET Framework 4 not found: ChiaKeySettings.exe is not built.")
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
