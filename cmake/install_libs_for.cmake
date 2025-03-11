#
# (C) Copyright IBM Deutschland GmbH 2021, 2025
# (C) Copyright IBM Corp. 2021, 2025
#
# non-exclusively licensed to gematik GmbH
#

macro(install_libs_for target install_component)
    message(STATUS "Will install libs for ${target} with component ${install_component}")
    install(CODE "set(CMAKE_INSTALL_LIBDIR ${CMAKE_INSTALL_LIBDIR})" COMPONENT "${install_component}")
    install(CODE "set(library \"\$<TARGET_FILE:${target}>\")" COMPONENT "${install_component}")
    install(CODE [[
        message(STATUS "Warnings about dependency resolution in search directory can be ignored.")
        message(STATUS "This may be caused by libraries provided by conan.")
        file(GET_RUNTIME_DEPENDENCIES
            EXECUTABLES "${library}"
            RESOLVED_DEPENDENCIES_VAR deps
            DIRECTORIES ${CONAN_LIB_DIRS}
            CONFLICTING_DEPENDENCIES_PREFIX conflicts
            POST_INCLUDE_REGEXES ".*/libstdc[+][+][.]so.*"
            POST_EXCLUDE_REGEXES "^/lib(64)?/.*"
        )
        foreach(conflict_lib ${conflicts_FILENAMES})
            # resolve conflicts by selecting the first alternative as would be chosen by dynamic-linker
            list(GET conflicts_${conflict_lib} 0 depend_library)

            file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
                TYPE SHARED_LIBRARY
                FOLLOW_SYMLINK_CHAIN
                FILES "${depend_library}"
            )
        endforeach()
        foreach(depend_library ${deps})
            file(INSTALL
                DESTINATION "${CMAKE_INSTALL_PREFIX}/${CMAKE_INSTALL_LIBDIR}"
                TYPE SHARED_LIBRARY
                FOLLOW_SYMLINK_CHAIN
                FILES "${depend_library}"
            )
        endforeach()
        list(REMOVE_DUPLICATES CMAKE_INSTALL_MANIFEST_FILES)
        ]]
        COMPONENT "${install_component}")
    file(RELATIVE_PATH RELATIVE_LIB_DIR ${CMAKE_INSTALL_FULL_BINDIR} ${CMAKE_INSTALL_FULL_LIBDIR})
    set_property(TARGET "${target}" APPEND PROPERTY INSTALL_RPATH "\${ORIGIN}/${RELATIVE_LIB_DIR}")
endmacro()
