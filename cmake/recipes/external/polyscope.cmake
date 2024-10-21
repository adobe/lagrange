if(TARGET polyscope::polyscope)
    return()
endif()

message(STATUS "Third-party (external): creating target 'polyscope::polyscope'")

if(NOT TARGET stb)
    include(stb)
    add_library(stb INTERFACE)
    target_link_libraries(stb INTERFACE stb::image stb::image_write)
endif()
include(glfw)
include(imgui)
include(nlohmann_json)
include(glad)

include(CPM)
CPMAddPackage(
    NAME polyscope
    GITHUB_REPOSITORY nmwsharp/polyscope
    GIT_TAG bb034326842313151c9419e51fac20c6c4ef8a93
)

add_library(polyscope::polyscope ALIAS polyscope)
