cmake_minimum_required(VERSION 3.15)
project(citrus)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_C_COMPILER clang)
set(CMAKE_CXX_COMPILER clang++)

# https://www.jetbrains.com/help/clion/google-sanitizers.html#Configuration
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-inline -fsanitize=address -fno-omit-frame-pointer -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-inline -fno-omit-frame-pointer -g")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wswitch -pedantic -Werror -Weffc++")
# https://stackoverflow.com/questions/46782354/c-debug-window-showing-incomplete-type-for-string-variable
# set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -D_GLIBCXX_DEBUG")
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -fno-limit-debug-info")
# D_GLIBCXX_DEBUG is a bad idea. Refer to this: https://bugs.llvm.org/show_bug.cgi?id=24202

set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_DEBUG} -DNDEBUG")
#set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DNDEBUG")

include_directories(include)

find_package(LLVM REQUIRED CONFIG)
message(STATUS "Found LLVM ${LLVM_PACKAGE_VERSION}")
message(STATUS "Using LLVMConfig.cmake in: ${LLVM_DIR}")

add_definitions(${LLVM_DEFINITIONS})
include_directories(${LLVM_INCLUDE_DIRS})

add_subdirectory(src)

add_executable(citrus src/main.cpp)
target_link_libraries(citrus citrusLib)

# llvm-cxxflags
execute_process(COMMAND llvm-config --cxxflags
        OUTPUT_VARIABLE llvm-cxxflags OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(llvm-cxxflags)
target_compile_options(citrus PRIVATE "${llvm-cxxflags}")
target_compile_features(citrus PRIVATE cxx_std_14)

# llvm-ldflags
execute_process(COMMAND llvm-config --ldflags
        OUTPUT_VARIABLE llvm-ldflags OUTPUT_STRIP_TRAILING_WHITESPACE)

# llvm-libraries
execute_process(COMMAND llvm-config --libs
        OUTPUT_VARIABLE llvm-libraries OUTPUT_STRIP_TRAILING_WHITESPACE)

# llvm-syslibs
execute_process(COMMAND llvm-config --system-libs
        OUTPUT_VARIABLE llvm-syslibs OUTPUT_STRIP_TRAILING_WHITESPACE)

string(CONCAT clang-ldflags
        "-Wl,--start-group -lclangAST -lclangASTMatchers -lclangAnalysis -lclangBasic -lclangDriver"
        " -lclangEdit -lclangFrontend -lclangFrontendTool -lclangLex -lclangParse -lclangSema -lclangEdit -lclangRewrite"
        " -lclangRewriteFrontend -lclangStaticAnalyzerFrontend -lclangStaticAnalyzerCheckers -lclangStaticAnalyzerCore"
        " -lclangSerialization -lclangToolingCore -lclangTooling -lclangFormat -Wl,--end-group")

set(HACK_LD_FLAGS "${clang-ldflags} ${llvm-ldflags} ${llvm-libraries} ${llvm-syslibs}")
set(CMAKE_CXX_LINK_EXECUTABLE "${CMAKE_CXX_LINK_EXECUTABLE} ${HACK_LD_FLAGS} -lstdc++fs")

#add_executable(func-complexity src/func/main.cpp src/func/action.cpp src/func/api.cpp)
#target_compile_options(func-complexity PRIVATE "${llvm-cxxflags}")
#target_compile_features(func-complexity PRIVATE cxx_std_14)
