include(FetchContent)
FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.14.0
    GIT_SHALLOW TRUE
)

set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(NetEngineTests
    test_Transport.cpp
    test_AckManager.cpp
    test_BitWriter.cpp
    test_Snapshot.cpp
    test_Crypto.cpp
    test_Rpc.cpp
    test_fps.cpp
    test_rts.cpp
    test_mmo.cpp
)

target_link_libraries(NetEngineTests
    PRIVATE
    Aegis_Core
    gtest
    gtest_main
)

target_include_directories(NetEngineTests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/..
    ${CMAKE_CURRENT_SOURCE_DIR}/../Transport
    ${CMAKE_CURRENT_SOURCE_DIR}/../Serializer
    ${CMAKE_CURRENT_SOURCE_DIR}/../Replication
    ${CMAKE_CURRENT_SOURCE_DIR}/../Packets
    ${CMAKE_CURRENT_SOURCE_DIR}/../Genres
    ${CMAKE_CURRENT_SOURCE_DIR}/../Genres/FPS
    ${CMAKE_CURRENT_SOURCE_DIR}/../Genres/RTS
    ${CMAKE_CURRENT_SOURCE_DIR}/../Genres/MMO
)

if(NETENGINE_USE_OPENSSL)
    target_compile_definitions(NetEngineTests PRIVATE NETENGINE_USE_OPENSSL)
endif()

if(NETENGINE_ENABLE_LOGGING)
    target_compile_definitions(NetEngineTests PRIVATE NETENGINE_LOGGING)
endif()

include(GoogleTest)
gtest_discover_tests(NetEngineTests)