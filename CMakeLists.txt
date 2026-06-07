cmake_minimum_required(VERSION 3.16)
project(LCFT VERSION 0.1 LANGUAGES CXX)
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

qt_add_executable(LCFT WIN32
    main.cpp
    LibraryManager.cpp
    NetworkManager.cpp
    filetreemodel.cpp
    resources.qrc
)

if(WIN32)
    target_sources(LCFT PRIVATE app_icon.rc)
endif()

if(ANDROID)
    if(DEFINED QT_ANDROID_ABIS AND NOT QT_ANDROID_ABIS STREQUAL "")
        set(LCFT_ANDROID_ABIS "${QT_ANDROID_ABIS}")
    elseif(DEFINED ANDROID_ABI AND NOT ANDROID_ABI STREQUAL "")
        set(LCFT_ANDROID_ABIS "${ANDROID_ABI}")
    endif()

    set_target_properties(LCFT PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android"
        QT_ANDROID_COMPILE_SDK_VERSION 35
        QT_ANDROID_TARGET_SDK_VERSION 35
        QT_ANDROID_MIN_SDK_VERSION 28
    )
    if(LCFT_ANDROID_ABIS)
        set_target_properties(LCFT PROPERTIES QT_ANDROID_ABIS "${LCFT_ANDROID_ABIS}")
    endif()
endif()

qt_add_qml_module(LCFT
    URI LCFT
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
        qml/components/FolderSelectPage.qml
        qml/components/FilePickerDialog.qml
        qml/components/LoginPage.qml
        qml/components/IncomingOfferPopup.qml
        qml/components/TransferList.qml
        qml/components/TransferDetailPage.qml
        qml/components/MobileChatHome.qml
        qml/components/MobileDevicePage.qml
)

target_link_libraries(LCFT
    PRIVATE
        Qt6::Quick
        Qt6::QuickControls2
        Qt6::QuickDialogs2
        Qt6::Network
)

include(GNUInstallDirs)
install(TARGETS LCFT
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
