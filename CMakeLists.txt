cmake_minimum_required(VERSION 3.19)
project(exiftool VERSION 0.1.0 LANGUAGES C)
include(ExternalProject)
include(GNUInstallDirs)

option(TOOLS_ONLY "Build tools for cross-compilation" OFF)

set(PERL_MODULES bytes.pm Carp.pm Compress Config.pm constant.pm Digest DynaLoader.pm Exporter.pm
    Exporter Fcntl.pm File integer.pm IO IO.pm List overload.pm overloading.pm overload
    Scalar SelectSaver.pm strict.pm Symbol.pm Time utf8.pm vars.pm warnings.pm warnings XSLoader.pm)
set(PERL_EXTS Compress/Raw/Zlib Digest/MD5 Fcntl File/Glob IO List/Util Time/HiRes Time/Piece)

# Build perl with autotools
set(PERL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/perl5)
set(EXIF_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/exiftool)
if(${CMAKE_SYSTEM_NAME} MATCHES "Emscripten")
    set(OBJ_FORMAT wasm)
    set(HOST_PERL ${PERL_PREFIX}/hostperl/perl -I${PERL_PREFIX}/hostperl/lib)
    add_executable(bin2obj IMPORTED GLOBAL)
    set_property(TARGET bin2obj PROPERTY IMPORTED_LOCATION
        ${CMAKE_CURRENT_SOURCE_DIR}/hostbuild/bin2obj)
    set(PERL_BUILD emconfigure ./configure -des -Dusemultiplicity -Dhintfile=emscripten)

    list(JOIN PERL_EXTS " " PERL_EXTS_STRING)
    list(APPEND PERL_BUILD -Dstatic_ext=${PERL_EXTS_STRING})
    list(APPEND PERL_BUILD -Dprivlib=${PERL_PREFIX}/lib && emmake make -j perl)
    list(TRANSFORM PERL_EXTS REPLACE "([^\/]+)$" "\\1/\\1.a")
    set(PERL_STATIC_LIB ${PERL_PREFIX}/libperl.a)
elseif(MSVC)
    set(OBJ_FORMAT coff)
    set(HOST_PERL ${PERL_PREFIX}/perl)
    add_executable(bin2obj bin2obj.c)
    list(APPEND PERL_MODULES base.pm Config_heavy.pl Tie Win32API)
    list(APPEND PERL_EXTS Win32API/File)
    set(PERL_BUILD cd win32 && nmake CCTYPE=MSVC142)

    list(JOIN PERL_EXTS " " PERL_EXTS_STRING)
    list(APPEND PERL_BUILD BUILD_STATIC=define STATIC_EXT=${PERL_EXTS_STRING})
    list(TRANSFORM PERL_EXTS REPLACE "([^\/]+)$" "\\1/\\1.lib")
    set(PERL_STATIC_LIB ${PERL_PREFIX}/perl536s.lib)
else()
    set(OBJ_FORMAT mach)
    set(HOST_PERL ${PERL_PREFIX}/perl)
    add_executable(bin2obj bin2obj.c)
    if(TOOLS_ONLY)
        set(PERL_BUILD mkdir hostperl && cd hostperl)
        list(APPEND PERL_BUILD && ../configure -des -Dmksymlinks)
        list(APPEND PERL_BUILD -Dfirstmakefile=GNUmakefile)
    else()
        set(PERL_BUILD ./configure -des -Dusemultiplicity)
        list(JOIN PERL_EXTS " " PERL_EXTS_STRING)
        list(APPEND PERL_BUILD -Dstatic_ext=${PERL_EXTS_STRING})
    endif()

    if(CMAKE_OSX_ARCHITECTURES)
        list(APPEND PERL_BUILD "-Accflags=-arch ${CMAKE_OSX_ARCHITECTURES}")
        list(APPEND PERL_BUILD "-Aldflags=-arch ${CMAKE_OSX_ARCHITECTURES}")
    endif()
    list(APPEND PERL_BUILD -Dprivlib=${PERL_PREFIX}/lib && make -j)
    list(TRANSFORM PERL_EXTS REPLACE "([^\/]+)$" "\\1/\\1.a")
    set(PERL_STATIC_LIB ${PERL_PREFIX}/libperl.a)
endif()

list(TRANSFORM PERL_EXTS PREPEND ${PERL_PREFIX}/lib/auto/)
file(MAKE_DIRECTORY ${PERL_PREFIX}/lib/CORE)
ExternalProject_Add(perl5
    SOURCE_DIR ${PERL_PREFIX} CONFIGURE_COMMAND "" INSTALL_COMMAND ""
    BUILD_COMMAND ${PERL_BUILD} BUILD_IN_SOURCE true
    BUILD_BYPRODUCTS ${PERL_STATIC_LIB} ${PERL_EXTS} ${PERL_PREFIX}/perl)

# Return early if only building tools
if(TOOLS_ONLY)
    return()
endif()

# Create imported target for perl
add_library(perl INTERFACE IMPORTED GLOBAL)
add_dependencies(perl perl5)
target_include_directories(perl INTERFACE ${PERL_PREFIX} ${PERL_PREFIX}/lib/CORE)
if(MSVC)
    target_compile_definitions(perl INTERFACE MULTIPLICITY PERL_IMPLICIT_SYS PERLDLL)
    target_link_libraries(perl INTERFACE ${PERL_STATIC_LIB} ${PERL_EXTS}
        oldnames.lib kernel32.lib user32.lib advapi32.lib
        comctl32.lib ws2_32.lib ucrt.lib vcruntime.lib libcmt.lib)
    target_link_options(perl INTERFACE /NODEFAULTLIB)
else()
    target_compile_definitions(perl INTERFACE MULTIPLICITY PERL_DARWIN PERL_USE_SAFE_PUTENV)
    target_link_libraries(perl INTERFACE ${PERL_STATIC_LIB} ${PERL_EXTS})
    target_link_options(perl INTERFACE -mmacosx-version-min=12.2 -fstack-protector-strong)
endif()

# Copy perl modules at configuration step
list(TRANSFORM PERL_MODULES PREPEND ${PERL_PREFIX}/lib/)
list(APPEND PERL_MODULES ${EXIF_PREFIX}/lib/File ${EXIF_PREFIX}/lib/Image)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cp.cmake
    "file(COPY ${PERL_MODULES} DESTINATION fatlib)\n")

# Pack perl sources into object file
set(FATPACKER ${CMAKE_CURRENT_SOURCE_DIR}/FatPacker.pm)
set(LOADER ${CMAKE_CURRENT_SOURCE_DIR}/loader.pl)
add_custom_command(OUTPUT packed.obj
    COMMAND ${CMAKE_COMMAND} -P cp.cmake
    COMMAND ${HOST_PERL} ${FATPACKER} ${LOADER} > packed.pl
    COMMAND bin2obj ${OBJ_FORMAT} packed.pl packed.obj
    DEPENDS perl bin2obj ${FATPACKER} ${LOADER})
set_source_files_properties(packed.obj PROPERTIES EXTERNAL_OBJECT true GENERATED true)

add_custom_command(OUTPUT perlxsi.c
    COMMAND ${HOST_PERL} -MExtUtils::Embed -e xsinit
    DEPENDS perl)
set_source_files_properties(perlxsi.c PROPERTIES GENERATED true)

add_custom_command(OUTPUT p3dci.obj
    COMMAND bin2obj ${OBJ_FORMAT} ${CMAKE_CURRENT_SOURCE_DIR}/p3dci.icc p3dci.obj
    DEPENDS bin2obj)
set_source_files_properties(p3dci.obj PROPERTIES EXTERNAL_OBJECT true GENERATED true)

# Build exiftool-dll C interface
add_library(exiftool SHARED exiftool.h exiftool.c perlxsi.c packed.obj)
target_link_libraries(exiftool PRIVATE perl)
target_compile_definitions(exiftool PRIVATE EXIFTOOL_EXPORTS)
if(MSVC)
    file(READ ${EXIF_PREFIX}/META.json EXIF_JSON)
    string(JSON EXIF_VERSION GET ${EXIF_JSON} version)
    string(REPLACE "." "," DLL_VERSION ${EXIF_VERSION})
    target_compile_definitions(exiftool PRIVATE
        DLL_VERSION=${DLL_VERSION} EXIF_VERSION=${EXIF_VERSION})
    target_sources(exiftool PRIVATE version.rc)
endif()

add_executable(test test.c p3dci.obj)
target_link_libraries(test PRIVATE exiftool)

# Install dll and headers
install(TARGETS exiftool EXPORT exiftool)
install(FILES exiftool.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
