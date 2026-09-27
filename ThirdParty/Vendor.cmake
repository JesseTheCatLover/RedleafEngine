# ThirdParty/Vendor.cmake

# --- GLAD ---

file(GLOB GLAD_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/glad/src/gl.c"
)

add_library(glad STATIC
        ${GLAD_SRC}
)

target_include_directories(glad PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/glad/include"
)


# --- STB ---

file(GLOB STB_SRC
        "${CMAKE_CURRENT_SOURCE_DIR}/stb/src/*.cpp"
)

add_library(stb STATIC
        ${STB_SRC}
)

target_include_directories(stb PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/stb/include"
)


# --- Native File Dialog Extended ---

add_library(nfd STATIC)

target_include_directories(nfd PUBLIC
        "${CMAKE_CURRENT_SOURCE_DIR}/nfd-extended/include"
)

if(WIN32)

    target_sources(nfd PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/nfd-extended/src/nfd_win.cpp"
    )

elseif(APPLE)

    target_sources(nfd PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/nfd-extended/src/nfd_cocoa.m"
    )

    find_library(COCOA_LIBRARY Cocoa REQUIRED)
    find_library(UTI_LIBRARY UniformTypeIdentifiers REQUIRED)

    # Keep this for older SDKs and NFD revisions that still reference it.
    find_library(
            MOBILE_CORE_SERVICES_LIBRARY
            MobileCoreServices
    )

    target_link_libraries(nfd PRIVATE
            ${COCOA_LIBRARY}
            ${UTI_LIBRARY}
    )

    if(MOBILE_CORE_SERVICES_LIBRARY)
        target_link_libraries(
                nfd PRIVATE
                ${MOBILE_CORE_SERVICES_LIBRARY}
        )
    endif()

elseif(UNIX)

    target_sources(nfd PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/nfd-extended/src/nfd_gtk.cpp"
            "${CMAKE_CURRENT_SOURCE_DIR}/nfd-extended/src/nfd_portal.cpp"
    )

    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK REQUIRED gtk+-3.0)

    target_include_directories(nfd PRIVATE
            ${GTK_INCLUDE_DIRS}
    )

    target_link_libraries(nfd PRIVATE
            ${GTK_LIBRARIES}
    )

endif()


# --- IDE organization ---

set_target_properties(
        glad
        stb
        nfd
        PROPERTIES
        EXCLUDE_FROM_ALL TRUE
        FOLDER "ThirdParty"
)