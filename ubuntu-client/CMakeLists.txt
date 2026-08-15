find_package(Qt6 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Multimedia)

qt_standard_project_setup(REQUIRES 6.4)

set(APP_SOURCES
  src/main.cpp
  src/controller/appcontroller.cpp
  src/controller/filelistmodel.cpp
  src/controller/devicelistmodel.cpp
)

qt_add_executable(localsend-ubuntu ${APP_SOURCES})

qt_add_qml_module(localsend-ubuntu
  URI LocalSend
  VERSION 1.0
  QML_FILES
    qml/Main.qml
    qml/CollapsedBar.qml
    qml/DeckBar.qml
    qml/ExpandedPanel.qml
    qml/FileListItem.qml
    qml/ImagePreview.qml
    qml/VideoPreview.qml
    qml/TextPreview.qml
    qml/SettingsDialog.qml
    qml/DeviceManagerDialog.qml
    qml/PairConfirmDialog.qml
    qml/Toast.qml
)

qt_add_resources(localsend-ubuntu "icons"
    PREFIX "/"
    FILES icons/portal.png
)

target_link_libraries(localsend-ubuntu PRIVATE
  Qt6::Core
  Qt6::Gui
  Qt6::Qml
  Qt6::Quick
  Qt6::QuickControls2
  Qt6::Multimedia
  localsend-core
)
target_include_directories(localsend-ubuntu PRIVATE src src/controller)
