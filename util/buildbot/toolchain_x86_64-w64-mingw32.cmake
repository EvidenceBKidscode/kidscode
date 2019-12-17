# name of the target operating system
SET(CMAKE_SYSTEM_NAME Windows)

# which compilers to use for C and C++
SET(CMAKE_COMPILER_PREFIX x86_64-w64-mingw32-)
SET(CMAKE_C_COMPILER ${CMAKE_COMPILER_PREFIX}gcc)
SET(CMAKE_CXX_COMPILER ${CMAKE_COMPILER_PREFIX}g++)
SET(CMAKE_RC_COMPILER ${CMAKE_COMPILER_PREFIX}windres)

# here is the target environment located
SET(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)

# adjust the default behaviour of the FIND_XXX() commands:
# search headers and libraries in the target environment, search
# programs in the host environment
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
