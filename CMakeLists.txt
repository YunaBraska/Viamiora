cmake_minimum_required(VERSION 3.24)
if(CMAKE_HOST_SYSTEM_NAME STREQUAL "Darwin" AND NOT CMAKE_OSX_DEPLOYMENT_TARGET)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "14.4" CACHE STRING "Minimum supported macOS version" FORCE)
endif()
project(Viamiora VERSION 2026.11.19 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(CMAKE_SYSTEM_NAME STREQUAL "iOS")
    message(FATAL_ERROR "The iOS/iPadOS app needs native calendar, contacts, credential, and media integration before it can be built. See docs/spec/platform-delivery.md.")
endif()
if(NOT CMAKE_SYSTEM_NAME STREQUAL "Darwin")
    message(FATAL_ERROR "Viamiora currently builds only on macOS. Native adapters and installed-package checks are required for other platforms. See docs/spec/platform-delivery.md.")
endif()
enable_language(OBJCXX)

include(CTest)
find_package(Qt6 6.11 REQUIRED COMPONENTS Quick QuickControls2 Network WebView Concurrent LinguistTools)
if(BUILD_TESTING)
    find_package(Qt6 6.11 REQUIRED COMPONENTS Test)
endif()
if(APPLE)
    set(VIAMIORA_QT_MACOS_MINIMUM "14.4")
    foreach(component Core Gui Qml Quick QuickControls2 Network WebView Concurrent)
        get_target_property(qt_binary Qt6::${component} IMPORTED_LOCATION)
        if(NOT qt_binary)
            message(FATAL_ERROR "Cannot inspect Qt ${component} deployment target")
        endif()
        if(qt_binary MATCHES "^(/opt/homebrew|/usr/local)/")
            set(VIAMIORA_PACKAGE_MANAGER_QT TRUE)
        endif()
        execute_process(COMMAND /usr/bin/otool -l "${qt_binary}"
            OUTPUT_VARIABLE qt_load_commands RESULT_VARIABLE inspection_result)
        if(NOT inspection_result EQUAL 0)
            message(FATAL_ERROR "Cannot inspect Qt ${component} deployment target")
        endif()
        string(REGEX MATCHALL "minos[ \t]+[0-9]+(\\.[0-9]+)+" qt_minimums "${qt_load_commands}")
        if(NOT qt_minimums)
            message(FATAL_ERROR "Qt ${component} has no readable macOS deployment target")
        endif()
        foreach(qt_minimum IN LISTS qt_minimums)
            string(REGEX REPLACE "minos[ \t]+" "" qt_minimum "${qt_minimum}")
            if(qt_minimum VERSION_GREATER VIAMIORA_QT_MACOS_MINIMUM)
                set(VIAMIORA_QT_MACOS_MINIMUM "${qt_minimum}")
            endif()
        endforeach()
    endforeach()
    # Homebrew may link transitive libraries that require the host's macOS patch level.
    if(VIAMIORA_PACKAGE_MANAGER_QT AND CMAKE_HOST_SYSTEM_VERSION VERSION_GREATER VIAMIORA_QT_MACOS_MINIMUM)
        set(VIAMIORA_QT_MACOS_MINIMUM "${CMAKE_HOST_SYSTEM_VERSION}")
    endif()
    if(VIAMIORA_QT_MACOS_MINIMUM VERSION_GREATER CMAKE_OSX_DEPLOYMENT_TARGET)
        message(STATUS "Raising macOS deployment target to ${VIAMIORA_QT_MACOS_MINIMUM} for the selected dependencies")
        set(CMAKE_OSX_DEPLOYMENT_TARGET "${VIAMIORA_QT_MACOS_MINIMUM}" CACHE STRING "Minimum supported macOS version" FORCE)
    endif()
endif()
qt_standard_project_setup(REQUIRES 6.11 I18N_TRANSLATED_LANGUAGES de sv)

add_executable(viamiora MACOSX_BUNDLE
    src/main.cpp
    src/ContactStore.cpp
    src/ContactStore.hpp
    src/ClipboardService.hpp
    src/BrandingService.cpp
    src/BrandingService.hpp
    src/AppearanceService.cpp
    src/AppearanceService.hpp
    src/MeetingStatus.hpp
    src/CalendarService.hpp
    src/SystemContacts.hpp
    src/PortalConfig.cpp
    src/PortalConfig.hpp
    src/PortalSession.cpp
    src/PortalSessionMeetings.cpp
    src/PortalSessionDetails.cpp
    src/PortalSessionBooking.cpp
    src/PortalSession.hpp
    src/MeetingBooking.cpp
    src/MeetingBooking.hpp
    src/MeetingInvitation.cpp
    src/MeetingInvitation.hpp
    src/CredentialStore.hpp
)
target_sources(viamiora PRIVATE assets/app-icon.icns LICENSE)
set_source_files_properties(assets/app-icon.icns PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_source_files_properties(LICENSE PROPERTIES MACOSX_PACKAGE_LOCATION Resources)
set_target_properties(viamiora PROPERTIES MACOSX_BUNDLE_ICON_FILE app-icon.icns)
target_sources(viamiora PRIVATE src/CalendarService_mac.mm src/SystemContacts_mac.mm src/CredentialStore_mac.mm src/MeetingStatus_mac.mm)
set_source_files_properties(src/CalendarService_mac.mm src/SystemContacts_mac.mm src/CredentialStore_mac.mm src/MeetingStatus_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
target_link_libraries(viamiora PRIVATE "-framework AppKit" "-framework EventKit" "-framework Contacts" "-framework Foundation" "-framework Security" "-framework LocalAuthentication")
set_target_properties(viamiora PROPERTIES MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/src/Info.plist.in")

set_source_files_properties(qml/Main.qml PROPERTIES QT_RESOURCE_ALIAS Main.qml)
set_source_files_properties(qml/MeetingOverview.js PROPERTIES QT_RESOURCE_ALIAS MeetingOverview.js)
qt_add_qml_module(viamiora URI Viamiora VERSION 1.0 QML_FILES qml/Main.qml qml/MeetingOverview.js RESOURCES assets/login-hall.jpg assets/app-icon.png)
set_property(TARGET viamiora PROPERTY QT_EXCLUDE_SOURCES_FROM_TRANSLATION LICENSE)
qt_add_translations(viamiora NO_GENERATE_PLURALS_TS_FILE)
target_include_directories(viamiora PRIVATE src)
target_link_libraries(viamiora PRIVATE Qt6::Quick Qt6::QuickControls2 Qt6::Network Qt6::WebView Qt6::Concurrent)
target_compile_definitions(viamiora PRIVATE VIAMIORA_VERSION="${PROJECT_VERSION}" VIAMIORA_QT_VERSION="${Qt6_VERSION}")
target_compile_options(viamiora PRIVATE $<$<CXX_COMPILER_ID:AppleClang,Clang,GNU>:-Wall;-Wextra;-Wpedantic>)

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.env.local")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/.env.local")
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/.env.local" VIAMIORA_SIGN_LINE REGEX "^VIAMIORA_SIGN_IDENTITY=")
    if(VIAMIORA_SIGN_LINE)
        string(REGEX REPLACE "^VIAMIORA_SIGN_IDENTITY=" "" VIAMIORA_SIGN_IDENTITY "${VIAMIORA_SIGN_LINE}")
        set(VIAMIORA_SIGN_IDENTITY_FILE "${CMAKE_CURRENT_BINARY_DIR}/signing-identity.txt")
        file(GENERATE OUTPUT "${VIAMIORA_SIGN_IDENTITY_FILE}" CONTENT "${VIAMIORA_SIGN_IDENTITY}\n")
        set(VIAMIORA_SIGNATURE_STAMP "${CMAKE_CURRENT_BINARY_DIR}/viamiora-signed.stamp")
        add_custom_command(OUTPUT "${VIAMIORA_SIGNATURE_STAMP}"
            COMMAND /usr/bin/codesign --force --sign "${VIAMIORA_SIGN_IDENTITY}" "$<TARGET_BUNDLE_DIR:viamiora>"
            COMMAND "${CMAKE_COMMAND}" -E touch "${VIAMIORA_SIGNATURE_STAMP}"
            DEPENDS viamiora "${VIAMIORA_SIGN_IDENTITY_FILE}"
                    "${CMAKE_CURRENT_BINARY_DIR}/viamiora.app/Contents/Info.plist"
                    "${CMAKE_CURRENT_BINARY_DIR}/viamiora.app/Contents/Resources/LICENSE"
            VERBATIM)
        add_custom_target(viamiora-signed ALL DEPENDS "${VIAMIORA_SIGNATURE_STAMP}")
    endif()
endif()

if(BUILD_TESTING)
    add_test(NAME qml-lint COMMAND Qt6::qmllint -W 0 -I "${CMAKE_CURRENT_BINARY_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/qml/Main.qml")
    add_executable(meeting-overview-test tests/MeetingOverviewTest.cpp)
    target_link_libraries(meeting-overview-test PRIVATE Qt6::Test Qt6::Qml)
    target_compile_definitions(meeting-overview-test PRIVATE VIAMIORA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}")
    add_test(NAME meeting-overview COMMAND meeting-overview-test)
    add_executable(contact-store-test tests/ContactStoreTest.cpp src/ContactStore.cpp src/ContactStore.hpp)
    target_include_directories(contact-store-test PRIVATE src)
    target_link_libraries(contact-store-test PRIVATE Qt6::Test Qt6::Qml)
    add_test(NAME contacts COMMAND contact-store-test)
    add_executable(portal-config-test tests/PortalConfigTest.cpp src/PortalConfig.cpp src/PortalConfig.hpp)
    target_include_directories(portal-config-test PRIVATE src)
    target_link_libraries(portal-config-test PRIVATE Qt6::Test Qt6::Qml)
    add_test(NAME portal-config COMMAND portal-config-test)
    add_executable(portal-session-test tests/PortalSessionTest.cpp src/PortalSession.cpp src/PortalSessionMeetings.cpp src/PortalSessionDetails.cpp src/PortalSessionBooking.cpp src/PortalSession.hpp src/BrandingService.cpp src/BrandingService.hpp src/MeetingBooking.cpp src/MeetingBooking.hpp src/MeetingInvitation.cpp src/MeetingInvitation.hpp src/PortalConfig.cpp src/PortalConfig.hpp)
    target_include_directories(portal-session-test PRIVATE src)
    target_link_libraries(portal-session-test PRIVATE Qt6::Test Qt6::Gui Qt6::Network Qt6::Concurrent Qt6::Qml)
    target_compile_definitions(portal-session-test PRIVATE
        VIAMIORA_SOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}"
        VIAMIORA_VERSION="${PROJECT_VERSION}")
    target_sources(portal-session-test PRIVATE src/CredentialStore_mac.mm)
    set_source_files_properties(src/CredentialStore_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_link_libraries(portal-session-test PRIVATE "-framework Security" "-framework Foundation" "-framework LocalAuthentication")
    add_test(NAME portal-session COMMAND portal-session-test)
    add_executable(credential-store-test tests/CredentialStoreTest.mm src/CredentialStore_mac.mm src/CredentialStore.hpp)
    set_source_files_properties(tests/CredentialStoreTest.mm src/CredentialStore_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_include_directories(credential-store-test PRIVATE src)
    target_link_libraries(credential-store-test PRIVATE Qt6::Test "-framework Security" "-framework Foundation" "-framework LocalAuthentication")
    add_test(NAME keychain COMMAND credential-store-test)
    add_executable(meeting-status-test tests/MeetingStatusTest.mm src/MeetingStatus_mac.mm src/MeetingStatus.hpp)
    set_source_files_properties(tests/MeetingStatusTest.mm src/MeetingStatus_mac.mm PROPERTIES COMPILE_FLAGS "-fobjc-arc")
    target_include_directories(meeting-status-test PRIVATE src)
    target_link_libraries(meeting-status-test PRIVATE Qt6::Test Qt6::Gui Qt6::Qml "-framework AppKit")
    add_test(NAME meeting-status COMMAND meeting-status-test)
endif()
