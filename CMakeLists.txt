cmake_minimum_required(VERSION 3.15)
project(audio_converter_notcurses CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 1) Headers C++ ficam em /usr/include/ncpp na sua distro
find_path(NOTCURSES_NCPP_INCLUDE_DIR
  NAMES ncpp/NotCurses.hh
  PATHS /usr/include /usr/local/include
  REQUIRED)
message(STATUS "Found ncpp headers at: ${NOTCURSES_NCPP_INCLUDE_DIR}")

# 2) Bibliotecas (Ubuntu: /usr/lib/x86_64-linux-gnu)
find_library(NOTCURSESXX_LIBRARY
  NAMES notcurses++
  PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
  REQUIRED)
find_library(NOTCURSES_LIBRARY
  NAMES notcurses
  PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
  REQUIRED)
find_library(NOTCURSES_CORE_LIBRARY
  NAMES notcurses-core
  PATHS /usr/lib /usr/lib/x86_64-linux-gnu /usr/local/lib
  REQUIRED)

message(STATUS "Linking with: ${NOTCURSESXX_LIBRARY}; ${NOTCURSES_LIBRARY}; ${NOTCURSES_CORE_LIBRARY}")
set(NOTCURSES_LIBS
  ${NOTCURSESXX_LIBRARY}
  ${NOTCURSES_LIBRARY}
  ${NOTCURSES_CORE_LIBRARY}
)

add_executable(audio_converter_tui
  src/tui/main.cpp
  src/tui/BaseScreen.cpp
  src/tui/WelcomeScreen.cpp
  src/tui/TestScreen.cpp
  src/tui/FileBrowser.cpp
  src/tui/Subframe.cpp
  src/tui/StateMachine.cpp
  src/tui/Signal.cpp
)
target_include_directories(audio_converter_tui PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${NOTCURSES_NCPP_INCLUDE_DIR}
)
target_link_libraries(audio_converter_tui PRIVATE ${NOTCURSES_LIBS})

add_executable(nc_hello
  src/tui/nc_hello.cpp
)
target_include_directories(nc_hello PRIVATE
  ${PROJECT_SOURCE_DIR}/include
  ${NOTCURSES_NCPP_INCLUDE_DIR}
)
target_link_libraries(nc_hello PRIVATE ${NOTCURSES_LIBS})

add_library(audio_converter_core STATIC
  src/converter/AudioConverter.cpp
  src/converter/MP3ToOpusConverter.cpp
)
target_include_directories(audio_converter_core PUBLIC
  ${PROJECT_SOURCE_DIR}/include
)

# (opcional) se o seu toolchain exigir -pthread:
# set(THREADS_PREFER_PTHREAD_FLAG ON)
# find_package(Threads REQUIRED)
# target_link_libraries(nc_hello PRIVATE Threads::Threads)
