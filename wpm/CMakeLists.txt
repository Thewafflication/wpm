# CMakeList.txt : CMake project for wpm, include source and define
# project specific logic here.
#

# Add source to this project's executable.
set(WPM_VERSION_HEADER "${CMAKE_CURRENT_BINARY_DIR}/version.h")

add_custom_target(wpm_version_header ALL
  COMMAND ${CMAKE_COMMAND}
    -DWPM_SOURCE_DIR=${CMAKE_SOURCE_DIR}
    -DWPM_VERSION_HEADER=${WPM_VERSION_HEADER}
    -P ${CMAKE_SOURCE_DIR}/cmake/GenerateVersion.cmake
  BYPRODUCTS ${WPM_VERSION_HEADER}
  COMMENT "Generating WPM version from Git"
  VERBATIM
)

add_executable (wpm
  "wpm.c"
  "archive.c"
  "archive.h"
  "init.c"
  "init.h"
  "helpers.c"
  "helpers.h"
)

add_dependencies(wpm wpm_version_header)

if(CMAKE_CONFIGURATION_TYPES)
  set(wpm_output_dir "${WPM_BIN_DIR}")
elseif(CMAKE_BUILD_TYPE)
  set(wpm_output_dir "${WPM_BIN_DIR}/${CMAKE_BUILD_TYPE}")
else()
  set(wpm_output_dir "${WPM_BIN_DIR}")
endif()

set_target_properties(wpm PROPERTIES
  RUNTIME_OUTPUT_DIRECTORY "${wpm_output_dir}"
  PDB_OUTPUT_DIRECTORY "${wpm_output_dir}"
)

foreach(config Debug Release RelWithDebInfo MinSizeRel)
  string(TOUPPER "${config}" config_upper)
  set_target_properties(wpm PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY_${config_upper} "${WPM_BIN_DIR}/${config}"
    PDB_OUTPUT_DIRECTORY_${config_upper} "${WPM_BIN_DIR}/${config}"
  )
endforeach()

target_include_directories(wpm PRIVATE
  ${CMAKE_CURRENT_BINARY_DIR}
)

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET wpm PROPERTY C_STANDARD 11)
  set_property(TARGET wpm PROPERTY C_STANDARD_REQUIRED ON)
endif()

target_compile_definitions(wpm PRIVATE SODIUM_STATIC)
target_link_libraries(wpm PRIVATE miniz libsodium)

if(BUILD_TESTING)
  find_program(WPM_POWERSHELL_EXECUTABLE NAMES pwsh powershell)
  if(NOT WPM_POWERSHELL_EXECUTABLE)
    message(FATAL_ERROR "PowerShell is required to run the WPM tests")
  endif()

  option(WPM_RUN_TESTS_AFTER_BUILD "Run WPM tests as part of the default build" OFF)
  option(WPM_BUILD_TEST_REPORTS "Generate local LaTeX test reports from automated test execution" OFF)
  if(WPM_BUILD_TEST_REPORTS)
    if(CMAKE_CONFIGURATION_TYPES)
      set(WPM_TEST_REPORT_DIR "${WPM_BIN_DIR}/${CMAKE_CFG_INTDIR}")
    else()
      set(WPM_TEST_REPORT_DIR "${wpm_output_dir}")
    endif()
    find_program(WPM_PDFLATEX_EXECUTABLE
      NAMES pdflatex pdflatex.exe
      HINTS
        "$ENV{LOCALAPPDATA}/Programs/MiKTeX/miktex/bin/x64"
        "$ENV{LOCALAPPDATA}/Programs/MiKTeX/miktex/bin"
        "$ENV{APPDATA}/MiKTeX/miktex/bin/x64"
        "$ENV{APPDATA}/MiKTeX/miktex/bin"
    )
    if(NOT WPM_PDFLATEX_EXECUTABLE)
      message(STATUS "pdflatex was not found; WPM_BUILD_TEST_REPORTS will generate LaTeX .tex reports only.")
    endif()
  endif()

  macro(wpm_add_test_case test_id test_slug test_script test_tex)
    add_test(
      NAME ${test_id}
      COMMAND ${WPM_POWERSHELL_EXECUTABLE}
        -NoProfile
        -ExecutionPolicy Bypass
        -File ${CMAKE_SOURCE_DIR}/tests/${test_script}
        -WpmExe $<TARGET_FILE:wpm>
    )

    if(WPM_BUILD_TEST_REPORTS)
      set(WPM_${test_slug}_EVIDENCE_TEX "${WPM_TEST_REPORT_DIR}/${test_slug}-execution-evidence.tex")
      set(WPM_${test_slug}_REPORT_TEX "${WPM_TEST_REPORT_DIR}/${test_slug}-report.tex")
      list(APPEND WPM_TEST_REPORT_TEX_OUTPUTS "${WPM_${test_slug}_REPORT_TEX}")

      add_custom_command(
        OUTPUT "${WPM_${test_slug}_EVIDENCE_TEX}"
        COMMAND ${WPM_POWERSHELL_EXECUTABLE}
          -NoProfile
          -ExecutionPolicy Bypass
          -File ${CMAKE_SOURCE_DIR}/tests/${test_script}
          -WpmExe $<TARGET_FILE:wpm>
          -EvidenceTex "${WPM_${test_slug}_EVIDENCE_TEX}"
          -NoFailOnFailure
        DEPENDS
          wpm
          ${CMAKE_SOURCE_DIR}/tests/${test_script}
          ${CMAKE_SOURCE_DIR}/tests/wpm-test-lib.ps1
        COMMENT "Running ${test_id} and collecting LaTeX execution evidence"
        VERBATIM
      )

      add_custom_command(
        OUTPUT "${WPM_${test_slug}_REPORT_TEX}"
        COMMAND ${WPM_POWERSHELL_EXECUTABLE}
          -NoProfile
          -ExecutionPolicy Bypass
          -File ${CMAKE_SOURCE_DIR}/tests/generate-test-report.ps1
          -TestCaseTex ${CMAKE_SOURCE_DIR}/docs/${test_tex}
          -EvidenceTex "${WPM_${test_slug}_EVIDENCE_TEX}"
          -OutputTex "${WPM_${test_slug}_REPORT_TEX}"
        DEPENDS
          "${WPM_${test_slug}_EVIDENCE_TEX}"
          ${CMAKE_SOURCE_DIR}/tests/generate-test-report.ps1
          ${CMAKE_SOURCE_DIR}/docs/${test_tex}
          ${CMAKE_SOURCE_DIR}/docs/iso_iec_ieee29119-3_2021.tex
        COMMENT "Generating ${test_id} LaTeX test report"
        VERBATIM
      )

      if(WPM_PDFLATEX_EXECUTABLE)
        set(WPM_${test_slug}_REPORT_PDF "${WPM_TEST_REPORT_DIR}/${test_slug}-report.pdf")
        list(APPEND WPM_TEST_REPORT_PDF_OUTPUTS "${WPM_${test_slug}_REPORT_PDF}")

        add_custom_command(
          OUTPUT "${WPM_${test_slug}_REPORT_PDF}"
          COMMAND ${WPM_POWERSHELL_EXECUTABLE}
            -NoProfile
            -ExecutionPolicy Bypass
            -File ${CMAKE_SOURCE_DIR}/tests/generate-test-report.ps1
            -TestCaseTex ${CMAKE_SOURCE_DIR}/docs/${test_tex}
            -EvidenceTex "${WPM_${test_slug}_EVIDENCE_TEX}"
            -OutputTex "${WPM_${test_slug}_REPORT_TEX}"
            -PdfLatex "${WPM_PDFLATEX_EXECUTABLE}"
          DEPENDS
            "${WPM_${test_slug}_EVIDENCE_TEX}"
            ${CMAKE_SOURCE_DIR}/tests/generate-test-report.ps1
            ${CMAKE_SOURCE_DIR}/docs/${test_tex}
            ${CMAKE_SOURCE_DIR}/docs/iso_iec_ieee29119-3_2021.tex
          COMMENT "Generating ${test_id} PDF test report"
          VERBATIM
        )
      endif()
    endif()
  endmacro()

  wpm_add_test_case(TC-0001 tc-0001 tc-0001-usage-version-check.ps1 tc-0001-usage-version-check.tex)
  wpm_add_test_case(TC-0002 tc-0002 tc-0002-package-initialization.ps1 tc-0002-package-initialization.tex)
  wpm_add_test_case(TC-0003 tc-0003 tc-0003-package-archive-build.ps1 tc-0003-package-archive-build.tex)
  wpm_add_test_case(TC-0004 tc-0004 tc-0004-package-archive-installation.ps1 tc-0004-package-archive-installation.tex)
  wpm_add_test_case(TC-0005 tc-0005 tc-0005-package-index-signature-verification.ps1 tc-0005-package-index-signature-verification.tex)
  wpm_add_test_case(TC-0006 tc-0006 tc-0006-package-metadata-archive-naming.ps1 tc-0006-package-metadata-archive-naming.tex)
  wpm_add_test_case(TC-0007 tc-0007 tc-0007-wpm-self-installation.ps1 tc-0007-wpm-self-installation.tex)
  wpm_add_test_case(TC-0008 tc-0008 tc-0008-package-removal.ps1 tc-0008-package-removal.tex)
  wpm_add_test_case(TC-0009 tc-0009 tc-0009-github-release-artifacts.ps1 tc-0009-github-release-artifacts.tex)

  add_custom_target(check
    COMMAND ${CMAKE_CTEST_COMMAND} --build-config $<CONFIG> --output-on-failure
    DEPENDS
      wpm
      wpm-third-party
      ${WPM_TEST_REPORT_TEX_OUTPUTS}
    WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
    COMMENT "Running WPM tests"
  )

  if(WPM_BUILD_TEST_REPORTS)
    add_custom_target(test-reports DEPENDS ${WPM_TEST_REPORT_TEX_OUTPUTS})
    if(WPM_PDFLATEX_EXECUTABLE)
      add_custom_target(test-report-pdfs DEPENDS ${WPM_TEST_REPORT_PDF_OUTPUTS})
    endif()

    if(CMAKE_CONFIGURATION_TYPES)
      set(WPM_PACKAGE_REPORT_DIR "${WPM_BIN_DIR}/${CMAKE_CFG_INTDIR}")
      set(WPM_PACKAGE_OUTPUT_DIR "${WPM_BIN_DIR}/${CMAKE_CFG_INTDIR}/packages")
      set(WPM_PACKAGE_STAGING_DIR "${CMAKE_BINARY_DIR}/wpm-package/${CMAKE_CFG_INTDIR}")
    else()
      set(WPM_PACKAGE_REPORT_DIR "${wpm_output_dir}")
      set(WPM_PACKAGE_OUTPUT_DIR "${wpm_output_dir}/packages")
      set(WPM_PACKAGE_STAGING_DIR "${CMAKE_BINARY_DIR}/wpm-package")
    endif()

    add_custom_target(wpm-package
      COMMAND ${CMAKE_COMMAND}
        -DWPM_EXECUTABLE=$<TARGET_FILE:wpm>
        -DWPM_PACKAGE_EXECUTABLE=$<TARGET_FILE:wpm>
        -DWPM_PACKAGE_STAGING_DIR=${WPM_PACKAGE_STAGING_DIR}
        -DWPM_PACKAGE_OUTPUT_DIR=${WPM_PACKAGE_OUTPUT_DIR}
        -DWPM_PACKAGE_ARCH=${wpm_arch}
        -DWPM_SETUP_SCRIPT=${CMAKE_SOURCE_DIR}/setup.cmd
        -DWPM_REMOVE_SCRIPT=${CMAKE_SOURCE_DIR}/remove.cmd
        -DWPM_REPORT_DIR=${WPM_PACKAGE_REPORT_DIR}
        -DWPM_DOCS_DIR=${CMAKE_SOURCE_DIR}/docs
        -DWPM_README_FILE=${CMAKE_SOURCE_DIR}/README.md
        -DWPM_LICENSE_FILE=${CMAKE_SOURCE_DIR}/LICENSE.txt
        -DWPM_THIRD_PARTY_NOTICES_FILE=${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
        -P ${CMAKE_SOURCE_DIR}/cmake/BuildSelfPackage.cmake
      DEPENDS
        wpm
        test-reports
        ${CMAKE_SOURCE_DIR}/setup.cmd
        ${CMAKE_SOURCE_DIR}/remove.cmd
        ${CMAKE_SOURCE_DIR}/cmake/BuildSelfPackage.cmake
        ${CMAKE_SOURCE_DIR}/README.md
        ${CMAKE_SOURCE_DIR}/LICENSE.txt
        ${CMAKE_SOURCE_DIR}/THIRD_PARTY_NOTICES.md
      COMMENT "Building a WPM package with test reports"
      VERBATIM
    )
  endif()

  add_custom_target(verify)
  add_dependencies(verify check)
  if(WPM_BUILD_TEST_REPORTS)
    add_dependencies(verify test-reports)
  endif()

  if(WPM_RUN_TESTS_AFTER_BUILD)
    add_custom_target(wpm-run-tests-after-build ALL
      DEPENDS check
    )
  endif()
endif()
