file(GLOB SOURCE_FILES *.cpp)
file(GLOB HEADER_FILES *.hpp)
file(GLOB EXTENSION_FILES func/api.cpp)

add_library(citrusLib SHARED ${SOURCE_FILES} ${HEADER_FILES} ${EXTENSION_FILES})

execute_process(COMMAND llvm-config --cxxflags
        OUTPUT_VARIABLE llvm-cxxflags OUTPUT_STRIP_TRAILING_WHITESPACE)
separate_arguments(llvm-cxxflags)
target_compile_options(citrusLib PRIVATE "${llvm-cxxflags}")
target_compile_features(citrusLib PRIVATE cxx_std_14)
