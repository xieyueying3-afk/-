cmake_minimum_required(VERSION 3.16)
project(CoreBackup VERSION 1.0.0 LANGUAGES CXX)

add_library(core_backup
    core/src/FileUtils.cpp
    core/src/BackupManager.cpp
    core/src/RestoreManager.cpp)
target_include_directories(core_backup PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/core/include)
target_compile_features(core_backup PUBLIC cxx_std_17)
set_target_properties(core_backup PROPERTIES CXX_EXTENSIONS OFF)
if(MSVC)
    target_compile_options(core_backup PRIVATE /W4 /utf-8)
else()
    target_compile_options(core_backup PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    add_executable(core_backup_tests tests/CoreTests.cpp)
    target_link_libraries(core_backup_tests PRIVATE core_backup)
    if(MSVC)
        target_compile_options(core_backup_tests PRIVATE /utf-8)
    endif()
    add_test(NAME core_backup_tests COMMAND core_backup_tests)
endif()
