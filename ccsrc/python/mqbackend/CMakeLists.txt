# ==============================================================================
#
# Copyright 2022 <Huawei Technologies Co., Ltd>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# ==============================================================================

# lint_cmake: -whitespace/indent

add_library(mqbackend_python_interface INTERFACE)
target_include_directories(mqbackend_python_interface INTERFACE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>
                                                                $<INSTALL_INTERFACE:${MQ_INSTALL_INCLUDEDIR}>)
append_to_property(mq_install_targets GLOBAL mqbackend_python_interface)

# ------------------------------------------------------------------------------

install(DIRECTORY ${CMAKE_CURRENT_LIST_DIR}/include/python DESTINATION ${MQ_INSTALL_INCLUDEDIR})

# ==============================================================================

pybind11_add_module(mqbackend MODULE OUTPUT_HINT "${MQ_PYTHON_PACKAGE_NAME}" ${CMAKE_CURRENT_LIST_DIR}/lib/binding.cc
                                                                             ${CMAKE_CURRENT_LIST_DIR}/lib/logging.cpp)

target_compile_definitions(mqbackend PUBLIC $<$<BOOL:${ENABLE_PROJECTQ}>:ENABLE_PROJECTQ>)
target_include_directories(mqbackend PRIVATE $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}/include>)

force_at_least_cxx17_workaround(mqbackend)
target_link_libraries(mqbackend PUBLIC ${MQ_OPENMP_TARGET} "$<$<BOOL:${ENABLE_PROJECTQ}>:mq_projectq>" mq_base
                                       mq_python_core mqbackend_python_interface)

get_target_property(_mqbackend_output_dir mqbackend LIBRARY_OUTPUT_DIRECTORY)
if(NOT _mqbackend_output_dir)
  set(_mqbackend_output_dir "${CMAKE_LIBRARY_OUTPUT_DIRECTORY}")
endif()

if(WIN32)
  get_filename_component(CXX_DIR ${CMAKE_CXX_COMPILER} PATH)
  file(
    GLOB
    LIB_LIST
    ${CXX_DIR}/libstdc++-6.dll
    ${CXX_DIR}/libwinpthread-1.dll
    ${CXX_DIR}/libssp-0.dll
    ${CXX_DIR}/libgcc_s_*-1.dll
    ${CXX_DIR}/libgomp-1.dll)
  foreach(win_dep_lib ${LIB_LIST})
    if(EXISTS "${win_dep_lib}")
      message("COPYING ${win_dep_lib}")
      file(COPY ${win_dep_lib} DESTINATION ${_mqbackend_output_dir} FOLLOW_SYMLINK_CHAIN)
    endif()
  endforeach()
endif()
