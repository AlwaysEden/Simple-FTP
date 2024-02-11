include_directories("${PROJECT_SOURCE_DIR}/include")
include_directories("${PROJECT_SOURCE_DIR}/googletest/googletest/include")
include_directories("${PROJECT_SOURCE_DIR}/googletest/googlemock/include")
file(GLOB sources "${PROJECT_SOURCE_DIR}/src/fshare.c")
#list(REMOVE_ITEM sources "${PROJECT_SOURCE_DIR}/src/main.c")

file(GLOB tests "${PROJECT_SOURCE_DIR}/test/fshare.cpp")
#list(REMOVE_ITEM tests "${PROJECT_SOURCE_DIR}/test/main.cpp")

foreach(file ${tests})
  set(name)
  get_filename_component(name ${file} NAME_WE)
  add_executable("${name}_tests"
    	${sources}
    	${file}
	"${PROJECT_SOURCE_DIR}/test/main.cpp")
  target_compile_options(${name}_tests PRIVATE -g -ftest-coverage -fprofile-arcs)
  target_compile_definitions(${name}_tests PRIVATE UNITTEST)
  target_link_libraries("${name}_tests" gmock  gtest_main gcov)
  add_test(NAME ${name} COMMAND "${name}_tests")
endforeach()
