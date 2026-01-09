# UpdateGitVersion.cmake - Run at build time to capture current git hash

# Get the current git hash
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY "${GIT_WORKING_DIR}"
    OUTPUT_VARIABLE GIT_HASH
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)

# Fallback if git fails
if(NOT GIT_HASH)
    set(GIT_HASH "unknown")
endif()

# Only update the file if the hash changed (avoids unnecessary rebuilds)
if(EXISTS "${OUTPUT_FILE}")
    file(READ "${OUTPUT_FILE}" EXISTING_CONTENT)
    string(REGEX MATCH "\"([^\"]+)\"" _ "${EXISTING_CONTENT}")
    set(EXISTING_HASH "${CMAKE_MATCH_1}")
    if("${EXISTING_HASH}" STREQUAL "${GIT_HASH}")
        message(STATUS "Git hash unchanged: ${GIT_HASH}")
        return()
    endif()
endif()

# Configure the header file with the new hash
configure_file("${INPUT_FILE}" "${OUTPUT_FILE}" @ONLY)
message(STATUS "Updated git hash: ${GIT_HASH}")
