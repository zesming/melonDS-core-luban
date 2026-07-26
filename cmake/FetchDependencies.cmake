set(MELONDSDS_BUNDLED_DEPENDENCIES_DIR "${PROJECT_SOURCE_DIR}/third_party" CACHE PATH
    "Directory containing bundled dependency source trees.")

set(MELONDSDS_REQUIRED_DEPENDENCIES
    melonDS libretro-common embed-binaries glm pntr fmt yamc span-lite date)
if (ENABLE_ZLIB)
    list(APPEND MELONDSDS_REQUIRED_DEPENDENCIES zlib)
endif ()
if (ENABLE_NETWORKING)
    list(APPEND MELONDSDS_REQUIRED_DEPENDENCIES libslirp)
endif ()

set(DEFAULT_USE_BUNDLED_DEPENDENCIES ON)
foreach (dependency IN LISTS MELONDSDS_REQUIRED_DEPENDENCIES)
    if (NOT IS_DIRECTORY "${MELONDSDS_BUNDLED_DEPENDENCIES_DIR}/${dependency}")
        set(DEFAULT_USE_BUNDLED_DEPENDENCIES OFF)
        break()
    endif ()
endforeach ()
option(MELONDSDS_USE_BUNDLED_DEPENDENCIES
    "Use dependency sources from MELONDSDS_BUNDLED_DEPENDENCIES_DIR." ${DEFAULT_USE_BUNDLED_DEPENDENCIES})

if (MELONDSDS_USE_BUNDLED_DEPENDENCIES)
    foreach (dependency IN LISTS MELONDSDS_REQUIRED_DEPENDENCIES)
        set(dependency_source "${MELONDSDS_BUNDLED_DEPENDENCIES_DIR}/${dependency}")
        if (NOT IS_DIRECTORY "${dependency_source}")
            message(FATAL_ERROR "Bundled dependency '${dependency}' was not found at ${dependency_source}")
        endif ()

        string(TOUPPER "${dependency}" dependency_name)
        set(source_variable "FETCHCONTENT_SOURCE_DIR_${dependency_name}")
        if (NOT DEFINED ${source_variable} OR "${${source_variable}}" STREQUAL "")
            set("${source_variable}" "${dependency_source}" CACHE PATH
                "Bundled source for ${dependency}" FORCE)
        endif ()
    endforeach ()
endif ()

macro(define_git_dependency_vars name default_url default_tag)
    string(TOUPPER ${name} VAR_NAME)
    string(MAKE_C_IDENTIFIER ${VAR_NAME} VAR_NAME)

    if (NOT ${VAR_NAME}_REPOSITORY_URL)
        set(
                "${VAR_NAME}_REPOSITORY_URL"
                "${default_url}"
                CACHE STRING
                "${name} repository URL. Set this to use a fork."
                FORCE
        )
    endif ()

    if (NOT ${VAR_NAME}_REPOSITORY_TAG)
        set(
                "${VAR_NAME}_REPOSITORY_TAG"
                "${default_tag}"
                CACHE STRING
                "${name} repository commit hash or tag. Set this when using a new version or a custom branch."
                FORCE
        )
    endif ()
endmacro()

function(fetch_dependency name default_url default_tag)
    define_git_dependency_vars(${name} ${default_url} ${default_tag})

    string(TOUPPER "${name}" FETCHCONTENT_NAME)
    set(FETCHCONTENT_SOURCE_VARIABLE "FETCHCONTENT_SOURCE_DIR_${FETCHCONTENT_NAME}")
    if (DEFINED ${FETCHCONTENT_SOURCE_VARIABLE} AND NOT "${${FETCHCONTENT_SOURCE_VARIABLE}}" STREQUAL "")
        message(STATUS "Using ${name}: ${${FETCHCONTENT_SOURCE_VARIABLE}} (local)")
    else()
        message(STATUS "Using ${name}: ${${VAR_NAME}_REPOSITORY_URL} (ref ${${VAR_NAME}_REPOSITORY_TAG})")
    endif()

    FetchContent_Declare(
        ${name}
        GIT_REPOSITORY "${${VAR_NAME}_REPOSITORY_URL}"
        GIT_TAG "${${VAR_NAME}_REPOSITORY_TAG}"
    )

    FetchContent_GetProperties(${name})
endfunction()

fetch_dependency(melonDS "https://github.com/zesming/melonDS-core-luban.git" "265cf6bd871529912167fd7b57ff6105b37cbd53")
fetch_dependency(libretro-common "https://github.com/JesseTG/libretro-common" "8e2b884")
fetch_dependency("embed-binaries" "https://github.com/andoalon/embed-binaries" "078b62b")
fetch_dependency(glm "https://github.com/g-truc/glm" "e7970a8")
if (ENABLE_NETWORKING)
    fetch_dependency(libslirp "https://github.com/JesseTG/libslirp-mirror" "e61dbd4")
endif ()
fetch_dependency(pntr "https://github.com/robloach/pntr" "650237a")
fetch_dependency(fmt "https://github.com/fmtlib/fmt" "11.0.2")
fetch_dependency(yamc "https://github.com/yohhoy/yamc" "4e015a7")
fetch_dependency(span-lite "https://github.com/martinmoene/span-lite" "00afc28")
fetch_dependency(date "https://github.com/HowardHinnant/date" "1ead671")

# We build zlib from source because some distributions (e.g. Ubuntu) ship a static library
# that wasn't compiled with -fPIC, which causes linking errors when building a shared library.
if (ENABLE_ZLIB)
    fetch_dependency(zlib "https://github.com/madler/zlib" "v1.3.1")
endif ()

if (TRACY_ENABLE)
    fetch_dependency(tracy "https://github.com/wolfpld/tracy" "v0.11.1")
endif()

set(BUILD_STATIC ON)
set(BUILD_STATIC_LIBS ON)
set(BUILD_QT_SDL OFF)
set(ENABLE_GDBSTUB OFF)
set(GLM_BUILD_LIBRARY ON CACHE BOOL "" FORCE)
set(GLM_ENABLE_CXX_17 ON CACHE BOOL "" FORCE)
option(ENABLE_TESTING "Enable unit testing." OFF)
set(MELONDSDS_DEPENDENCIES melonDS libretro-common embed-binaries glm)
if (ENABLE_ZLIB)
    list(APPEND MELONDSDS_DEPENDENCIES zlib)
endif ()
if (ENABLE_NETWORKING)
    list(APPEND MELONDSDS_DEPENDENCIES libslirp)
endif ()
list(APPEND MELONDSDS_DEPENDENCIES pntr fmt yamc span-lite date)
FetchContent_MakeAvailable(${MELONDSDS_DEPENDENCIES})

FetchContent_GetProperties(melonDS SOURCE_DIR MELONDS_DEPENDENCY_SOURCE_DIR)
FetchContent_GetProperties(embed-binaries SOURCE_DIR EMBED_BINARIES_DEPENDENCY_SOURCE_DIR)
list(PREPEND CMAKE_MODULE_PATH
    "${MELONDS_DEPENDENCY_SOURCE_DIR}/cmake"
    "${EMBED_BINARIES_DEPENDENCY_SOURCE_DIR}/cmake"
)

if (TRACY_ENABLE)
    set(BUILD_SHARED_LIBS OFF)
    option(TRACY_DELAYED_INIT "" ON)
    option(TRACY_MANUAL_LIFETIME "" ON)
    option(TRACY_ON_DEMAND "" ON)
    option(TRACY_STATIC "" ON)
    FetchContent_MakeAvailable(tracy)
endif()

if (ENABLE_ZLIB)
    set_target_properties(example minigzip PROPERTIES EXCLUDE_FROM_ALL TRUE)
    if(HAVE_OFF64_T)
        set_target_properties(example64 minigzip64 PROPERTIES EXCLUDE_FROM_ALL TRUE)
    endif()
endif ()
