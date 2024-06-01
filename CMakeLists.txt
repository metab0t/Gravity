cmake_minimum_required(VERSION 3.16)

project(Gravity)

#use /utf-8 and /bigobj for MSVC
# define WIN32_LEAN_AND_MEAN to exclude some APIs from windows.h
if(MSVC)
  add_compile_options(/utf-8 /bigobj /MP)
  add_definitions(-DWIN32_LEAN_AND_MEAN)
endif()

add_definitions(-DUSE_IPOPT)

#use c++latest standard
set(CMAKE_CXX_STANDARD 20)

set(IPOPT_INCLUDE_DIR D:/Ipopt/include/coin-or)
set(IPOPT_LIB_DIR D:/Ipopt/lib)
set(IPOPT_LIB ipopt.dll.lib)

add_library(
    gravity STATIC
)

target_sources(gravity PRIVATE
src/Arc.cpp
src/constant
src/constraint.cpp
src/expr.cpp
src/func.cpp
src/model.cpp
src/Net.cpp
src/Node.cpp
src/param.cpp
src/Path.cpp
src/poly.cpp
src/solver.cpp
src/utils.cpp
src/var.cpp
)

target_include_directories(gravity PUBLIC
  include
  ${IPOPT_INCLUDE_DIR}
)
target_link_directories(gravity PUBLIC
  ${IPOPT_LIB_DIR}
)
target_link_libraries(gravity PUBLIC
  ${IPOPT_LIB}
)

add_library(gravitypower STATIC)
target_sources(gravitypower PRIVATE
examples/Optimization/NonLinear/Power/Bound.cpp
examples/Optimization/NonLinear/Power/Bus.cpp
examples/Optimization/NonLinear/Power/Conductor.cpp
examples/Optimization/NonLinear/Power/Gen.cpp
examples/Optimization/NonLinear/Power/Line.cpp
examples/Optimization/NonLinear/Power/PowerNet.cpp
)

target_include_directories(gravitypower PUBLIC
examples/Optimization/NonLinear/Power
)

target_link_libraries(gravitypower gravity)

add_executable(
  acopf
  examples/Optimization/NonLinear/Power/ACOPF/ACOPF_main.cpp
)

target_link_libraries(acopf gravitypower)
