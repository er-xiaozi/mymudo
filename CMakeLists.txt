cmake_minimum_required(VERSION 3.0)
project(mymuduo)

# 设置动态库输出路径
set(LIBRARY_OUTPUT_PATH ${PROJECT_SOURCE_DIR}/lib)

# 设置可执行文件输出路径
#set(EXECUTABLE_OUTPUT_PATH ${PROJECT_BINARY_DIR}/bin)

# 设置调试信息以及C++11标准
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11")

# 参与编译的源代码文件(用于mymuduo库)
aux_source_directory(. MYMUDUO_SRC_LIST)

# 编译生成动态库mymuduo
add_library(mymuduo SHARED ${MYMUDUO_SRC_LIST})



# 如果testserver.cc在当前目录下，可以这样写：
#add_executable(testserver testserver.cc)
#target_link_libraries(testserver mymuduo pthread)