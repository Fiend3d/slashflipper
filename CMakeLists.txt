cmake_minimum_required(VERSION 3.15)
project(slashflipper)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find SDL3
find_package(SDL3 REQUIRED)

# Include directories
include_directories(
    ${SDL3_INCLUDE_DIRS}
    .
)

set(SOURCES
    main.cpp
)

enable_language(RC)

add_executable(${PROJECT_NAME} ${SOURCES} resources/resources.rc)

set_target_properties(${PROJECT_NAME} PROPERTIES WIN32_EXECUTABLE TRUE)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
    SDL3::SDL3-static
)

configure_file(${CMAKE_CURRENT_SOURCE_DIR}/app.manifest 
               ${CMAKE_CURRENT_BINARY_DIR}/app.manifest COPYONLY)

add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND "mt.exe" -manifest \"${CMAKE_CURRENT_SOURCE_DIR}/app.manifest\" 
                     -outputresource:"$<TARGET_FILE:slashflipper>";#1
)
