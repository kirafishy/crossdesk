cmake_minimum_required(VERSION 3.16)
project(crossdesk_harmony LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
get_filename_component(HARMONY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../.." ABSOLUTE)
set(CORE_LIBRARY "${HARMONY_ROOT}/entry/libs/${OHOS_ARCH}/libcrossdesk_harmony_core.so")
if(NOT EXISTS "${CORE_LIBRARY}")
  message(FATAL_ERROR "Build MiniRTC first: python3 apps/harmony/scripts/build.py --mobile --native-only")
endif()
add_library(crossdesk_core SHARED IMPORTED)
set_target_properties(crossdesk_core PROPERTIES
  IMPORTED_LOCATION "${CORE_LIBRARY}"
  IMPORTED_SONAME "libcrossdesk_harmony_core.so")
add_library(crossdesk SHARED napi_init.cpp)
# HAP libraries are resolved by the app's linker namespace. The host build
# directory is not a useful runtime search path on a HarmonyOS device.
set_target_properties(crossdesk PROPERTIES SKIP_BUILD_RPATH TRUE)
target_include_directories(crossdesk PRIVATE "${HARMONY_ROOT}/native")
target_link_libraries(crossdesk PRIVATE crossdesk_core libace_napi.z.so)
