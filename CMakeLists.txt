cmake_minimum_required(VERSION 3.21)

project(TaskManager VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)

qt_add_executable(TaskManager WIN32
    src/main.cpp
    src/mainwindow.cpp
    src/mainwindow.h
    src/task.cpp
    src/task.h
    src/task.cpp
    src/taskstore.h
)

target_link_libraries(TaskManager PRIVATE Qt6::Widgets)
target_include_directories(TaskManager PRIVATE src)

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Test)
    qt_add_executable(TaskManagerTests
        tests/task_tests.cpp
        src/mainwindow.cpp
        src/mainwindow.h
        src/task.cpp
        src/task.h
        src/task.cpp
        src/taskstore.h
    )
    target_link_libraries(TaskManagerTests PRIVATE Qt6::Test Qt6::Widgets)
    target_include_directories(TaskManagerTests PRIVATE src)
    add_test(NAME TaskManagerTests COMMAND TaskManagerTests)
endif()
