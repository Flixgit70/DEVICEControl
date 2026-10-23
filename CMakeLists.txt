cmake_minimum_required(VERSION 3.22)
project(DEVICEControl VERSION 0.7.12 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

include(FetchContent)

# -----------------------------------------------------------------------------
# Reproducible dependencies
# -----------------------------------------------------------------------------
# Local developer checkouts are supported, but CI and fresh clones work without
# machine-specific paths. If JUCE_DIR / EMBERPLUS_DIR are unset, pinned upstream
# releases are fetched automatically.
set(DEVICECONTROL_JUCE_TAG "8.0.15" CACHE STRING "Pinned JUCE release/tag")
set(DEVICECONTROL_EMBERPLUS_TAG "v1.8.2" CACHE STRING "Pinned Lawo ember-plus release/tag")
set(JUCE_DIR "" CACHE PATH "Optional path to a local JUCE checkout")
set(EMBERPLUS_DIR "" CACHE PATH "Optional path to a local Lawo ember-plus checkout")

if(NOT JUCE_DIR AND DEFINED ENV{JUCE_DIR})
    set(JUCE_DIR "$ENV{JUCE_DIR}" CACHE PATH "Optional path to a local JUCE checkout" FORCE)
endif()

if(NOT EMBERPLUS_DIR AND DEFINED ENV{EMBERPLUS_DIR})
    set(EMBERPLUS_DIR "$ENV{EMBERPLUS_DIR}" CACHE PATH "Optional path to a local Lawo ember-plus checkout" FORCE)
endif()

if(JUCE_DIR)
    if(NOT EXISTS "${JUCE_DIR}/CMakeLists.txt")
        message(FATAL_ERROR "JUCE_DIR is set to '${JUCE_DIR}', but no CMakeLists.txt exists there")
    endif()
    message(STATUS "DEVICEControl: using local JUCE at ${JUCE_DIR}")
    add_subdirectory("${JUCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/juce-build")
else()
    message(STATUS "DEVICEControl: fetching JUCE ${DEVICECONTROL_JUCE_TAG}")
    FetchContent_Declare(
        juce
        GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
        GIT_TAG "${DEVICECONTROL_JUCE_TAG}"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(juce)
endif()

if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

if(EMBERPLUS_DIR)
    if(NOT EXISTS "${EMBERPLUS_DIR}/libember/CMakeLists.txt")
        message(FATAL_ERROR "EMBERPLUS_DIR is set to '${EMBERPLUS_DIR}', but libember/CMakeLists.txt was not found")
    endif()
    message(STATUS "DEVICEControl: using local ember-plus at ${EMBERPLUS_DIR}")
    set(emberplus_SOURCE_DIR "${EMBERPLUS_DIR}")
    set(emberplus_BINARY_DIR "${CMAKE_BINARY_DIR}/_deps/emberplus-build")
else()
    message(STATUS "DEVICEControl: fetching ember-plus ${DEVICECONTROL_EMBERPLUS_TAG}")
    FetchContent_Declare(
        emberplus
        GIT_REPOSITORY https://github.com/Lawo/ember-plus.git
        GIT_TAG "${DEVICECONTROL_EMBERPLUS_TAG}"
        GIT_SHALLOW TRUE
        GIT_PROGRESS TRUE
    )
    FetchContent_GetProperties(emberplus)
    if(NOT emberplus_POPULATED)
        FetchContent_Populate(emberplus)
    endif()
endif()

# Build only the protocol pieces DEVICEControl needs; avoid TinyEmber/Qt.
add_subdirectory("${emberplus_SOURCE_DIR}/libs101" "${emberplus_BINARY_DIR}/libs101" EXCLUDE_FROM_ALL)
add_subdirectory("${emberplus_SOURCE_DIR}/libember" "${emberplus_BINARY_DIR}/libember" EXCLUDE_FROM_ALL)

# libember 1.8.2 still uses std::auto_ptr in its DOM implementation.
# Compile only the legacy Lawo implementation targets as C++14; DEVICEControl
# itself stays on C++17.
set_target_properties(ember-static ember-shared PROPERTIES
    CXX_STANDARD 14
    CXX_STANDARD_REQUIRED ON
    CXX_EXTENSIONS OFF
)

# -----------------------------------------------------------------------------
# Application
# -----------------------------------------------------------------------------
juce_add_gui_app(DEVICEControl
    PRODUCT_NAME "DEVICEControl"
    COMPANY_NAME "K2Consulting"
    BUNDLE_ID "de.k2consulting.devicecontrol"
    ICON_BIG "${CMAKE_CURRENT_SOURCE_DIR}/Data/AppIcon.png"
)
juce_generate_juce_header(DEVICEControl)

juce_add_binary_data(DeviceControlAssets
    SOURCES
        "${CMAKE_CURRENT_SOURCE_DIR}/Data/K2_Logo.png"
)

file(GLOB_RECURSE DEVICECONTROL_SOURCES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/Source/*.h"
)

target_sources(DEVICEControl PRIVATE ${DEVICECONTROL_SOURCES})
target_include_directories(DEVICEControl PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/Source")

target_compile_definitions(DEVICEControl PRIVATE
    JUCE_WEB_BROWSER=0
    JUCE_USE_CURL=0
    JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:DEVICEControl,JUCE_PRODUCT_NAME>"
    JUCE_APPLICATION_VERSION_STRING="${PROJECT_VERSION}"
)

target_link_libraries(DEVICEControl PRIVATE
    DeviceControlAssets
    juce::juce_gui_extra
    juce::juce_data_structures
    libember::ember-static
    libs101::s101
)

if(MSVC)
    target_compile_options(DEVICEControl PRIVATE /W4 /permissive-)
else()
    target_compile_options(DEVICEControl PRIVATE -Wall -Wextra -Wpedantic)
endif()

# -----------------------------------------------------------------------------
# Regression tests
# -----------------------------------------------------------------------------
include(CTest)
if(BUILD_TESTING)
    juce_add_console_app(DEVICEControlRegressionTests
        PRODUCT_NAME "DEVICEControl Regression Tests"
    )
    juce_generate_juce_header(DEVICEControlRegressionTests)

    target_sources(DEVICEControlRegressionTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/RegressionTests.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/model/PageModel.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/model/ParameterStore.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/model/ControlTreeModel.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/model/MappingDraft.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/services/CsvService.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/Source/services/ProjectService.cpp"
    )
    target_include_directories(DEVICEControlRegressionTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/Source"
    )
    target_compile_definitions(DEVICEControlRegressionTests PRIVATE
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
    )
    target_link_libraries(DEVICEControlRegressionTests PRIVATE
        juce::juce_core
        juce::juce_data_structures
    )

    if(MSVC)
        target_compile_options(DEVICEControlRegressionTests PRIVATE /W4 /permissive-)
    else()
        target_compile_options(DEVICEControlRegressionTests PRIVATE -Wall -Wextra -Wpedantic)
    endif()

    add_test(NAME DEVICEControl.ModelCsvProjectRegression
             COMMAND DEVICEControlRegressionTests)
    set_tests_properties(DEVICEControl.ModelCsvProjectRegression PROPERTIES TIMEOUT 30)

    add_executable(DEVICEControlEmberSdkContractTests
        "${CMAKE_CURRENT_SOURCE_DIR}/tests/EmberSdkContractTests.cpp"
    )
    target_link_libraries(DEVICEControlEmberSdkContractTests PRIVATE libember::ember-static libs101::s101)
    target_compile_features(DEVICEControlEmberSdkContractTests PRIVATE cxx_std_17)
    add_test(NAME DEVICEControl.EmberSdkContract
             COMMAND DEVICEControlEmberSdkContractTests)
    set_tests_properties(DEVICEControl.EmberSdkContract PROPERTIES TIMEOUT 30)
endif()
