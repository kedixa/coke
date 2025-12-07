# Find workflow library and create target coke-workflow-static,
# coke-workflow-shared.

if (NOT TARGET coke-workflow-static)
    find_library(
        COKE_STATIC_WORKFLOW_LIBRARY
        NAMES
            libworkflow.a libworkflow.lib
        PATHS ${WORKFLOW_LIB_DIR}
    )

    if (COKE_STATIC_WORKFLOW_LIBRARY)
        add_library(coke-workflow-static STATIC IMPORTED GLOBAL)
        set_target_properties(coke-workflow-static
            PROPERTIES
                IMPORTED_LOCATION ${COKE_STATIC_WORKFLOW_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${WORKFLOW_INCLUDE_DIR}
        )

        message("Find static Workflow: " ${COKE_STATIC_WORKFLOW_LIBRARY})
    endif ()
endif ()

if (NOT TARGET coke-workflow-shared)
    find_library(
        COKE_SHARED_WORKFLOW_LIBRARY
        NAMES
            libworkflow.so libworkflow.dylib libworkflow.dll
        PATHS ${WORKFLOW_LIB_DIR}
    )

    if (COKE_SHARED_WORKFLOW_LIBRARY)
        add_library(coke-workflow-shared SHARED IMPORTED GLOBAL)
        set_target_properties(coke-workflow-shared
            PROPERTIES
                IMPORTED_LOCATION ${COKE_SHARED_WORKFLOW_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${WORKFLOW_INCLUDE_DIR}
        )

        message("Find shared Workflow: " ${COKE_SHARED_WORKFLOW_LIBRARY})
    endif ()
endif ()

if (NOT TARGET coke-workflow)
    get_target_property(coke_target_type coke::coke TYPE)
    if (coke_target_type STREQUAL "STATIC_LIBRARY")
        set(coke_is_static TRUE)
    else ()
        set(coke_is_static FALSE)
    endif ()

    if (TARGET coke-workflow-static AND coke_is_static)
        add_library(coke-workflow ALIAS coke-workflow-static)
    elseif (TARGET coke-workflow-shared)
        add_library(coke-workflow ALIAS coke-workflow-shared)
    else ()
        message(FATAL_ERROR "Workflow Not Found")
    endif ()
endif ()
