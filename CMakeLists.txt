cmake_minimum_required(VERSION 2.6) # Pour éviter un warning au lancement de CMake, on doit préciser la version minimale

#set(CMAKE_CXX_FLAGS "-O3 -fno-rtti -fno-exceptions") # maximum speed
#set(CMAKE_CXX_FLAGS "-g -pg -O3 -lc") # profiling
#set(CMAKE_CXX_FLAGS "-fprofile-generate") # profile-guided optimization
#set(CMAKE_CXX_FLAGS "-O3 -fprofile-use") # use p-g optimisation

# Nom du projet
project(indexd)

file(
    GLOB_RECURSE
    source_files
    src/*
)


# Déclaration de l'exécutable
add_executable(
    indexd
    ${source_files}
)

find_package (Threads)
target_link_libraries (indexd ${CMAKE_THREAD_LIBS_INIT})

#find_package( Boost COMPONENTS system thread filesystem REQUIRED)

#target_link_libraries(indexd pqxx)
