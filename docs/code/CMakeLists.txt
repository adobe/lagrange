# Find console script for code extraction
find_program(CODE_EXTRACT_PATH NAMES code-extract)

if(NOT CODE_EXTRACT_PATH)
    message(WARNING "Could not find code-extract script. Please make sure it is installed via conda/pip.")
    return()
endif()

message(STATUS "Code extract: ${CODE_EXTRACT_PATH}")

# Custom target for code extraction
set(extracted_code_dir "${CMAKE_CURRENT_BINARY_DIR}/extracted_code")
file(MAKE_DIRECTORY "${extracted_code_dir}")

add_custom_target(lagrange_extract_code_script
    COMMAND ${CODE_EXTRACT_PATH}
        ${LAGRANGE_WEBSITE_REPO}
        ${extracted_code_dir}
)
set_target_properties(lagrange_extract_code_script PROPERTIES FOLDER "${LAGRANGE_IDE_PREFIX}Lagrange/Utils")

# Additionally execute this process at CMake time so we can capture the list of extracted files (very hacky approach)
execute_process(COMMAND ${CODE_EXTRACT_PATH} ${LAGRANGE_WEBSITE_REPO} ${extracted_code_dir})

# New target to compile extracted code
file(GLOB_RECURSE SRC_FILES "${extracted_code_dir}/*.cpp")
source_group(TREE "${extracted_code_dir}/" PREFIX "Source Files" FILES ${SRC_FILES})
add_library(lagrange_extracted_code ${SRC_FILES})

# Dependencies. For now we only enable the core module. But once we cover more files on the website
# we will have to include additional modules.
lagrange_include_modules(core)
# lagrange_include_modules(autouv core primitive raycasting ui)
target_link_libraries(lagrange_extracted_code PRIVATE
    lagrange::core
#     lagrange::autouv
#     lagrange::primitive
#     lagrange::raycasting
#     lagrange::ui
)
add_dependencies(lagrange_extracted_code lagrange_extract_code_script)
