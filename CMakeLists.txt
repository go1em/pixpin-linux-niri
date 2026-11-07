cmake_minimum_required(VERSION 3.16)
project(pixpin VERSION 1.0.7 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets DBus Svg Network)
find_package(PkgConfig REQUIRED)
pkg_check_modules(TESS REQUIRED tesseract lept)

include_directories(
    src
    ${TESS_INCLUDE_DIRS}
)

set(SOURCES
    src/main.cpp
    src/app/Application.cpp
    src/app/SystemTray.cpp
    src/capture/ScreenCapturer.cpp
    src/capture/CaptureOverlay.cpp
    src/capture/MagnifierWidget.cpp
    src/ui/ToolBarWidget.cpp
    src/ui/AnnotationLayer.cpp
    src/ui/PinWindow.cpp
    src/ocr/OcrEngine.cpp
    src/ocr/OcrResultDialog.cpp
    src/utils/ImageUtils.cpp
)

set(HEADERS
    src/app/Application.h
    src/app/SystemTray.h
    src/capture/ScreenCapturer.h
    src/capture/CaptureOverlay.h
    src/capture/MagnifierWidget.h
    src/ui/ToolBarWidget.h
    src/ui/AnnotationLayer.h
    src/ui/PinWindow.h
    src/ocr/OcrEngine.h
    src/ocr/OcrResultDialog.h
    src/utils/ImageUtils.h
)

add_executable(pixpin ${SOURCES} ${HEADERS})

target_link_libraries(pixpin PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    Qt6::DBus
    Qt6::Svg
    Qt6::Network
    ${TESS_LIBRARIES}
)

target_compile_definitions(pixpin PRIVATE
    PIXPIN_VERSION="${PROJECT_VERSION}"
)

# Installation targets
include(GNUInstallDirs)

install(TARGETS pixpin
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

install(FILES pixpin.desktop
    DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)

install(FILES pixpin.svg
    DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps
)

install(FILES src/ocr/ocr_worker.py
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pixpin
    PERMISSIONS OWNER_READ OWNER_WRITE OWNER_EXECUTE GROUP_READ GROUP_EXECUTE WORLD_READ WORLD_EXECUTE
)

install(DIRECTORY resources/tessdata/
    DESTINATION ${CMAKE_INSTALL_DATADIR}/pixpin/tessdata
)

# CPack configuration
set(CPACK_PACKAGE_NAME "pixpin")
set(CPACK_PACKAGE_VENDOR "realsky")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Smart Screenshot, Pin and Offline OCR tool for Linux")
set(CPACK_PACKAGE_VERSION "${PROJECT_VERSION}")
set(CPACK_PACKAGE_CONTACT "realsky")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")

# Debian packaging
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "realsky")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libc6, libqt6core6, libqt6gui6, libqt6widgets6, libqt6dbus6, libqt6svg6, libqt6network6, libtesseract5, liblept5")
set(CPACK_DEBIAN_PACKAGE_SUGGESTS "python3, python3-pip")
set(CPACK_DEBIAN_PACKAGE_CONTROL_EXTRA
    "${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postinst;${CMAKE_CURRENT_SOURCE_DIR}/packaging/debian/postrm"
)

# RPM packaging
set(CPACK_RPM_PACKAGE_GROUP "Applications/Graphics")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")

include(CPack)

