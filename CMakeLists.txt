cmake_minimum_required(VERSION 3.18)
project(RoguelikeGame LANGUAGES CXX)

find_package(SFML 2.5.1 EXACT CONFIG REQUIRED COMPONENTS graphics window system audio)
add_executable(RoguelikeGame GameMain.cpp World.cpp)
target_compile_features(RoguelikeGame PRIVATE cxx_std_17)
target_link_libraries(RoguelikeGame PRIVATE sfml-graphics sfml-window sfml-system sfml-audio)
if(MSVC)
    target_compile_definitions(RoguelikeGame PRIVATE _CRT_SECURE_NO_WARNINGS)
    target_compile_options(RoguelikeGame PRIVATE /W4)
endif()

if(WIN32)
    add_custom_command(TARGET RoguelikeGame POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            $<TARGET_FILE:sfml-graphics>
            $<TARGET_FILE:sfml-window>
            $<TARGET_FILE:sfml-system>
            $<TARGET_FILE:sfml-audio>
            "$<TARGET_FILE_DIR:sfml-audio>/openal32.dll"
            $<TARGET_FILE_DIR:RoguelikeGame>)
endif()

enable_testing()
add_test(NAME DungeonRules COMMAND RoguelikeGame --self-test)
