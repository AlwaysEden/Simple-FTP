cmake_minimum_required(VERSION 3.1)

project(fshare_ver.1)

#configure_file(CMakeLists.txt.in googletest-download/CMakeLists.txt)
#execute_process(COMMAND ${CMAKE_COMMAND} -G "${CMAKE_GENERATOR}" . WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
#execute_process(COMMAND ${CMAKE_COMMAND} --build . WORKING_DIRECTORY ${CMAKE_BINARY_DIR}/googletest-download )
#add_subdirectory(${CMAKE_BINARY_DIR}/googletest-src ${CMAKE_BINARY_DIR}/googletest-build)

set(gtest_SRC_DIR ${PROJECT_SOURCE_DIR}/googletest)
add_subdirectory(${gtest_SRC_DIR})

enable_testing()
add_subdirectory(test)

include_directories("${PROJECT_SOURCE_DIR}/include")

file(GLOB fshare "${PROJECT_SOURCE_DIR}/include/fshare.h" "${PROJECT_SOURCE_DIR}/src/fshare.c")

file(GLOB fshared "${PROJECT_SOURCE_DIR}/include/fshared.h" "${PROJECT_SOURCE_DIR}/src/fshared.c")

add_executable(fshare ${fshare})
target_compile_options(fshare PRIVATE -ftest-coverage -fprofile-arcs -g)
target_link_libraries(fshare gcov)

add_executable(fshared ${fshared})
target_compile_options(fshared PRIVATE -g -pthread)
target_link_libraries(fshared pthread)

