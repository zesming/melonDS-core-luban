set(MELONDSDS_ATTRIBUTION_INPUTS
    "melondsds-wrapper|${PROJECT_SOURCE_DIR}/LICENSE"
    "melonds-upstream|${melonDS_SOURCE_DIR}/LICENSE"
    "dolphin-optional|${melonDS_SOURCE_DIR}/src/dolphin/license_dolphin.txt"
    "fatfs|${melonDS_SOURCE_DIR}/src/fatfs/LICENSE.txt"
    "teakra|${melonDS_SOURCE_DIR}/src/teakra/LICENSE"
    "freebios|${melonDS_SOURCE_DIR}/freebios/drastic_bios_readme.txt"
    "blip-buf|${melonDS_SOURCE_DIR}/src/blip-buf/license.txt"
    "libretro-common|${PROJECT_SOURCE_DIR}/cmake/libretro-common-LICENSE.txt"
    "fmt|${fmt_SOURCE_DIR}/LICENSE"
    "glm|${glm_SOURCE_DIR}/copying.txt"
    "pntr|${pntr_SOURCE_DIR}/LICENSE.md"
    "date|${date_SOURCE_DIR}/LICENSE.txt"
    "open-sans|${PROJECT_SOURCE_DIR}/src/libretro/assets/OpenSans-LICENSE.txt"
    "pcap-optional|${melonDS_SOURCE_DIR}/src/net/pcap/pcap.h"
    "sha1|${melonDS_SOURCE_DIR}/src/sha1/sha1.c"
    "xxhash|${melonDS_SOURCE_DIR}/src/xxhash/xxhash.h"
    "yamc|${yamc_SOURCE_DIR}/LICENSE"
    "span-lite|${span-lite_SOURCE_DIR}/LICENSE.txt"
    "tiny-aes-c|${melonDS_SOURCE_DIR}/src/tiny-AES-c/unlicense.txt"
    "libretro-common-base64|${libretro-common_SOURCE_DIR}/encodings/encoding_base64.c"
    "libretro-common-md5|${libretro-common_SOURCE_DIR}/utils/md5.c"
    "stb-image|${pntr_SOURCE_DIR}/external/stb_image.h"
    "stb-image-write|${pntr_SOURCE_DIR}/external/stb_image_write.h"
    "stb-truetype|${pntr_SOURCE_DIR}/external/stb_truetype.h"
    "font8x8-basic|${pntr_SOURCE_DIR}/external/font8x8_basic.h")
if (ENABLE_NETWORKING)
    list(APPEND MELONDSDS_ATTRIBUTION_INPUTS "libslirp|${libslirp_SOURCE_DIR}/COPYRIGHT")
endif ()
if (ENABLE_ZLIB)
    list(APPEND MELONDSDS_ATTRIBUTION_INPUTS "zlib|${zlib_SOURCE_DIR}/LICENSE")
endif ()
set(MELONDSDS_ATTRIBUTION_DEPENDENCIES "")
set(MELONDSDS_ATTRIBUTION_INPUT_DIGESTS "")
foreach (MELONDSDS_ATTRIBUTION_INPUT IN LISTS MELONDSDS_ATTRIBUTION_INPUTS)
    string(REPLACE "|" ";" MELONDSDS_ATTRIBUTION_INPUT_FIELDS "${MELONDSDS_ATTRIBUTION_INPUT}")
    list(GET MELONDSDS_ATTRIBUTION_INPUT_FIELDS 0 MELONDSDS_ATTRIBUTION_LABEL)
    list(GET MELONDSDS_ATTRIBUTION_INPUT_FIELDS 1 MELONDSDS_ATTRIBUTION_PATH)
    list(APPEND MELONDSDS_ATTRIBUTION_DEPENDENCIES "${MELONDSDS_ATTRIBUTION_PATH}")
    file(SHA256 "${MELONDSDS_ATTRIBUTION_PATH}" MELONDSDS_ATTRIBUTION_INPUT_SHA256)
    string(APPEND MELONDSDS_ATTRIBUTION_INPUT_DIGESTS
        "${MELONDSDS_ATTRIBUTION_INPUT_SHA256}  ${MELONDSDS_ATTRIBUTION_LABEL}\n")
endforeach ()
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${MELONDSDS_ATTRIBUTION_DEPENDENCIES}")

function(melondsds_extract_prefix source_variable marker output)
    set(source "${${source_variable}}")
    string(FIND "${source}" "${marker}" marker_index)
    if (marker_index LESS 0)
        message(FATAL_ERROR "Unable to extract attribution text before marker: ${marker}")
    endif ()
    string(LENGTH "${marker}" marker_length)
    math(EXPR extraction_length "${marker_index} + ${marker_length}")
    string(SUBSTRING "${source}" 0 "${extraction_length}" extraction)
    set("${output}" "${extraction}" PARENT_SCOPE)
endfunction ()

function(melondsds_extract_before source_variable marker output)
    set(source "${${source_variable}}")
    string(FIND "${source}" "${marker}" marker_index)
    if (marker_index LESS 0)
        message(FATAL_ERROR "Unable to extract attribution text before marker: ${marker}")
    endif ()
    string(SUBSTRING "${source}" 0 "${marker_index}" extraction)
    set("${output}" "${extraction}" PARENT_SCOPE)
endfunction ()

file(READ "${PROJECT_SOURCE_DIR}/LICENSE" MELONDSDS_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/LICENSE" MELONDS_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/src/dolphin/license_dolphin.txt" DOLPHIN_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/src/fatfs/LICENSE.txt" FATFS_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/src/teakra/LICENSE" TEAKRA_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/freebios/drastic_bios_readme.txt" FREEBIOS_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/src/blip-buf/license.txt" BLIP_BUF_LICENSE)
file(READ "${PROJECT_SOURCE_DIR}/cmake/libretro-common-LICENSE.txt" LIBRETRO_COMMON_LICENSE)
file(READ "${fmt_SOURCE_DIR}/LICENSE" FMT_LICENSE)
file(READ "${glm_SOURCE_DIR}/copying.txt" GLM_LICENSE)
file(READ "${pntr_SOURCE_DIR}/LICENSE.md" PNTR_LICENSE)
file(READ "${date_SOURCE_DIR}/LICENSE.txt" DATE_LICENSE)
file(READ "${PROJECT_SOURCE_DIR}/src/libretro/assets/OpenSans-LICENSE.txt" OPEN_SANS_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/src/net/pcap/pcap.h" PCAP_SOURCE)
melondsds_extract_before(PCAP_SOURCE "\n#ifndef lib_pcap_pcap_h" PCAP_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/src/sha1/sha1.c" SHA1_SOURCE)
melondsds_extract_before(SHA1_SOURCE "\n#include <stdio.h>" SHA1_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/src/xxhash/xxhash.h" XXHASH_SOURCE)
melondsds_extract_prefix(XXHASH_SOURCE "*/" XXHASH_LICENSE)
file(READ "${yamc_SOURCE_DIR}/LICENSE" YAMC_LICENSE)
file(READ "${libretro-common_SOURCE_DIR}/encodings/encoding_base64.c" BASE64_SOURCE)
melondsds_extract_prefix(BASE64_SOURCE "*/" BASE64_LICENSE_HEADER)
file(READ "${libretro-common_SOURCE_DIR}/utils/md5.c" MD5_SOURCE)
melondsds_extract_prefix(MD5_SOURCE "*/" MD5_LICENSE_HEADER)
file(READ "${pntr_SOURCE_DIR}/external/stb_image.h" STB_IMAGE_SOURCE)
melondsds_extract_prefix(STB_IMAGE_SOURCE "\n" STB_IMAGE_LICENSE_HEADER)
string(FIND "${STB_IMAGE_SOURCE}" "This software is available under 2 licenses -- choose whichever you prefer." STB_LICENSE_START)
if (STB_LICENSE_START LESS 0)
    message(FATAL_ERROR "Unable to extract stb attribution license")
endif ()
string(SUBSTRING "${STB_IMAGE_SOURCE}" "${STB_LICENSE_START}" -1 STB_LICENSE)
file(READ "${pntr_SOURCE_DIR}/external/stb_image_write.h" STB_IMAGE_WRITE_SOURCE)
melondsds_extract_prefix(STB_IMAGE_WRITE_SOURCE "\n" STB_IMAGE_WRITE_LICENSE_HEADER)
file(READ "${pntr_SOURCE_DIR}/external/stb_truetype.h" STB_TRUETYPE_SOURCE)
melondsds_extract_prefix(STB_TRUETYPE_SOURCE "\n// authored from 2009-2021 by Sean Barrett / RAD Game Tools" STB_TRUETYPE_LICENSE_HEADER)
file(READ "${pntr_SOURCE_DIR}/external/font8x8_basic.h" FONT8X8_SOURCE)
melondsds_extract_prefix(FONT8X8_SOURCE "*/" FONT8X8_LICENSE_HEADER)
if (ENABLE_NETWORKING)
    file(READ "${libslirp_SOURCE_DIR}/COPYRIGHT" SLIRP_LICENSE)
else ()
    set(SLIRP_LICENSE "")
endif ()
file(READ "${span-lite_SOURCE_DIR}/LICENSE.txt" SPAN_LITE_LICENSE)
file(READ "${melonDS_SOURCE_DIR}/src/tiny-AES-c/unlicense.txt" TINY_AES_LICENSE)
if (ENABLE_ZLIB)
    file(READ "${zlib_SOURCE_DIR}/LICENSE" ZLIB_LICENSE)
else ()
    set(ZLIB_LICENSE "")
endif ()

configure_file("${PROJECT_SOURCE_DIR}/cmake/melondsds-LICENSE.txt.in" "${CMAKE_CURRENT_BINARY_DIR}/melondsds-LICENSE.txt")

# Parent integrations can request an additional distributed attribution.  The
# wrapper itself remains usable standalone and only writes its build-tree copy.
if (DEFINED MELONDSDS_HAP_ATTRIBUTION_OUTPUT AND NOT "${MELONDSDS_HAP_ATTRIBUTION_OUTPUT}" STREQUAL "")
    get_filename_component(MELONDSDS_HAP_ATTRIBUTION_DIR "${MELONDSDS_HAP_ATTRIBUTION_OUTPUT}" DIRECTORY)
    file(MAKE_DIRECTORY "${MELONDSDS_HAP_ATTRIBUTION_DIR}")
    configure_file("${PROJECT_SOURCE_DIR}/cmake/melondsds-LICENSE.txt.in" "${MELONDSDS_HAP_ATTRIBUTION_OUTPUT}")
endif ()

# Tracy is disabled by the parent integration and has no distributed attribution.
