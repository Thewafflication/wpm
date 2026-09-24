# TinyCC has no SIMD intrinsic headers. Compile only the upstream compression
# kernels with Clang; emit ELF objects for TinyCC's linker, using the Windows
# calling convention for the exported x64 entry points. These kernels have no
# CRT dependencies and never exchange vector types across the ABI boundary.
option(WPM_BLAKE2B_SIMD "Build CPU-dispatched BLAKE2b SIMD kernels (requires Clang)" ON)
set(WPM_BLAKE2B_SIMD_ENABLED OFF)
if(WIN32 AND WPM_BLAKE2B_SIMD AND wpm_arch MATCHES "^(x86|x64)$")
  file(GLOB wpm_simd_clang_hints
    "$ENV{ProgramFiles}/Microsoft Visual Studio/*/*/VC/Tools/Llvm/x64/bin")
  find_program(WPM_SIMD_CLANG NAMES clang clang.exe
    HINTS "$ENV{ProgramFiles}/LLVM/bin" ${wpm_simd_clang_hints}
    DOC "Clang used only to compile BLAKE2b SIMD kernels" REQUIRED)
  get_filename_component(wpm_simd_llvm_dir "${WPM_SIMD_CLANG}" DIRECTORY)
  find_program(WPM_SIMD_OBJCOPY NAMES llvm-objcopy llvm-objcopy.exe
    HINTS "${wpm_simd_llvm_dir}" REQUIRED)
  find_program(WPM_SIMD_NM NAMES llvm-nm llvm-nm.exe
    HINTS "${wpm_simd_llvm_dir}" REQUIRED)
  set(wpm_blake2b_ref "${LIBSODIUM_SOURCE_DIR}/src/libsodium/crypto_generichash/blake2b/ref")
  set(wpm_simd_definitions HAVE_EMMINTRIN_H=1 HAVE_TMMINTRIN_H=1
    HAVE_SMMINTRIN_H=1 HAVE_AVXINTRIN_H=1 HAVE_AVX2INTRIN_H=1)
  set_property(SOURCE "${wpm_blake2b_ref}/blake2b-ref.c" APPEND PROPERTY
    COMPILE_DEFINITIONS ${wpm_simd_definitions})
  set_property(SOURCE "${LIBSODIUM_SOURCE_DIR}/src/libsodium/sodium/runtime.c"
    APPEND PROPERTY COMPILE_DEFINITIONS ${wpm_simd_definitions} HAVE_CPUID=1 HAVE__XGETBV=1)
  set_property(SOURCE "${LIBSODIUM_SOURCE_DIR}/src/libsodium/sodium/runtime.c"
    APPEND PROPERTY COMPILE_OPTIONS -include "${CMAKE_SOURCE_DIR}/cmake/wpm_sodium_xgetbv.h")
  if(wpm_arch STREQUAL "x64")
    set(wpm_simd_triple x86_64-unknown-linux-gnu)
  else()
    set(wpm_simd_triple i386-unknown-linux-gnu)
  endif()
  file(GLOB_RECURSE wpm_simd_headers
    "${wpm_blake2b_ref}/*.h" "${LIBSODIUM_SOURCE_DIR}/src/libsodium/include/*.h"
    "${WPM_WCRT_ROOT}/include/*.h")
  foreach(wpm_simd_backend ssse3 sse41 avx2)
    set(wpm_simd_source "${wpm_blake2b_ref}/blake2b-compress-${wpm_simd_backend}.c")
    set(wpm_simd_object "${CMAKE_CURRENT_BINARY_DIR}/blake2b-${wpm_simd_backend}.o")
    set(wpm_simd_abi)
    if(wpm_arch STREQUAL "x64")
      set(wpm_simd_abi
        "-Dblake2b_compress_${wpm_simd_backend}=__attribute__((ms_abi)) blake2b_compress_${wpm_simd_backend}")
    endif()
    add_custom_command(OUTPUT "${wpm_simd_object}"
      COMMAND "${WPM_SIMD_CLANG}" -target ${wpm_simd_triple}
        -O3 -std=c99 -ffreestanding -fno-stack-protector
        -fno-asynchronous-unwind-tables -fno-unwind-tables
        -fno-pic -fno-pie -mno-red-zone -mstackrealign
        ${wpm_simd_abi}
        -isystem "${WPM_WCRT_ROOT}/include"
        -I "${LIBSODIUM_GENERATED_INCLUDE_DIR}"
        -I "${LIBSODIUM_SOURCE_DIR}/src/libsodium/include"
        -I "${LIBSODIUM_SOURCE_DIR}/src/libsodium/include/sodium"
        -DHAVE_EMMINTRIN_H=1 -DHAVE_TMMINTRIN_H=1 -DHAVE_SMMINTRIN_H=1
        -DHAVE_AVX2INTRIN_H=1 -DNATIVE_LITTLE_ENDIAN=1 -DCONFIGURED=1
        -c "${wpm_simd_source}" -o "${wpm_simd_object}"
      # TinyCC's PE read-only-data merging can lose constant-pool alignment.
      # Merge constants into .text instead: ELF input alignment is retained
      # within that section, which is page-aligned in the final PE image.
      COMMAND "${WPM_SIMD_OBJCOPY}"
        --rename-section=.rodata=.text,alloc,load,readonly,code,contents
        --rename-section=.rodata.cst4=.text,alloc,load,readonly,code,contents
        --rename-section=.rodata.cst8=.text,alloc,load,readonly,code,contents
        --rename-section=.rodata.cst16=.text,alloc,load,readonly,code,contents
        --rename-section=.rodata.cst32=.text,alloc,load,readonly,code,contents
        --rename-section=.rodata.cst64=.text,alloc,load,readonly,code,contents
        "${wpm_simd_object}"
      COMMAND "${CMAKE_COMMAND}" "-DSIMD_NM=${WPM_SIMD_NM}"
        "-DSIMD_OBJECT=${wpm_simd_object}" -P "${CMAKE_SOURCE_DIR}/cmake/CheckSimdObject.cmake"
      DEPENDS "${wpm_simd_source}" ${wpm_simd_headers}
        "${CMAKE_SOURCE_DIR}/cmake/CheckSimdObject.cmake"
      VERBATIM COMMENT "Building BLAKE2b ${wpm_simd_backend} kernel for ${wpm_arch}")
    set_source_files_properties("${wpm_simd_object}" PROPERTIES
      GENERATED TRUE EXTERNAL_OBJECT TRUE)
    target_sources(libsodium PRIVATE "${wpm_simd_object}")
  endforeach()
  set(WPM_BLAKE2B_SIMD_ENABLED ON)
  message(STATUS "BLAKE2b SIMD: SSSE3/SSE4.1/AVX2, runtime CPU/OS detection (${WPM_SIMD_CLANG})")
else()
  message(STATUS "BLAKE2b SIMD: disabled; scalar implementation")
endif()
