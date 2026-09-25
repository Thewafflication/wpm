# Keep the generic zlib-ng build and dispatch only the supported SSE2 kernels.
option(WPM_ZLIB_SSE2 "Use TinyCC SSE2 compression and extraction kernels" ON)
set(WPM_ZLIB_SSE2_ENABLED OFF)
if(WIN32 AND WPM_ZLIB_SSE2 AND wpm_arch MATCHES "^(x86|x64)$")
  set(wpm_zlib_generic "${ZLIB_NG_SOURCE_DIR}/arch/generic")
  set_property(SOURCE "${wpm_zlib_generic}/chunkset_c.c"
    DIRECTORY "${ZLIB_NG_SOURCE_DIR}" APPEND PROPERTY COMPILE_DEFINITIONS
    chunkmemset_safe_c=wpm_chunkmemset_scalar inflate_fast_c=wpm_inflate_scalar)
  set_property(SOURCE "${wpm_zlib_generic}/slide_hash_c.c"
    DIRECTORY "${ZLIB_NG_SOURCE_DIR}" APPEND PROPERTY COMPILE_DEFINITIONS
    slide_hash_c=wpm_slide_hash_scalar)
  set(wpm_zlib_sse2_sources
    "${ZLIB_NG_SOURCE_DIR}/arch/x86/chunkset_sse2.c"
    "${ZLIB_NG_SOURCE_DIR}/arch/x86/slide_hash_sse2.c")
  set_source_files_properties(${wpm_zlib_sse2_sources}
    DIRECTORY "${ZLIB_NG_SOURCE_DIR}" PROPERTIES COMPILE_DEFINITIONS X86_SSE2=1)
  target_sources(zlib-ng PRIVATE ${wpm_zlib_sse2_sources}
    "${CMAKE_SOURCE_DIR}/cmake/wpm_zlib_dispatch.c")
  target_include_directories(zlib-ng PRIVATE "${CMAKE_SOURCE_DIR}/cmake/tcc-sse2")
  set(WPM_ZLIB_SSE2_ENABLED ON)
  message(STATUS "zlib-ng: TinyCC SSE2 kernels with WCRT runtime detection")
endif()
