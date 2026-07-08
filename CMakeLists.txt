cmake_minimum_required(VERSION 3.20)

project(AstraphosVC VERSION 0.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(ASTRAPHOSVC_BUILD_TESTS "Build AstraphosVC tests" ON)

find_package(ZLIB REQUIRED)

add_library(astraphosvc_core
    commits/avc_commit.c
    compression/avc_compress.c
    config/avc_config.c
    hashing/avc_hash.c
    index/avc_index.c
    merge/avc_merge.c
    objects/avc_oid.c
    objects/avc_object.c
    refs/avc_refs.c
    repository/avc_repository.c
    utils/avc_error.c
    utils/avc_fs.c
    utils/avc_log.c
)
target_include_directories(astraphosvc_core PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_options(astraphosvc_core PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
)
target_link_libraries(astraphosvc_core PUBLIC ZLIB::ZLIB)

add_executable(astraphosvc
    cli/avc_cli.c
    cli/main.c
)
target_link_libraries(astraphosvc PRIVATE astraphosvc_core)
target_compile_options(astraphosvc PRIVATE
    $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
)

install(TARGETS astraphosvc RUNTIME DESTINATION bin)

enable_testing()
if(ASTRAPHOSVC_BUILD_TESTS)
    add_executable(test_phase1
        tests/unit/test_phase1.c
    )
    target_link_libraries(test_phase1 PRIVATE astraphosvc_core)
    target_compile_options(test_phase1 PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    )
    add_test(NAME phase1-unit COMMAND test_phase1)

    add_executable(test_phase2
        tests/unit/test_phase2.c
    )
    target_link_libraries(test_phase2 PRIVATE astraphosvc_core)
    target_compile_options(test_phase2 PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    )
    add_test(NAME phase2-unit COMMAND test_phase2)

    add_executable(test_phase3
        tests/unit/test_phase3.c
    )
    target_link_libraries(test_phase3 PRIVATE astraphosvc_core)
    target_compile_options(test_phase3 PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    )
    add_test(NAME phase3-unit COMMAND test_phase3)

    add_executable(test_phase4
        tests/unit/test_phase4.c
    )
    target_link_libraries(test_phase4 PRIVATE astraphosvc_core)
    target_compile_options(test_phase4 PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    )
    add_test(NAME phase4-unit COMMAND test_phase4)

    add_executable(test_phase5
        tests/unit/test_phase5.c
    )
    target_link_libraries(test_phase5 PRIVATE astraphosvc_core)
    target_compile_options(test_phase5 PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    )
    add_test(NAME phase5-unit COMMAND test_phase5)

    add_executable(test_phase6
        tests/unit/test_phase6.c
    )
    target_link_libraries(test_phase6 PRIVATE astraphosvc_core)
    target_compile_options(test_phase6 PRIVATE
        $<$<C_COMPILER_ID:GNU,Clang>:-Wall -Wextra -Wpedantic -Werror>
    )
    add_test(NAME phase6-unit COMMAND test_phase6)

    add_test(NAME phase1-cli-version COMMAND astraphosvc version)
    set_tests_properties(phase1-cli-version PROPERTIES PASS_REGULAR_EXPRESSION "AstraphosVC 0.1.0")

    add_test(NAME phase1-cli-help COMMAND astraphosvc help)
    set_tests_properties(phase1-cli-help PROPERTIES PASS_REGULAR_EXPRESSION "Implemented commands")

    add_test(NAME phase1-cli-init
        COMMAND sh ${CMAKE_CURRENT_SOURCE_DIR}/tests/integration/test_phase1_cli.sh $<TARGET_FILE:astraphosvc>
    )
endif()
