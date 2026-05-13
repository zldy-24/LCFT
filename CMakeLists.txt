cmake_minimum_required(VERSION 3.16)
project(LanChatShell VERSION 0.1 LANGUAGES CXX)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_AUTOMOC ON)

if(ANDROID)
    set(QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    set(QT_ANDROID_COMPILE_SDK_VERSION 35)
    set(QT_ANDROID_TARGET_SDK_VERSION 35)
    set(QT_ANDROID_MIN_SDK_VERSION 28)
endif()

find_package(Qt6 6.10 REQUIRED COMPONENTS Quick QuickControls2 QuickDialogs2 Network)

qt_standard_project_setup(REQUIRES 6.10)

qt_add_executable(appLanChatShell WIN32
    main.cpp
    LibraryManager.cpp
    NetworkManager.cpp
    filetreemodel.cpp
    resources.qrc
)

if(WIN32)
    target_sources(appLanChatShell PRIVATE app_icon.rc)
endif()

if(ANDROID)
    set_target_properties(appLanChatShell PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_COMPILE_SDK_VERSION 35
        QT_ANDROID_TARGET_SDK_VERSION 35
        QT_ANDROID_MIN_SDK_VERSION 28
    )
endif()

qt_add_qml_module(appLanChatShell
    URI LanChatShell
    VERSION 1.0
    QML_FILES
        qml/Main.qml
        qml/components/LeftNavBar.qml
        qml/components/ConversationList.qml
        qml/components/ContactList.qml
        qml/components/FileList.qml
        qml/components/ChatPage.qml
        qml/components/ContactDetailPage.qml
        qml/components/FileDetailPage.qml
        qml/components/FilePickerDialog.qml
        qml/components/LoginPage.qml
        qml/components/IncomingOfferPopup.qml
        qml/components/TransferList.qml
        qml/components/TransferDetailPage.qml
        qml/components/MobileChatHome.qml
        qml/components/MobileDevicePage.qml
)

target_link_libraries(appLanChatShell
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Network
)

include(GNUInstallDirs)
install(TARGETS appLanChatShell
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
