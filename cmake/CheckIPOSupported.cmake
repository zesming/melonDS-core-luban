if (NOT CMAKE_SYSTEM_NAME STREQUAL "OHOS")
    include("${CMAKE_ROOT}/Modules/CheckIPOSupported.cmake")
    return()
endif ()

# The HMS toolchain is layered over the OpenHarmony toolchain. CMake's nested
# IPO probe loses the SDK roots while configuring its child project, even when
# IPO is disabled by the embedding application. Fail closed on OHOS and let
# melonDS keep both ENABLE_LTO options off without running that probe.
function(check_ipo_supported)
    set(one_value_args RESULT OUTPUT)
    set(multi_value_args LANGUAGES)
    cmake_parse_arguments(IPO "" "${one_value_args}" "${multi_value_args}" ${ARGN})
    if (NOT IPO_RESULT)
        message(FATAL_ERROR "check_ipo_supported requires a RESULT variable")
    endif ()
    set(${IPO_RESULT} FALSE PARENT_SCOPE)
    if (IPO_OUTPUT)
        set(${IPO_OUTPUT} "IPO capability probing is disabled for OHOS cross builds" PARENT_SCOPE)
    endif ()
endfunction()
