# Linux-target ELF is safe here only for self-contained kernels. In particular,
# an introduced CRT call could silently cross the wrong x64 calling convention.
execute_process(COMMAND "${SIMD_NM}" --undefined-only "${SIMD_OBJECT}"
  RESULT_VARIABLE result OUTPUT_VARIABLE undefined ERROR_VARIABLE error
  OUTPUT_STRIP_TRAILING_WHITESPACE)
if(NOT result EQUAL 0 OR NOT undefined STREQUAL "")
  message(FATAL_ERROR "SIMD kernel must have no external dependencies: ${SIMD_OBJECT}\n${undefined}\n${error}")
endif()
