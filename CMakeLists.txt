cmake_minimum_required(VERSION 3.16)
project(imgviewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(USE_LIBARCHIVE "Use libarchive for archive support (fallback to KIO zip://)" OFF)
option(USE_KIO "Use KIO for archive support" OFF)

find_package(Qt6 REQUIRED COMPONENTS Widgets)

add_executable(imgviewer
    src/main.cpp
    src/MainWindow.cpp
    src/Filter.cpp
    src/DirectoryEntry.cpp
    src/ImageView.cpp
    src/DirectoryList.cpp
    src/DirectoryListContainer.cpp
    src/TagList.cpp
    src/TagListContainer.cpp
    src/ImageDetailList.cpp
    src/ImageDetailModel.cpp

    ### Contains Q_OBJECT macros, needed for automoc
    src/include/imgviewer/DirectoryList.h
    src/include/imgviewer/DirectoryListContainer.h
    src/include/imgviewer/DirectoryEntry.h
    src/include/imgviewer/Filter.h
    src/include/imgviewer/ImageView.h
    src/include/imgviewer/ImageDetailList.h
    src/include/imgviewer/ImageDetailModel.h
    src/include/imgviewer/TagList.h
    src/include/imgviewer/TagListContainer.h
)
target_compile_definitions(imgviewer PRIVATE -DQT_DEPRECATED_WARNINGS)
target_include_directories(imgviewer PRIVATE src/include)

if(USE_LIBARCHIVE)
  find_package(LibArchive REQUIRED)
  target_compile_definitions(imgviewer PRIVATE USE_LIBARCHIVE)
  target_link_libraries(imgviewer PRIVATE Qt6::Widgets LibArchive::LibArchive)
elseif(USE_KIO)
  find_package(KF6KIO REQUIRED CONFIG)
  target_compile_definitions(imgviewer PRIVATE USE_KIO)
  target_link_libraries(imgviewer PRIVATE Qt6::Widgets KF6::KIOCore)
endif()

include(GNUInstallDirs)

install(TARGETS imgviewer RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})

install(
  FILES branding/logo-circle.png
  RENAME imgviewer.png
  DESTINATION ${CMAKE_INSTALL_DATADIR}/icons
)

install(
  FILES branding/imgviewer.desktop
  DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
)
