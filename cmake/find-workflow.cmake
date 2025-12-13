# Find workflow library and create coke::workflow-static and/or
# coke::workflow-shared.

if (TARGET coke::workflow-static OR TARGET coke::workflow-shared)
    # Target has been created, nothing todo.
elseif (TARGET workflow-static OR TARGET workflow-shared)
    # Maybe in subdirectory mode, create alias.

    if (TARGET workflow-static)
        add_library(coke::workflow-static ALIAS workflow-static)
    endif ()

    if (TARGET workflow-shared)
        add_library(coke::workflow-shared ALIAS workflow-shared)
    endif ()
else ()
    # In config mode, find workflow and create target.

    if (NOT WORKFLOW_INCLUDE_DIR OR NOT WORKFLOW_LIB_DIR)
        find_package(workflow REQUIRED)
    endif ()

    if (NOT TARGET OpenSSL::SSL OR NOT TARGET OpenSSL::Crypto)
        find_package(OpenSSL)
    endif ()

    if (NOT TARGET Threads)
        find_package(Threads)
    endif ()

    find_library(
        COKE_STATIC_WORKFLOW_LIBRARY
        NAMES
            libworkflow.a libworkflow.lib
        PATHS ${WORKFLOW_LIB_DIR}
    )

    if (COKE_STATIC_WORKFLOW_LIBRARY)
        add_library(coke::workflow-static STATIC IMPORTED GLOBAL)
        set_target_properties(coke::workflow-static
            PROPERTIES
                IMPORTED_LOCATION ${COKE_STATIC_WORKFLOW_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${WORKFLOW_INCLUDE_DIR}
                INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto;Threads::Threads"
        )

        message("Find static workflow: " ${COKE_STATIC_WORKFLOW_LIBRARY})
    endif ()

    find_library(
        COKE_SHARED_WORKFLOW_LIBRARY
        NAMES
            libworkflow.so libworkflow.dylib libworkflow.dll
        PATHS ${WORKFLOW_LIB_DIR}
    )

    if (COKE_SHARED_WORKFLOW_LIBRARY)
        add_library(coke::workflow-shared SHARED IMPORTED GLOBAL)
        set_target_properties(coke::workflow-shared
            PROPERTIES
                IMPORTED_LOCATION ${COKE_SHARED_WORKFLOW_LIBRARY}
                INTERFACE_INCLUDE_DIRECTORIES ${WORKFLOW_INCLUDE_DIR}
                INTERFACE_LINK_LIBRARIES "OpenSSL::SSL;OpenSSL::Crypto;Threads::Threads"
        )

        message("Find shared workflow: " ${COKE_SHARED_WORKFLOW_LIBRARY})
    endif ()
endif ()
