set(wpm_mbedtls_root "${CMAKE_SOURCE_DIR}/third_party/mbedtls")
if(NOT EXISTS "${wpm_mbedtls_root}/include/mbedtls/ssl.h")
  message(FATAL_ERROR "Initialize the pinned Mbed TLS submodule before building WPM")
endif()
file(GLOB wpm_mbedtls_sources "${wpm_mbedtls_root}/library/*.c")
file(GLOB_RECURSE wpm_mbedtls_headers "${wpm_mbedtls_root}/include/*.h" "${wpm_mbedtls_root}/library/*.h")
set_source_files_properties(${wpm_mbedtls_sources} PROPERTIES OBJECT_DEPENDS
  "${CMAKE_SOURCE_DIR}/cmake/wpm_mbedtls_config.h;${wpm_mbedtls_headers}")
add_library(wpm-mbedtls STATIC ${wpm_mbedtls_sources}
  "${CMAKE_SOURCE_DIR}/cmake/wpm_mbedtls_platform.c")
target_include_directories(wpm-mbedtls PUBLIC "${wpm_mbedtls_root}/include"
  "${CMAKE_SOURCE_DIR}/cmake")
target_compile_definitions(wpm-mbedtls PUBLIC MBEDTLS_CONFIG_FILE="wpm_mbedtls_config.h")
target_compile_options(wpm-mbedtls PRIVATE -include wchar.h)
target_compile_definitions(wpm-mbedtls PRIVATE _WINT_T WCRT_POSIX=1)
set_target_properties(wpm-mbedtls PROPERTIES C_STANDARD 99 C_STANDARD_REQUIRED ON)

# Embed the audited CA snapshot so standalone release executables can use TLS.
set(wpm_ca_bundle "${CMAKE_SOURCE_DIR}/third_party/certificates/cacert.pem")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${wpm_ca_bundle}")
file(READ "${wpm_ca_bundle}" wpm_ca_pem)
string(REGEX MATCHALL "-----BEGIN CERTIFICATE-----[A-Za-z0-9+/=\r\n]+-----END CERTIFICATE-----"
  wpm_ca_certificates "${wpm_ca_pem}")
if(NOT wpm_ca_certificates)
  message(FATAL_ERROR "The bundled CA file contains no PEM certificates")
endif()
set(wpm_ca_header "static const char wpm_ca_pem[] =\n")
foreach(wpm_ca_certificate IN LISTS wpm_ca_certificates)
  string(REPLACE "\r" "" wpm_ca_certificate "${wpm_ca_certificate}")
  string(REPLACE "\n" "\\n\"\n\"" wpm_ca_certificate "${wpm_ca_certificate}")
  string(APPEND wpm_ca_header "\"${wpm_ca_certificate}\\n\"\n")
endforeach()
string(APPEND wpm_ca_header ";\n")
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/wpm_ca_bundle.h" "${wpm_ca_header}")
