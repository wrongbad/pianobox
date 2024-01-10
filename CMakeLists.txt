cmake_minimum_required(VERSION 3.26)
project(pianobox)

set(CMAKE_CXX_STANDARD 17)
set(BUILD_SHARED_LIBS FALSE)

set(PA_USE_JACK OFF)

# add_subdirectory(portaudio)
# add_subdirectory(portmidi)
add_subdirectory(audioplus)

add_executable(pianobox pianobox.cpp)
target_link_libraries(pianobox audioplus_wav audioplus_audio audioplus_midi)