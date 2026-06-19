if(NOT OPTIONX_RUNTIME_DLL_DIR OR NOT OPTIONX_RUNTIME_TARGET_DIR)
    return()
endif()

if(NOT EXISTS "${OPTIONX_RUNTIME_DLL_DIR}")
    return()
endif()

file(GLOB optionx_runtime_files
    "${OPTIONX_RUNTIME_DLL_DIR}/*.dll"
    "${OPTIONX_RUNTIME_DLL_DIR}/curl-ca-bundle.crt"
)

foreach(optionx_runtime_file IN LISTS optionx_runtime_files)
    file(COPY "${optionx_runtime_file}" DESTINATION "${OPTIONX_RUNTIME_TARGET_DIR}")
endforeach()
