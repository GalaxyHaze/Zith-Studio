cmake_minimum_required(VERSION 4.1)
project(Zith_Studio)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(Zith_Studio
    main.cpp
    editor/MainWindow.cpp
    editor/Code.cpp
    editor/Syntax.cpp
    editor/LspClient.cpp
    editor/LspCompletionModel.cpp
    editor/DiagnosticsPanel.cpp
    editor/ActivityBar.cpp
    editor/BreadcrumbsBar.cpp
    editor/FileTreePanel.cpp
    editor/ContextManager.cpp
    editor/CompilerPanel.cpp
    editor/SnippetManager.cpp
    editor/FindReplaceBar.cpp
)

target_link_libraries(Zith_Studio PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)
