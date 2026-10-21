find_package(GTest CONFIG REQUIRED)

add_executable(
  moa_sky_tests
  test_EvilNight.cpp
  test_MoonPlacement.cpp
  test_SkyRules.cpp
  test_SkyTheme.cpp
  test_SkyUniforms.cpp
  test_SkyboxSettings.cpp
)
target_link_libraries(moa_sky_tests PRIVATE moa_sky_core GTest::gtest_main)
# The star table of the Unity project: our own data from the Yale Bright Star Catalogue, not game data.
target_compile_definitions(moa_sky_tests PRIVATE MOA_STAR_TABLE="${PROJECT_SOURCE_DIR}/unity/moa_sky/Data/stars_bsc5.bin")
moa_set_warnings(moa_sky_tests)

include(GoogleTest)
gtest_discover_tests(moa_sky_tests DISCOVERY_MODE PRE_TEST)
