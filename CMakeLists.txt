cmake_minimum_required(VERSION 3.2.2)
project(bemorehuman)

set(USE_LANGUAGES
    c99)

add_subdirectory(lib)
add_subdirectory(ratgen)
add_subdirectory(valgen)
add_subdirectory(recgen)
add_subdirectory(test-accuracy)

