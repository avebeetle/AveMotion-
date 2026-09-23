cmake_minimum_required(VERSION 3.25)

project(AveMotion
    VERSION 0.24.0
    DESCRIPTION "AveMotion standalone Telegram-compatible vector animation SDK"
    LANGUAGES C CXX)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AveMotionOptions)
include(AveMotionWarnings)
include(AveMotionRlottie)

add_library(avemotion_miniz OBJECT
    src/formats/MinizInflate.cpp)
target_compile_features(avemotion_miniz PRIVATE cxx_std_20)
target_include_directories(avemotion_miniz PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src/formats"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/miniz")
set_target_properties(avemotion_miniz PROPERTIES
    POSITION_INDEPENDENT_CODE ON)
if(MSVC)
    target_compile_options(avemotion_miniz PRIVATE /W0)
else()
    target_compile_options(avemotion_miniz PRIVATE -w)
endif()

add_library(avemotion_formats STATIC
    src/formats/Tgs.cpp
    $<TARGET_OBJECTS:avemotion_miniz>)
add_library(AveMotion::Formats ALIAS avemotion_formats)
set_target_properties(avemotion_formats PROPERTIES EXPORT_NAME Formats)
target_compile_features(avemotion_formats PUBLIC cxx_std_20)
target_include_directories(avemotion_formats
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/formats")
avemotion_set_warnings(avemotion_formats)

add_library(avemotion_core STATIC
    src/core/Hash.cpp)
add_library(AveMotion::Core ALIAS avemotion_core)
set_target_properties(avemotion_core PROPERTIES EXPORT_NAME Core)
target_compile_features(avemotion_core PUBLIC cxx_std_20)
target_include_directories(avemotion_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
avemotion_set_warnings(avemotion_core)

add_library(avemotion_model STATIC
    src/model/AssetModel.cpp
    src/model/AssetModelBuilder.cpp)
add_library(AveMotion::Model ALIAS avemotion_model)
set_target_properties(avemotion_model PROPERTIES EXPORT_NAME Model)
target_compile_features(avemotion_model PUBLIC cxx_std_20)
target_include_directories(avemotion_model
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(avemotion_model PUBLIC AveMotion::Core)
avemotion_set_warnings(avemotion_model)

add_library(avemotion_validation STATIC
    src/validation/AssetValidator.cpp)
add_library(AveMotion::Validation ALIAS avemotion_validation)
set_target_properties(avemotion_validation PROPERTIES EXPORT_NAME Validation)
target_compile_features(avemotion_validation PUBLIC cxx_std_20)
target_include_directories(avemotion_validation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(avemotion_validation PUBLIC AveMotion::Core AveMotion::Model)
avemotion_set_warnings(avemotion_validation)

if(AVEMOTION_BUILD_CORPUS_LAB)
    add_library(avemotion_corpus_analysis STATIC
        src/lab/CorpusAnalysis.cpp)
    target_compile_features(avemotion_corpus_analysis PUBLIC cxx_std_20)
    target_include_directories(avemotion_corpus_analysis
        PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src/lab")
    target_link_libraries(avemotion_corpus_analysis
        PUBLIC AveMotion::Core AveMotion::Model AveMotion::Validation)
    avemotion_set_warnings(avemotion_corpus_analysis)
endif()

add_library(avemotion_evaluation STATIC
    src/evaluation/PropertyEvaluator.cpp)
add_library(AveMotion::Evaluation ALIAS avemotion_evaluation)
set_target_properties(avemotion_evaluation PROPERTIES EXPORT_NAME Evaluation)
target_compile_features(avemotion_evaluation PUBLIC cxx_std_20)
target_include_directories(avemotion_evaluation
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(avemotion_evaluation PUBLIC AveMotion::Core AveMotion::Model)
avemotion_set_warnings(avemotion_evaluation)

add_library(avemotion_reference STATIC
    src/reference/ReferenceRuntime.cpp
    src/reference/UpstreamInfo.cpp)
add_library(AveMotion::Reference ALIAS avemotion_reference)
set_target_properties(avemotion_reference PROPERTIES EXPORT_NAME Reference)
target_compile_features(avemotion_reference PUBLIC cxx_std_20)
target_include_directories(avemotion_reference
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${AVEMOTION_RLOTTIE_INCLUDE_DIR}")
target_link_libraries(avemotion_reference PUBLIC AveMotion::Core)
avemotion_set_warnings(avemotion_reference)

target_compile_definitions(avemotion_reference
    PRIVATE
        AVEMOTION_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
        AVEMOTION_RLOTTIE_REPOSITORY="${AVEMOTION_RLOTTIE_REPOSITORY}"
        AVEMOTION_RLOTTIE_COMMIT="${AVEMOTION_RLOTTIE_COMMIT}"
        AVEMOTION_RLOTTIE_VARIANT_NAME="${AVEMOTION_RLOTTIE_VARIANT}"
        AVEMOTION_RLOTTIE_LICENSE_FAMILY="${AVEMOTION_RLOTTIE_LICENSE_FAMILY}")

set(AVEMOTION_RUNTIME_SOURCES
    src/runtime/Runtime.cpp
    src/runtime/Playback.cpp
    src/runtime/RecordingBackend.cpp)
if(AVEMOTION_RLOTTIE_AVAILABLE)
    list(APPEND AVEMOTION_RUNTIME_SOURCES
        src/runtime/RlottieSceneBridge.cpp)
    if(AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
        list(APPEND AVEMOTION_RUNTIME_SOURCES
            src/model/TelegramParsedModelBuilder.cpp)
    endif()
endif()

add_library(avemotion_runtime STATIC ${AVEMOTION_RUNTIME_SOURCES})
add_library(AveMotion::Runtime ALIAS avemotion_runtime)
set_target_properties(avemotion_runtime PROPERTIES EXPORT_NAME Runtime)
target_compile_features(avemotion_runtime PUBLIC cxx_std_20)
target_include_directories(avemotion_runtime
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime"
        "${CMAKE_CURRENT_SOURCE_DIR}/src/model"
        "${AVEMOTION_RLOTTIE_INCLUDE_DIR}")
target_link_libraries(avemotion_runtime PUBLIC AveMotion::Core AveMotion::Formats AveMotion::Model AveMotion::Evaluation)
avemotion_set_warnings(avemotion_runtime)

if(AVEMOTION_RLOTTIE_VARIANT STREQUAL "samsung")
    target_compile_definitions(avemotion_runtime PRIVATE
        AVEMOTION_REFERENCE_ROUND_FRAME_POSITION=1)
else()
    target_compile_definitions(avemotion_runtime PRIVATE
        AVEMOTION_REFERENCE_ROUND_FRAME_POSITION=0)
endif()

if(AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
    target_compile_definitions(avemotion_runtime
        PRIVATE
            AVEMOTION_TELEGRAM_LOCAL_GEOMETRY_EXTENSIONS=1
            AVEMOTION_TELEGRAM_PARSED_MODEL=1)
    target_include_directories(avemotion_runtime SYSTEM PRIVATE
        "${AVEMOTION_RLOTTIE_INTERNAL_LOTTIE_DIR}"
        "${AVEMOTION_RLOTTIE_INTERNAL_VECTOR_DIR}"
        "${AVEMOTION_RLOTTIE_INTERNAL_PIXMAN_DIR}"
        "${AVEMOTION_RLOTTIE_GENERATED_INCLUDE_DIR}")
else()
    target_compile_definitions(avemotion_runtime
        PRIVATE
            AVEMOTION_TELEGRAM_LOCAL_GEOMETRY_EXTENSIONS=0
            AVEMOTION_TELEGRAM_PARSED_MODEL=0)
endif()

add_library(avemotion_player STATIC
    src/player/Player.cpp)
add_library(AveMotion::Player ALIAS avemotion_player)
set_target_properties(avemotion_player PROPERTIES EXPORT_NAME Player)
target_compile_features(avemotion_player PUBLIC cxx_std_20)
target_include_directories(avemotion_player
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(avemotion_player PUBLIC AveMotion::Runtime)
avemotion_set_warnings(avemotion_player)

add_library(avemotion_rendering STATIC
    src/render/RenderPlanner.cpp
    src/render/PrimitivePathGenerator.cpp
    src/render/RepeaterTransform.cpp
    src/render/TrimPathGenerator.cpp
    src/render/SourceGeometryProjector.cpp
    src/render/HeadlessBackend.cpp)
add_library(AveMotion::Rendering ALIAS avemotion_rendering)
set_target_properties(avemotion_rendering PROPERTIES EXPORT_NAME Rendering)
target_compile_features(avemotion_rendering PUBLIC cxx_std_20)
target_include_directories(avemotion_rendering
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>)
target_link_libraries(avemotion_rendering PUBLIC AveMotion::Core AveMotion::Runtime)
avemotion_set_warnings(avemotion_rendering)

if(WIN32 AND AVEMOTION_BUILD_DIRECT2D_BACKEND)
    add_library(avemotion_backend_d2d STATIC
        backends/direct2d/Direct2DBackend.cpp)
    add_library(AveMotion::Direct2D ALIAS avemotion_backend_d2d)
    set_target_properties(avemotion_backend_d2d PROPERTIES EXPORT_NAME Direct2D)
    target_compile_features(avemotion_backend_d2d PUBLIC cxx_std_20)
    target_include_directories(avemotion_backend_d2d
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:include>)
    target_link_libraries(avemotion_backend_d2d
        PUBLIC AveMotion::Rendering
        PRIVATE d2d1 dxgi)
    avemotion_set_warnings(avemotion_backend_d2d)
endif()

if(AVEMOTION_RLOTTIE_AVAILABLE)
    target_link_libraries(avemotion_reference PRIVATE rlottie::rlottie)
    target_link_libraries(avemotion_runtime PRIVATE rlottie::rlottie)
    target_compile_definitions(avemotion_reference PRIVATE AVEMOTION_HAS_RLOTTIE=1)
    target_compile_definitions(avemotion_runtime PRIVATE AVEMOTION_HAS_RLOTTIE=1)
else()
    target_compile_definitions(avemotion_reference PRIVATE AVEMOTION_HAS_RLOTTIE=0)
    target_compile_definitions(avemotion_runtime PRIVATE AVEMOTION_HAS_RLOTTIE=0)
endif()

if(AVEMOTION_BUILD_VALIDATOR_TOOL AND AVEMOTION_RLOTTIE_AVAILABLE)
    add_executable(avemotion_validate apps/validate/main.cpp)
    target_link_libraries(avemotion_validate PRIVATE
        AveMotion::Formats AveMotion::Runtime AveMotion::Validation)
    avemotion_set_warnings(avemotion_validate)
endif()

if(AVEMOTION_BUILD_VALIDATION_CHARACTERIZER
   AND AVEMOTION_RLOTTIE_AVAILABLE
   AND AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
    add_executable(avemotion_validation_characterize
        apps/validation_characterize/main.cpp)
    target_link_libraries(avemotion_validation_characterize PRIVATE
        AveMotion::Formats AveMotion::Runtime AveMotion::Validation)
    avemotion_set_warnings(avemotion_validation_characterize)
endif()

if(AVEMOTION_BUILD_CORPUS_LAB
   AND AVEMOTION_RLOTTIE_AVAILABLE
   AND AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
    add_executable(avemotion_corpus_lab apps/corpus_lab/main.cpp)
    target_compile_features(avemotion_corpus_lab PRIVATE cxx_std_20)
    target_include_directories(avemotion_corpus_lab PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/lab")
    target_link_libraries(avemotion_corpus_lab PRIVATE
        avemotion_corpus_analysis
        AveMotion::Runtime
        AveMotion::Evaluation
        AveMotion::Rendering
        AveMotion::Validation)
    if(WIN32)
        target_link_libraries(avemotion_corpus_lab PRIVATE Psapi)
    endif()
    avemotion_set_warnings(avemotion_corpus_lab)
endif()

if(AVEMOTION_BUILD_PROBE)
    add_executable(avemotion_probe apps/probe/main.cpp)
    target_link_libraries(avemotion_probe PRIVATE AveMotion::Reference)
    avemotion_set_warnings(avemotion_probe)
endif()

if(AVEMOTION_BUILD_CHARACTERIZER)
    add_executable(avemotion_characterize apps/characterize/main.cpp)
    target_link_libraries(avemotion_characterize PRIVATE AveMotion::Reference)
    avemotion_set_warnings(avemotion_characterize)
endif()

if(AVEMOTION_BUILD_SCENE_CHARACTERIZER)
    add_executable(avemotion_scene_characterize apps/scene_characterize/main.cpp)
    target_link_libraries(avemotion_scene_characterize
        PRIVATE AveMotion::Runtime AveMotion::Reference)
    avemotion_set_warnings(avemotion_scene_characterize)
endif()

if(AVEMOTION_BUILD_PLAN_CHARACTERIZER)
    add_executable(avemotion_plan_characterize apps/plan_characterize/main.cpp)
    target_link_libraries(avemotion_plan_characterize
        PRIVATE AveMotion::Rendering AveMotion::Reference)
    avemotion_set_warnings(avemotion_plan_characterize)
endif()

if(AVEMOTION_BUILD_MODEL_CHARACTERIZER
   AND AVEMOTION_RLOTTIE_AVAILABLE
   AND AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
    add_executable(avemotion_model_characterize apps/model_characterize/main.cpp)
    target_link_libraries(avemotion_model_characterize
        PRIVATE AveMotion::Runtime AveMotion::Reference)
    avemotion_set_warnings(avemotion_model_characterize)
endif()

if(AVEMOTION_BUILD_PROPERTY_CHARACTERIZER
   AND AVEMOTION_RLOTTIE_AVAILABLE
   AND AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
    add_executable(avemotion_property_characterize
        apps/property_characterize/main.cpp)
    target_include_directories(avemotion_property_characterize PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/model")
    target_link_libraries(avemotion_property_characterize
        PRIVATE AveMotion::Evaluation AveMotion::Runtime AveMotion::Reference)
    avemotion_set_warnings(avemotion_property_characterize)
endif()

if(AVEMOTION_RLOTTIE_AVAILABLE
   AND AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
    add_executable(avemotion_source_geometry_characterize
        apps/source_geometry_characterize/main.cpp)
    target_link_libraries(avemotion_source_geometry_characterize
        PRIVATE AveMotion::Rendering AveMotion::Evaluation
                AveMotion::Runtime AveMotion::Reference)
    avemotion_set_warnings(avemotion_source_geometry_characterize)
endif()

if(AVEMOTION_BUILD_WIN32_PREVIEW)
    if(NOT WIN32)
        message(FATAL_ERROR
            "AVEMOTION_BUILD_WIN32_PREVIEW requires a Windows host toolchain.")
    endif()
    # CMake's MSVC manifest merge uses the resource compiler to embed the
    # single merged manifest. Enable RC even though no standalone .rc source is
    # compiled by the preview target.
    enable_language(RC)
    if(NOT TARGET avemotion_backend_d2d)
        message(FATAL_ERROR
            "The Win32 preview requires AVEMOTION_BUILD_DIRECT2D_BACKEND=ON.")
    endif()
    if(NOT AVEMOTION_RLOTTIE_AVAILABLE
       OR NOT AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
        message(FATAL_ERROR
            "The Part 20 Win32 preview currently requires the pinned Telegram rlottie adapter.")
    endif()

    set(AVEMOTION_WIN32_PREVIEW_MANIFEST
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/win32_preview/AveMotionPreview.manifest")
    set(AVEMOTION_WIN32_PREVIEW_LEGACY_RC
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/win32_preview/AveMotionPreview.rc")
    if(NOT EXISTS "${AVEMOTION_WIN32_PREVIEW_MANIFEST}")
        message(FATAL_ERROR "The Win32 preview application manifest is missing.")
    endif()
    if(EXISTS "${AVEMOTION_WIN32_PREVIEW_LEGACY_RC}")
        message(FATAL_ERROR
            "AveMotionPreview.rc must not embed a second RT_MANIFEST resource. "
            "List AveMotionPreview.manifest directly as a target source.")
    endif()

    add_executable(avemotion_win32_preview WIN32
        apps/win32_preview/main.cpp
        apps/win32_preview/PreviewApp.cpp
        apps/win32_preview/Win32Graphics.cpp
        # List the application manifest directly. CMake's MSVC link wrapper
        # merges this input with the linker-generated manifest and embeds a
        # single RT_MANIFEST resource. Wrapping it in a .rc file would create
        # a duplicate resource id 1 and fail with CVT1100/LNK1123.
        apps/win32_preview/AveMotionPreview.manifest)
    target_compile_features(avemotion_win32_preview PRIVATE cxx_std_20)
    target_compile_definitions(avemotion_win32_preview PRIVATE
        UNICODE=1
        _UNICODE=1
        NOMINMAX=1
        WINVER=0x0A00
        _WIN32_WINNT=0x0A00)
    target_include_directories(avemotion_win32_preview PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/apps/win32_preview")
    target_link_libraries(avemotion_win32_preview PRIVATE
        AveMotion::Runtime
        AveMotion::Player
        AveMotion::Evaluation
        AveMotion::Rendering
        AveMotion::Direct2D
        d3d11
        dxgi
        d2d1
        shell32
        user32
        gdi32)
    avemotion_set_warnings(avemotion_win32_preview)
    if(MSVC)
        target_compile_options(avemotion_win32_preview PRIVATE /utf-8)
    endif()

    add_custom_command(TARGET avemotion_win32_preview POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:avemotion_win32_preview>/preview-assets"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/repeater_content_group.json"
            "$<TARGET_FILE_DIR:avemotion_win32_preview>/preview-assets/repeater_content_group.json"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tgs/repeater_content_group.tgs"
            "$<TARGET_FILE_DIR:avemotion_win32_preview>/preview-assets/repeater_content_group.tgs"
        VERBATIM)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Python3 REQUIRED COMPONENTS Interpreter)

    add_test(NAME avemotion.cmake.legacy_subproject
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_legacy_cmake_subproject.py)

    add_test(NAME avemotion.cmake.win32_manifest_wiring
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_win32_manifest_wiring.py
            --build-dir ${CMAKE_BINARY_DIR})

    add_test(NAME avemotion.cmake.player_wiring
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_player_wiring.py)

    add_test(NAME avemotion.cmake.tgs_wiring
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_tgs_wiring.py)

    add_test(NAME avemotion.cmake.validation_wiring
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_validation_wiring.py)

    add_test(NAME avemotion.cmake.corpus_lab_wiring
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_corpus_lab_wiring.py
            --root ${CMAKE_CURRENT_SOURCE_DIR})

    add_test(NAME avemotion.corpus.zip_safety
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_corpus_zip_safety.py)

    add_test(NAME avemotion.validation.tgs_corpus_integrity
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_tgs_compatibility_corpus.py
            --root ${CMAKE_CURRENT_SOURCE_DIR}
            --check)

    add_executable(avemotion_core_tests tests/core_tests.cpp)
    target_link_libraries(avemotion_core_tests PRIVATE AveMotion::Core)
    avemotion_set_warnings(avemotion_core_tests)
    add_test(NAME avemotion.core COMMAND avemotion_core_tests)

    add_executable(avemotion_tgs_tests tests/tgs_tests.cpp)
    target_link_libraries(avemotion_tgs_tests PRIVATE AveMotion::Formats)
    target_compile_definitions(avemotion_tgs_tests PRIVATE
        AVEMOTION_TGS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tgs"
        AVEMOTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
    avemotion_set_warnings(avemotion_tgs_tests)
    add_test(NAME avemotion.formats.tgs COMMAND avemotion_tgs_tests)

    add_executable(avemotion_tgs_runtime_tests tests/tgs_runtime_tests.cpp)
    target_link_libraries(avemotion_tgs_runtime_tests PRIVATE AveMotion::Runtime)
    target_compile_definitions(avemotion_tgs_runtime_tests PRIVATE
        AVEMOTION_TGS_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tgs"
        AVEMOTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures"
        AVEMOTION_TEST_REFERENCE_VARIANT="${AVEMOTION_RLOTTIE_VARIANT}")
    avemotion_set_warnings(avemotion_tgs_runtime_tests)
    add_test(NAME avemotion.runtime.tgs COMMAND avemotion_tgs_runtime_tests)

    if(AVEMOTION_RLOTTIE_AVAILABLE
       AND AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
        add_executable(avemotion_validation_corpus_tests
            tests/validation_corpus_tests.cpp)
        target_link_libraries(avemotion_validation_corpus_tests PRIVATE
            AveMotion::Formats AveMotion::Runtime AveMotion::Validation)
        target_compile_definitions(avemotion_validation_corpus_tests PRIVATE
            AVEMOTION_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
        avemotion_set_warnings(avemotion_validation_corpus_tests)
        add_test(NAME avemotion.validation.compatibility_corpus
            COMMAND avemotion_validation_corpus_tests)
    endif()

    add_executable(avemotion_golden_variance_tests
        tests/golden_variance_tests.cpp)
    target_compile_features(avemotion_golden_variance_tests PRIVATE cxx_std_20)
    target_include_directories(avemotion_golden_variance_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    avemotion_set_warnings(avemotion_golden_variance_tests)
    add_test(NAME avemotion.golden.cross_platform_policy
        COMMAND avemotion_golden_variance_tests)

    add_test(NAME avemotion.golden.source_geometry_policy
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_cross_platform_golden_policy.py)

    add_executable(avemotion_asset_validator_tests
        tests/asset_validator_tests.cpp)
    target_link_libraries(avemotion_asset_validator_tests
        PRIVATE AveMotion::Validation)
    avemotion_set_warnings(avemotion_asset_validator_tests)
    add_test(NAME avemotion.validation.asset
        COMMAND avemotion_asset_validator_tests)

    if(AVEMOTION_BUILD_CORPUS_LAB)
        add_executable(avemotion_corpus_benchmark_metrics_tests
            tests/corpus_benchmark_metrics_tests.cpp)
        target_compile_features(avemotion_corpus_benchmark_metrics_tests PRIVATE cxx_std_20)
        target_include_directories(avemotion_corpus_benchmark_metrics_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/apps/corpus_lab"
            "${CMAKE_CURRENT_SOURCE_DIR}/include")
        avemotion_set_warnings(avemotion_corpus_benchmark_metrics_tests)
        add_test(NAME avemotion.corpus.benchmark_metrics
            COMMAND avemotion_corpus_benchmark_metrics_tests)

        add_executable(avemotion_corpus_analysis_tests
            tests/corpus_analysis_tests.cpp)
        target_include_directories(avemotion_corpus_analysis_tests PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src/lab")
        target_link_libraries(avemotion_corpus_analysis_tests PRIVATE
            avemotion_corpus_analysis)
        avemotion_set_warnings(avemotion_corpus_analysis_tests)
        add_test(NAME avemotion.corpus.analysis
            COMMAND avemotion_corpus_analysis_tests)
    endif()

    if(TARGET avemotion_corpus_lab)
        set(AVEMOTION_CORPUS_LAB_SMOKE_DIR
            "${CMAKE_CURRENT_BINARY_DIR}/corpus-lab-smoke")
        add_test(NAME avemotion.corpus.lab_smoke
            COMMAND avemotion_corpus_lab
                --input ${CMAKE_CURRENT_SOURCE_DIR}/tests/compatibility/tgs
                --output ${AVEMOTION_CORPUS_LAB_SMOKE_DIR}
                --samples 5
                --load-repeats 1
                --cpu-repeats 1
                --render-size 64
                --strict)
        set_tests_properties(avemotion.corpus.lab_smoke PROPERTIES TIMEOUT 300)
        add_test(NAME avemotion.corpus.lab_output
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_corpus_lab_output.py
                --output ${AVEMOTION_CORPUS_LAB_SMOKE_DIR}
                --expected-assets 16)
        set_tests_properties(avemotion.corpus.lab_output PROPERTIES
            DEPENDS avemotion.corpus.lab_smoke)
        add_test(NAME avemotion.corpus.lab_output_safety
            COMMAND ${Python3_EXECUTABLE}
                ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_corpus_lab_output_safety.py
                --root ${CMAKE_CURRENT_SOURCE_DIR}
                --executable $<TARGET_FILE:avemotion_corpus_lab>)
    endif()

    add_executable(avemotion_property_evaluator_tests
        tests/property_evaluator_tests.cpp)
    target_link_libraries(avemotion_property_evaluator_tests
        PRIVATE AveMotion::Evaluation)
    avemotion_set_warnings(avemotion_property_evaluator_tests)
    add_test(NAME avemotion.evaluation.properties
        COMMAND avemotion_property_evaluator_tests)

    add_executable(avemotion_render_plan_tests tests/render_plan_tests.cpp)
    target_link_libraries(avemotion_render_plan_tests PRIVATE AveMotion::Rendering)
    avemotion_set_warnings(avemotion_render_plan_tests)
    add_test(NAME avemotion.render.plan COMMAND avemotion_render_plan_tests)

    add_executable(avemotion_polystar_geometry_tests
        tests/polystar_geometry_tests.cpp)
    target_include_directories(avemotion_polystar_geometry_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render")
    target_link_libraries(avemotion_polystar_geometry_tests
        PRIVATE AveMotion::Rendering)
    avemotion_set_warnings(avemotion_polystar_geometry_tests)
    add_test(NAME avemotion.render.polystar_geometry
        COMMAND avemotion_polystar_geometry_tests)

    add_executable(avemotion_repeater_transform_tests
        tests/repeater_transform_tests.cpp)
    target_include_directories(avemotion_repeater_transform_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render")
    target_link_libraries(avemotion_repeater_transform_tests
        PRIVATE AveMotion::Rendering)
    avemotion_set_warnings(avemotion_repeater_transform_tests)
    add_test(NAME avemotion.render.repeater_transform
        COMMAND avemotion_repeater_transform_tests)

    add_executable(avemotion_trim_path_tests
        tests/trim_path_tests.cpp)
    target_include_directories(avemotion_trim_path_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/src/render")
    target_link_libraries(avemotion_trim_path_tests
        PRIVATE AveMotion::Rendering)
    avemotion_set_warnings(avemotion_trim_path_tests)
    add_test(NAME avemotion.render.trim_path
        COMMAND avemotion_trim_path_tests)

    add_executable(avemotion_direct2d_capture_metrics_tests
        tests/direct2d_capture_metrics_tests.cpp)
    target_include_directories(avemotion_direct2d_capture_metrics_tests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests")
    target_link_libraries(avemotion_direct2d_capture_metrics_tests
        PRIVATE AveMotion::Core)
    avemotion_set_warnings(avemotion_direct2d_capture_metrics_tests)
    add_test(NAME avemotion.capture.metrics
        COMMAND avemotion_direct2d_capture_metrics_tests)

    add_executable(avemotion_direct2d_header_tests tests/direct2d_header_tests.cpp)
    target_link_libraries(avemotion_direct2d_header_tests PRIVATE AveMotion::Rendering)
    avemotion_set_warnings(avemotion_direct2d_header_tests)
    add_test(NAME avemotion.direct2d.header COMMAND avemotion_direct2d_header_tests)

    if(AVEMOTION_BUILD_DIRECT2D_CONTRACT_TEST)
        add_library(avemotion_backend_d2d_contract STATIC
            backends/direct2d/Direct2DBackend.cpp)
        target_compile_features(avemotion_backend_d2d_contract PUBLIC cxx_std_20)
        target_compile_definitions(avemotion_backend_d2d_contract PRIVATE _WIN32=1)
        target_include_directories(avemotion_backend_d2d_contract
            BEFORE PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/direct2d_contract/include"
            PUBLIC
                $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
        target_link_libraries(avemotion_backend_d2d_contract
            PUBLIC AveMotion::Rendering)
        avemotion_set_warnings(avemotion_backend_d2d_contract)

        add_executable(avemotion_direct2d_contract_tests
            tests/direct2d_contract_tests.cpp)
        target_compile_definitions(avemotion_direct2d_contract_tests PRIVATE _WIN32=1)
        target_include_directories(avemotion_direct2d_contract_tests
            BEFORE PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/direct2d_contract/include")
        target_link_libraries(avemotion_direct2d_contract_tests
            PRIVATE avemotion_backend_d2d_contract)
        avemotion_set_warnings(avemotion_direct2d_contract_tests)
        add_test(NAME avemotion.direct2d.contract
            COMMAND avemotion_direct2d_contract_tests)
    endif()

    if(TARGET avemotion_backend_d2d)
        add_executable(avemotion_direct2d_smoke_tests tests/direct2d_smoke_tests.cpp)
        target_compile_definitions(avemotion_direct2d_smoke_tests PRIVATE
            AVEMOTION_D2D_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/direct2d-artifacts")
        target_link_libraries(avemotion_direct2d_smoke_tests
            PRIVATE AveMotion::Direct2D d3d11)
        avemotion_set_warnings(avemotion_direct2d_smoke_tests)
        add_test(NAME avemotion.direct2d.smoke COMMAND avemotion_direct2d_smoke_tests)

        if(AVEMOTION_BUILD_DIRECT2D_CAPTURE_TEST
           AND AVEMOTION_RLOTTIE_AVAILABLE
           AND AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
            add_executable(avemotion_direct2d_capture_tests
                tests/direct2d_capture_tests.cpp)
            target_include_directories(avemotion_direct2d_capture_tests PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests")
            target_compile_definitions(avemotion_direct2d_capture_tests PRIVATE
                AVEMOTION_CAPTURE_ARTIFACT_DIR="${CMAKE_CURRENT_BINARY_DIR}/direct2d-capture-artifacts"
                AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus"
                AVEMOTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            target_link_libraries(avemotion_direct2d_capture_tests
                PRIVATE AveMotion::Direct2D AveMotion::Evaluation
                        AveMotion::Runtime AveMotion::Reference d3d11)
            avemotion_set_warnings(avemotion_direct2d_capture_tests)
            add_test(NAME avemotion.direct2d.capture
                COMMAND avemotion_direct2d_capture_tests)
            set_tests_properties(avemotion.direct2d.capture PROPERTIES
                TIMEOUT 300)
        endif()
    endif()
    if(TARGET avemotion_win32_preview)
        set(AVEMOTION_WIN32_PREVIEW_ARTIFACT_DIR
            "${CMAKE_CURRENT_BINARY_DIR}/win32-preview-artifacts")
        add_test(NAME avemotion.win32.preview.selftest
            COMMAND avemotion_win32_preview
                --self-test
                --warp
                --asset
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/tgs/repeater_content_group.tgs"
                --artifact-dir
                    "${AVEMOTION_WIN32_PREVIEW_ARTIFACT_DIR}")
        set_tests_properties(avemotion.win32.preview.selftest PROPERTIES
            TIMEOUT 180)
    endif()

    add_test(NAME avemotion.vendor.ordering
        COMMAND ${Python3_EXECUTABLE}
            ${CMAKE_CURRENT_SOURCE_DIR}/scripts/test_vendor_fingerprint_order.py)
    add_test(NAME avemotion.vendor.verify
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/verify_vendor.py)

    if(AVEMOTION_RLOTTIE_AVAILABLE)
        add_executable(avemotion_reference_tests tests/reference_tests.cpp)
        target_link_libraries(avemotion_reference_tests PRIVATE AveMotion::Reference)
        target_compile_definitions(avemotion_reference_tests PRIVATE
            AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")
        avemotion_set_warnings(avemotion_reference_tests)
        add_test(NAME avemotion.reference COMMAND avemotion_reference_tests)

        add_executable(avemotion_runtime_seams_tests tests/runtime_seams_tests.cpp)
        target_link_libraries(avemotion_runtime_seams_tests
            PRIVATE AveMotion::Runtime AveMotion::Reference)
        target_compile_definitions(avemotion_runtime_seams_tests PRIVATE
            AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")
        avemotion_set_warnings(avemotion_runtime_seams_tests)
        add_test(NAME avemotion.runtime.seams COMMAND avemotion_runtime_seams_tests)

        add_executable(avemotion_frame_mapping_tests tests/frame_mapping_tests.cpp)
        target_link_libraries(avemotion_frame_mapping_tests
            PRIVATE AveMotion::Runtime AveMotion::Reference)
        target_compile_definitions(avemotion_frame_mapping_tests PRIVATE
            AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus"
            AVEMOTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
        avemotion_set_warnings(avemotion_frame_mapping_tests)
        add_test(NAME avemotion.runtime.frame_mapping COMMAND avemotion_frame_mapping_tests)

        find_package(Threads REQUIRED)
        add_executable(avemotion_reference_session_tests tests/reference_session_tests.cpp)
        target_link_libraries(avemotion_reference_session_tests
            PRIVATE AveMotion::Runtime AveMotion::Reference Threads::Threads)
        target_compile_definitions(avemotion_reference_session_tests PRIVATE
            AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus"
            AVEMOTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
        avemotion_set_warnings(avemotion_reference_session_tests)
        add_test(NAME avemotion.runtime.reference_sessions COMMAND avemotion_reference_session_tests)

        if(AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
            add_executable(avemotion_recording_lifecycle_tests tests/recording_lifecycle_tests.cpp)
            target_link_libraries(avemotion_recording_lifecycle_tests PRIVATE AveMotion::Runtime rlottie::rlottie Threads::Threads)
            target_include_directories(avemotion_recording_lifecycle_tests PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/runtime" "${AVEMOTION_RLOTTIE_INCLUDE_DIR}")
            target_include_directories(avemotion_recording_lifecycle_tests PRIVATE
                "${AVEMOTION_RLOTTIE_INTERNAL_LOTTIE_DIR}"
                "${AVEMOTION_RLOTTIE_INTERNAL_VECTOR_DIR}"
                "${AVEMOTION_RLOTTIE_INTERNAL_PIXMAN_DIR}"
                "${AVEMOTION_RLOTTIE_GENERATED_INCLUDE_DIR}")
            target_compile_definitions(avemotion_recording_lifecycle_tests PRIVATE
                AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus"
                AVEMOTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            avemotion_set_warnings(avemotion_recording_lifecycle_tests)
            add_test(NAME avemotion.runtime.recording_lifecycle COMMAND avemotion_recording_lifecycle_tests)

            add_executable(avemotion_property_parity_tests
                tests/property_parity_tests.cpp)
            target_include_directories(avemotion_property_parity_tests PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/src/model")
            target_link_libraries(avemotion_property_parity_tests
                PRIVATE AveMotion::Evaluation AveMotion::Runtime AveMotion::Reference)
            target_compile_definitions(avemotion_property_parity_tests PRIVATE
                AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")
            avemotion_set_warnings(avemotion_property_parity_tests)
            add_test(NAME avemotion.evaluation.telegram_parity
                COMMAND avemotion_property_parity_tests)

            add_executable(avemotion_parsed_model_tests tests/parsed_model_tests.cpp)
            target_link_libraries(avemotion_parsed_model_tests
                PRIVATE AveMotion::Runtime AveMotion::Reference)
            target_compile_definitions(avemotion_parsed_model_tests PRIVATE
                AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")
            avemotion_set_warnings(avemotion_parsed_model_tests)
            add_test(NAME avemotion.runtime.parsed_model
                COMMAND avemotion_parsed_model_tests)

            add_executable(avemotion_direct2d_capture_preflight_tests
                tests/direct2d_capture_preflight_tests.cpp)
            target_include_directories(avemotion_direct2d_capture_preflight_tests PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests")
            target_link_libraries(avemotion_direct2d_capture_preflight_tests
                PRIVATE AveMotion::Rendering AveMotion::Evaluation
                        AveMotion::Runtime AveMotion::Reference)
            target_compile_definitions(avemotion_direct2d_capture_preflight_tests PRIVATE
                AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus"
                AVEMOTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            avemotion_set_warnings(avemotion_direct2d_capture_preflight_tests)
            add_test(NAME avemotion.direct2d.capture_preflight
                COMMAND avemotion_direct2d_capture_preflight_tests)

            add_executable(avemotion_source_geometry_projection_tests
                tests/source_geometry_projection_tests.cpp)
            target_link_libraries(avemotion_source_geometry_projection_tests
                PRIVATE AveMotion::Rendering AveMotion::Evaluation
                        AveMotion::Runtime AveMotion::Reference)
            target_compile_definitions(avemotion_source_geometry_projection_tests PRIVATE
                AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus"
                AVEMOTION_FIXTURE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures")
            avemotion_set_warnings(avemotion_source_geometry_projection_tests)
            add_test(NAME avemotion.render.source_geometry
                COMMAND avemotion_source_geometry_projection_tests)

            set(AVEMOTION_SOURCE_GEOMETRY_GOLDEN_OUTPUT
                "${CMAKE_CURRENT_BINARY_DIR}/source-geometry-golden")
            add_test(NAME avemotion.render.source_geometry_characterize
                COMMAND avemotion_source_geometry_characterize
                    --output ${AVEMOTION_SOURCE_GEOMETRY_GOLDEN_OUTPUT}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/1667-firework.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/ModernPictogramsForLottie_LoudMute.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/StickAndBall.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/dynamic_path_test.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/gradient_animated_background.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/mask.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/matte_two_item_with_lowerlayer.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/polystar_line_clockwise_trim.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/primitive_geometry.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/polystar_polygon_geometry.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/trim_path_geometry.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/multi_trim_path_geometry.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/repeater_geometry.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/fixtures/repeater_content_group.json)
            set(AVEMOTION_SOURCE_GEOMETRY_GOLDEN_COMPARE_ARGS
                --expected ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/source-geometry-telegram.tsv
                --actual ${AVEMOTION_SOURCE_GEOMETRY_GOLDEN_OUTPUT}/source_geometry_manifest.tsv)
            if(MSVC)
                list(APPEND AVEMOTION_SOURCE_GEOMETRY_GOLDEN_COMPARE_ARGS
                    --allow-telegram-msvc-raw-float-variance)
            endif()
            add_test(NAME avemotion.render.source_geometry_golden
                COMMAND ${Python3_EXECUTABLE}
                    ${CMAKE_CURRENT_SOURCE_DIR}/scripts/compare_source_geometry_golden.py
                    ${AVEMOTION_SOURCE_GEOMETRY_GOLDEN_COMPARE_ARGS})
            set_tests_properties(avemotion.render.source_geometry_golden PROPERTIES
                DEPENDS avemotion.render.source_geometry_characterize)
        endif()

        add_executable(avemotion_asset_model_tests tests/asset_model_tests.cpp)
        target_link_libraries(avemotion_asset_model_tests
            PRIVATE AveMotion::Runtime AveMotion::Reference)
        target_compile_definitions(avemotion_asset_model_tests PRIVATE
            AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")
        avemotion_set_warnings(avemotion_asset_model_tests)
        add_test(NAME avemotion.runtime.asset_model
            COMMAND avemotion_asset_model_tests)

        add_executable(avemotion_playback_tests tests/playback_tests.cpp)
        target_link_libraries(avemotion_playback_tests
            PRIVATE AveMotion::Runtime AveMotion::Reference)
        target_compile_definitions(avemotion_playback_tests PRIVATE
            AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")
        avemotion_set_warnings(avemotion_playback_tests)
        add_test(NAME avemotion.runtime.playback
            COMMAND avemotion_playback_tests)

        add_executable(avemotion_player_tests tests/player_tests.cpp)
        target_link_libraries(avemotion_player_tests
            PRIVATE AveMotion::Player)
        target_compile_definitions(avemotion_player_tests PRIVATE
            AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")
        avemotion_set_warnings(avemotion_player_tests)
        add_test(NAME avemotion.player.scheduler
            COMMAND avemotion_player_tests)

        if(AVEMOTION_RLOTTIE_VARIANT STREQUAL "telegram")
            add_executable(avemotion_model_render_plan_tests
                tests/model_render_plan_tests.cpp)
            target_link_libraries(avemotion_model_render_plan_tests
                PRIVATE AveMotion::Rendering AveMotion::Reference)
            target_compile_definitions(avemotion_model_render_plan_tests PRIVATE
                AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")
            avemotion_set_warnings(avemotion_model_render_plan_tests)
            add_test(NAME avemotion.render.model_plan
                COMMAND avemotion_model_render_plan_tests)
        endif()

        add_executable(avemotion_canonical_geometry_tests
            tests/canonical_geometry_tests.cpp)
        target_link_libraries(avemotion_canonical_geometry_tests
            PRIVATE AveMotion::Rendering AveMotion::Reference)
        target_compile_definitions(avemotion_canonical_geometry_tests PRIVATE
            AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")
        avemotion_set_warnings(avemotion_canonical_geometry_tests)
        add_test(NAME avemotion.runtime.canonical
            COMMAND avemotion_canonical_geometry_tests)

        add_executable(avemotion_manifest_tests tests/manifest_tests.cpp)
        target_link_libraries(avemotion_manifest_tests PRIVATE AveMotion::Reference)
        avemotion_set_warnings(avemotion_manifest_tests)
        add_test(NAME avemotion.vendor.manifest COMMAND avemotion_manifest_tests)

        if(AVEMOTION_BUILD_CHARACTERIZER)
            add_test(NAME avemotion.characterize.smoke
                COMMAND avemotion_characterize
                    --output ${CMAKE_CURRENT_BINARY_DIR}/characterize-smoke
                    --size 48
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/StickAndBall.json)
        endif()

        if(AVEMOTION_BUILD_SCENE_CHARACTERIZER)
            add_test(NAME avemotion.scene.characterize.smoke
                COMMAND avemotion_scene_characterize
                    --output ${CMAKE_CURRENT_BINARY_DIR}/scene-characterize-smoke
                    --size 48
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/StickAndBall.json)
        endif()

        set(AVEMOTION_SCENE_GOLDEN_FILE
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/scene-${AVEMOTION_RLOTTIE_VARIANT}.tsv")
        if(EXISTS "${AVEMOTION_SCENE_GOLDEN_FILE}")
            add_executable(avemotion_scene_golden_tests tests/scene_golden_tests.cpp)
            target_link_libraries(avemotion_scene_golden_tests
                PRIVATE AveMotion::Runtime AveMotion::Reference)
            target_include_directories(avemotion_scene_golden_tests PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests")
            target_compile_definitions(avemotion_scene_golden_tests PRIVATE
                AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus"
                AVEMOTION_SCENE_GOLDEN_FILE="${AVEMOTION_SCENE_GOLDEN_FILE}")
            avemotion_set_warnings(avemotion_scene_golden_tests)
            add_test(NAME avemotion.scene.golden COMMAND avemotion_scene_golden_tests)
        endif()

        if(AVEMOTION_BUILD_PLAN_CHARACTERIZER)
            add_test(NAME avemotion.plan.characterize.smoke
                COMMAND avemotion_plan_characterize
                    --output ${CMAKE_CURRENT_BINARY_DIR}/plan-characterize-smoke
                    --size 48
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/StickAndBall.json)
        endif()

        if(TARGET avemotion_model_characterize)
            set(AVEMOTION_MODEL_GOLDEN_OUTPUT
                "${CMAKE_CURRENT_BINARY_DIR}/model-characterize-golden")
            add_test(NAME avemotion.model.characterize
                COMMAND avemotion_model_characterize
                    --output ${AVEMOTION_MODEL_GOLDEN_OUTPUT}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/1667-firework.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/ModernPictogramsForLottie_LoudMute.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/StickAndBall.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/dynamic_path_test.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/gradient_animated_background.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/mask.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/matte_two_item_with_lowerlayer.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/polystar_line_clockwise_trim.json)
            add_test(NAME avemotion.model.golden
                COMMAND ${CMAKE_COMMAND} -E compare_files
                    ${AVEMOTION_MODEL_GOLDEN_OUTPUT}/model_manifest.tsv
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/model-telegram.tsv)
            set_tests_properties(avemotion.model.golden PROPERTIES
                DEPENDS avemotion.model.characterize)
            add_test(NAME avemotion.model.parsed_golden
                COMMAND ${CMAKE_COMMAND} -E compare_files
                    ${AVEMOTION_MODEL_GOLDEN_OUTPUT}/parsed_model_details.tsv
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/parsed-model-telegram.tsv)
            set_tests_properties(avemotion.model.parsed_golden PROPERTIES
                DEPENDS avemotion.model.characterize)
        endif()

        if(TARGET avemotion_property_characterize)
            set(AVEMOTION_PROPERTY_GOLDEN_OUTPUT
                "${CMAKE_CURRENT_BINARY_DIR}/property-characterize-golden")
            add_test(NAME avemotion.evaluation.characterize
                COMMAND avemotion_property_characterize
                    --output ${AVEMOTION_PROPERTY_GOLDEN_OUTPUT}
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/1667-firework.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/ModernPictogramsForLottie_LoudMute.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/StickAndBall.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/dynamic_path_test.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/gradient_animated_background.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/mask.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/matte_two_item_with_lowerlayer.json
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus/polystar_line_clockwise_trim.json)
            add_test(NAME avemotion.evaluation.golden
                COMMAND ${CMAKE_COMMAND} -E compare_files
                    ${AVEMOTION_PROPERTY_GOLDEN_OUTPUT}/property_manifest.tsv
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/property-telegram.tsv)
            set_tests_properties(avemotion.evaluation.golden PROPERTIES
                DEPENDS avemotion.evaluation.characterize)
        endif()

        if(TARGET avemotion_validation_characterize)
            set(AVEMOTION_VALIDATION_GOLDEN_OUTPUT
                "${CMAKE_CURRENT_BINARY_DIR}/validation-characterize-golden")
            add_test(NAME avemotion.validation.characterize
                COMMAND avemotion_validation_characterize
                    --root ${CMAKE_CURRENT_SOURCE_DIR}
                    --manifest ${CMAKE_CURRENT_SOURCE_DIR}/tests/compatibility/manifest.tsv
                    --output ${AVEMOTION_VALIDATION_GOLDEN_OUTPUT})
            add_test(NAME avemotion.validation.golden
                COMMAND ${CMAKE_COMMAND} -E compare_files
                    ${AVEMOTION_VALIDATION_GOLDEN_OUTPUT}/validation_manifest.tsv
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/validation-telegram.tsv)
            set_tests_properties(avemotion.validation.golden PROPERTIES
                DEPENDS avemotion.validation.characterize)
            add_test(NAME avemotion.validation.feature_golden
                COMMAND ${CMAKE_COMMAND} -E compare_files
                    ${AVEMOTION_VALIDATION_GOLDEN_OUTPUT}/validation_features.tsv
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/validation-features-telegram.tsv)
            add_test(NAME avemotion.validation.summary_golden
                COMMAND ${CMAKE_COMMAND} -E compare_files
                    ${AVEMOTION_VALIDATION_GOLDEN_OUTPUT}/validation_summary.txt
                    ${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/validation-summary-telegram.txt)
            set_tests_properties(
                avemotion.validation.feature_golden
                avemotion.validation.summary_golden
                PROPERTIES DEPENDS avemotion.validation.characterize)
        endif()

        set(AVEMOTION_PLAN_GOLDEN_FILE
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/golden/plan-${AVEMOTION_RLOTTIE_VARIANT}.tsv")
        if(EXISTS "${AVEMOTION_PLAN_GOLDEN_FILE}")
            add_executable(avemotion_plan_golden_tests tests/plan_golden_tests.cpp)
            target_link_libraries(avemotion_plan_golden_tests
                PRIVATE AveMotion::Rendering AveMotion::Reference)
            target_include_directories(avemotion_plan_golden_tests PRIVATE
                "${CMAKE_CURRENT_SOURCE_DIR}/tests")
            target_compile_definitions(avemotion_plan_golden_tests PRIVATE
                AVEMOTION_CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus"
                AVEMOTION_PLAN_GOLDEN_FILE="${AVEMOTION_PLAN_GOLDEN_FILE}")
            avemotion_set_warnings(avemotion_plan_golden_tests)
            add_test(NAME avemotion.plan.golden COMMAND avemotion_plan_golden_tests)
        endif()
    else()
        add_executable(avemotion_offline_tests tests/offline_tests.cpp)
        target_link_libraries(avemotion_offline_tests PRIVATE AveMotion::Reference)
        avemotion_set_warnings(avemotion_offline_tests)
        add_test(NAME avemotion.offline COMMAND avemotion_offline_tests)

        add_executable(avemotion_runtime_offline_tests tests/runtime_offline_tests.cpp)
        target_link_libraries(avemotion_runtime_offline_tests PRIVATE AveMotion::Runtime)
        avemotion_set_warnings(avemotion_runtime_offline_tests)
        add_test(NAME avemotion.runtime.offline COMMAND avemotion_runtime_offline_tests)
    endif()
endif()

if(AVEMOTION_ENABLE_INSTALL)
    if(AVEMOTION_RLOTTIE_AVAILABLE)
        message(FATAL_ERROR
            "Installation of targets linked to the rlottie reference engine is intentionally disabled while the rlottie reference engine is linked. "
            "Configure with AVEMOTION_RLOTTIE_VARIANT=none or AVEMOTION_ENABLE_INSTALL=OFF.")
    endif()

    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)
    configure_package_config_file(
        cmake/AveMotionConfig.cmake.in
        "${CMAKE_CURRENT_BINARY_DIR}/AveMotionConfig.cmake"
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AveMotion)
    write_basic_package_version_file(
        "${CMAKE_CURRENT_BINARY_DIR}/AveMotionConfigVersion.cmake"
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion)
    install(TARGETS avemotion_formats avemotion_core avemotion_model avemotion_validation avemotion_evaluation avemotion_reference avemotion_runtime avemotion_player avemotion_rendering
        EXPORT AveMotionTargets
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    if(TARGET avemotion_backend_d2d)
        install(TARGETS avemotion_backend_d2d
            EXPORT AveMotionTargets
            ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
            LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
    endif()
    install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
    install(EXPORT AveMotionTargets
        FILE AveMotionTargets.cmake
        NAMESPACE AveMotion::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AveMotion)
    install(FILES
        "${CMAKE_CURRENT_BINARY_DIR}/AveMotionConfig.cmake"
        "${CMAKE_CURRENT_BINARY_DIR}/AveMotionConfigVersion.cmake"
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/AveMotion)
endif()
