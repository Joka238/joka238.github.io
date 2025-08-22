cmake_minimum_required(VERSION 3.15)
project(telepaint_clean_anon)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED sdl2)
pkg_check_modules(JSONC REQUIRED json-c)
find_package(OpenSSL REQUIRED)

add_executable(telepaint
  src/main.cpp
  src/github_client.cpp
  src/crypto.cpp
)

target_include_directories(telepaint PRIVATE
  src
  ${SDL2_INCLUDE_DIRS}
  ${JSONC_INCLUDE_DIRS}
)

# Ensure linker sees library directories (fixes 'ld: library SDL2 not found' on macOS)
if (SDL2_LIBRARY_DIRS)
  target_link_directories(telepaint PRIVATE ${SDL2_LIBRARY_DIRS})
endif()
if (JSONC_LIBRARY_DIRS)
  target_link_directories(telepaint PRIVATE ${JSONC_LIBRARY_DIRS})
endif()

target_link_libraries(telepaint PRIVATE
  ${SDL2_LIBRARIES}
  ${JSONC_LIBRARIES}
  OpenSSL::SSL
  OpenSSL::Crypto
  curl
)

# Keep additional cflags if any
add_definitions(${SDL2_CFLAGS_OTHER})
