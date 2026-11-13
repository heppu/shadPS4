function(configure_profile_build)
    target_compile_options(shadps4 PRIVATE -march=native)
    if(SHAD_PGO_STAGE STREQUAL "GENERATE")
        target_compile_options(shadps4 PRIVATE
            -fprofile-generate=/out/performance/pgo-raw)
        target_link_options(shadps4 PRIVATE
            -fprofile-generate=/out/performance/pgo-raw)
        target_sources(shadps4 PRIVATE /out/performance/pgo-checkpoint.cpp)
    elseif(SHAD_PGO_STAGE STREQUAL "USE")
        target_compile_options(shadps4 PRIVATE
            -fprofile-use=/out/performance/pgo.profdata)
        target_link_options(shadps4 PRIVATE
            -fprofile-use=/out/performance/pgo.profdata)
    else()
        message(FATAL_ERROR "SHAD_PGO_STAGE must be GENERATE or USE")
    endif()
endfunction()
cmake_language(DEFER DIRECTORY "${CMAKE_SOURCE_DIR}" CALL configure_profile_build)
