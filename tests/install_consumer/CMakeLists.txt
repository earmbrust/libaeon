cmake_minimum_required(VERSION 3.24)
project(libaeon_installed_consumer LANGUAGES CXX)

find_package(libaeon 2.0 CONFIG REQUIRED)

if(NOT TARGET aeon::aeon OR NOT TARGET aeon::aeon-static)
    message(FATAL_ERROR "Installed libAeon targets are missing")
endif()

add_executable(installed_shared ../consumer_interface.cpp)
target_link_libraries(installed_shared PRIVATE aeon::aeon)
set_target_properties(installed_shared PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

add_executable(installed_static ../consumer_interface.cpp)
target_link_libraries(installed_static PRIVATE aeon::aeon-static)
set_target_properties(installed_static PROPERTIES
    CXX_STANDARD 20 CXX_STANDARD_REQUIRED ON CXX_EXTENSIONS OFF)

enable_testing()
add_test(NAME InstalledSharedConsumer COMMAND installed_shared)
add_test(NAME InstalledStaticConsumer COMMAND installed_static)
