# I didn’t know what to set the minimum required version to, so I decided to
# use the same minimum version that Descent 3 uses:
# <https://github.com/DescentDevelopers/Descent3/blob/ed385adb5965e50200efb515c0eb3463ab60a763/CMakeLists.txt#L1>
cmake_minimum_required(VERSION 3.20)
project("SDL Logging Example")
add_executable(sdl-log-example main.c)
