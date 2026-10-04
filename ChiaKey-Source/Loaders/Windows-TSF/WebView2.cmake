# Microsoft SDK only; Evergreen Runtime is a separately serviced system component.
# Keep the package hash pinned so local and CI builds consume the same bytes.
set(CHIAKEY_WEBVIEW2_VERSION "1.0.3537.50")
set(CHIAKEY_WEBVIEW2_SHA256 "5ea526bbd728adda0da4d31219267e96460494a427e4894c4e09d9f320f4b9aa")
set(CHIAKEY_WEBVIEW2_DIR "${CMAKE_CURRENT_BINARY_DIR}/webview2-${CHIAKEY_WEBVIEW2_VERSION}")
set(webview_archive "${CMAKE_CURRENT_BINARY_DIR}/webview2-${CHIAKEY_WEBVIEW2_VERSION}.zip")
if(NOT EXISTS "${webview_archive}")
    file(DOWNLOAD
        "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/${CHIAKEY_WEBVIEW2_VERSION}/microsoft.web.webview2.${CHIAKEY_WEBVIEW2_VERSION}.nupkg"
        "${webview_archive}" EXPECTED_HASH "SHA256=${CHIAKEY_WEBVIEW2_SHA256}" TLS_VERIFY ON)
endif()
file(SHA256 "${webview_archive}" webview_hash)
if(NOT webview_hash STREQUAL CHIAKEY_WEBVIEW2_SHA256)
    message(FATAL_ERROR "WebView2 SDK archive checksum mismatch: ${webview_archive}")
endif()
file(ARCHIVE_EXTRACT INPUT "${webview_archive}" DESTINATION "${CHIAKEY_WEBVIEW2_DIR}")
set(CHIAKEY_WEBVIEW2_FILES
    "${CHIAKEY_WEBVIEW2_DIR}/lib/net462/Microsoft.Web.WebView2.Core.dll"
    "${CHIAKEY_WEBVIEW2_DIR}/lib/net462/Microsoft.Web.WebView2.WinForms.dll"
    "${CHIAKEY_WEBVIEW2_DIR}/runtimes/win-${CHIAKEY_MANAGED_PLATFORM}/native/WebView2Loader.dll")
