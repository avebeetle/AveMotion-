option(AVEMOTION_BUILD_PROBE "Build the reference metadata/render probe" ON)
option(AVEMOTION_BUILD_VALIDATOR_TOOL
    "Build the AveMotion asset validator command-line tool"
    ON)
option(AVEMOTION_BUILD_VALIDATION_CHARACTERIZER
    "Build the TGS compatibility/validation characterizer"
    ON)
option(AVEMOTION_BUILD_CORPUS_LAB
    "Build the private/external TGS corpus analysis laboratory"
    ON)
option(AVEMOTION_BUILD_CHARACTERIZER "Build the CPU corpus characterizer" ON)
option(AVEMOTION_BUILD_SCENE_CHARACTERIZER "Build the evaluated-scene characterizer" ON)
option(AVEMOTION_BUILD_PLAN_CHARACTERIZER "Build the render-plan characterizer" ON)
option(AVEMOTION_BUILD_MODEL_CHARACTERIZER "Build the immutable asset-model characterizer" ON)
option(AVEMOTION_BUILD_PROPERTY_CHARACTERIZER "Build the canonical property evaluator characterizer" ON)
option(AVEMOTION_BUILD_SOURCE_GEOMETRY_CHARACTERIZER
    "Build the Telegram source-geometry characterizer" ON)
option(AVEMOTION_BUILD_DIRECT2D_BACKEND "Build the Windows Direct2D backend" ON)
option(AVEMOTION_BUILD_DIRECT2D_CONTRACT_TEST
    "Build the portable Direct2D backend contract test with a deterministic SDK shim"
    ON)
option(AVEMOTION_BUILD_DIRECT2D_CAPTURE_TEST
    "Build the native Windows Direct2D capture corpus against Telegram CPU frames"
    OFF)
option(AVEMOTION_BUILD_WIN32_PREVIEW
    "Build the live Windows Win32/D3D11/Direct2D preview host"
    OFF)
option(AVEMOTION_ENABLE_INSTALL "Enable installation of AveMotion-owned targets" OFF)

set(AVEMOTION_RLOTTIE_VARIANT "telegram" CACHE STRING
    "Reference implementation: samsung, telegram, or none")
set_property(CACHE AVEMOTION_RLOTTIE_VARIANT PROPERTY STRINGS samsung telegram none)
