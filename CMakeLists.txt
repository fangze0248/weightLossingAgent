cmake_minimum_required(VERSION 3.16)

project(weightLossingAgent VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets Sql)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets Sql)

add_library(persistence_core STATIC
    database/databasemanager.h
    database/databasemanager.cpp
    database/modeljsoncodec.h
    database/modeljsoncodec.cpp
    src/repositories/sqliteuserrepository.h
    src/repositories/sqliteuserrepository.cpp
    src/repositories/sqliteexerciserepository.h
    src/repositories/sqliteexerciserepository.cpp
    src/repositories/sqlitereciperepository.h
    src/repositories/sqlitereciperepository.cpp
    src/repositories/sqliteplanrepository.h
    src/repositories/sqliteplanrepository.cpp
    src/repositories/sqlitefeedbackrepository.h
    src/repositories/sqlitefeedbackrepository.cpp
)

target_include_directories(persistence_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(persistence_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)

target_compile_features(persistence_core PUBLIC cxx_std_17)

add_library(recommendation_core STATIC
    src/recommendation/recommendationcore.h
    src/recommendation/recommendationcore.cpp
    src/recommendation/healthcalculator.h
    src/recommendation/healthcalculator.cpp
    src/recommendation/ExerciseRecommender.h
    src/recommendation/ExerciseRecommender.cpp
    src/recommendation/MealRecommender.h
    src/recommendation/MealRecommender.cpp
    src/recommendation/WeeklyPlanner.h
    src/recommendation/WeeklyPlanner.cpp
)

target_include_directories(recommendation_core PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(recommendation_core PUBLIC
    Qt${QT_VERSION_MAJOR}::Core
)

target_compile_features(recommendation_core PUBLIC
    cxx_std_17
)

set(PROJECT_SOURCES
        main.cpp
        mainwindow.cpp
        mainwindow.h
        src/application/plangenerationservice.cpp
        src/application/plangenerationservice.h
        src/interfaces/IPlanGenerationService.h
        src/session/sessionmanager.cpp
        src/session/sessionmanager.h
        src/ui/appstyle.cpp
        src/ui/appstyle.h
        src/ui/dashboardpage.cpp
        src/ui/dashboardpage.h
        src/ui/exercisepage.cpp
        src/ui/exercisepage.h
        src/ui/healthpage.cpp
        src/ui/healthpage.h
        src/ui/loginpage.cpp
        src/ui/loginpage.h
        src/ui/planpage.cpp
        src/ui/planpage.h
        src/ui/profiledialog.cpp
        src/ui/profiledialog.h
        src/ui/recipepage.cpp
        src/ui/recipepage.h
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(weightLossingAgent
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET weightLossingAgent APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(weightLossingAgent SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(weightLossingAgent
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_include_directories(weightLossingAgent PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
)

target_link_libraries(weightLossingAgent PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Sql
    persistence_core
    recommendation_core
)

# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.weightLossingAgent)
endif()
set_target_properties(weightLossingAgent PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS weightLossingAgent
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(weightLossingAgent)
endif()

enable_testing()

function(add_recommendation_test target_name source_file)
    add_executable(${target_name}
        ${source_file}
    )

    target_link_libraries(${target_name} PRIVATE
        recommendation_core
        Qt${QT_VERSION_MAJOR}::Core
    )

    if(WIN32)
        add_test(
            NAME ${target_name}
            COMMAND ${CMAKE_COMMAND} -E env
                "PATH=$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$ENV{PATH}"
                $<TARGET_FILE:${target_name}>
        )
    else()
        add_test(
            NAME ${target_name}
            COMMAND $<TARGET_FILE:${target_name}>
        )
    endif()
endfunction()

add_recommendation_test(
    contracts_compile_test
    tests/contracts_compile_test.cpp
)

add_recommendation_test(
    exercise_recommender_test
    tests/exercise_recommender_test.cpp
)

add_recommendation_test(
    meal_recommender_test
    tests/meal_recommender_test.cpp
)

add_recommendation_test(
    weekly_planner_test
    tests/weekly_planner_test.cpp
)

add_executable(health_calculator_test
    tests/health_calculator_test.cpp
)

target_link_libraries(health_calculator_test PRIVATE
    recommendation_core
    Qt${QT_VERSION_MAJOR}::Core
)

add_test(
    NAME health_calculator_test
    COMMAND health_calculator_test
)

add_executable(sqlite_repository_test
    tests/sqlite_repository_test.cpp
)

target_link_libraries(sqlite_repository_test PRIVATE
    persistence_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)

if(WIN32)
    add_test(
        NAME sqlite_repository_test
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$ENV{PATH}"
            $<TARGET_FILE:sqlite_repository_test>
    )
else()
    add_test(
        NAME sqlite_repository_test
        COMMAND $<TARGET_FILE:sqlite_repository_test>
    )
endif()

add_executable(recommendation_persistence_integration_test
    tests/recommendation_persistence_integration_test.cpp
)

target_link_libraries(recommendation_persistence_integration_test PRIVATE
    persistence_core
    recommendation_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)

if(WIN32)
    add_test(
        NAME recommendation_persistence_integration_test
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$ENV{PATH}"
            $<TARGET_FILE:recommendation_persistence_integration_test>
    )
else()
    add_test(
        NAME recommendation_persistence_integration_test
        COMMAND $<TARGET_FILE:recommendation_persistence_integration_test>
    )
endif()

add_executable(plan_generation_service_test
    tests/plan_generation_service_test.cpp
    src/application/plangenerationservice.cpp
)

target_include_directories(plan_generation_service_test PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/src"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_link_libraries(plan_generation_service_test PRIVATE
    persistence_core
    recommendation_core
    Qt${QT_VERSION_MAJOR}::Core
    Qt${QT_VERSION_MAJOR}::Sql
)

if(WIN32)
    add_test(
        NAME plan_generation_service_test
        COMMAND ${CMAKE_COMMAND} -E env
            "PATH=$<TARGET_FILE_DIR:Qt${QT_VERSION_MAJOR}::Core>;$ENV{PATH}"
            $<TARGET_FILE:plan_generation_service_test>
    )
else()
    add_test(
        NAME plan_generation_service_test
        COMMAND $<TARGET_FILE:plan_generation_service_test>
    )
endif()
