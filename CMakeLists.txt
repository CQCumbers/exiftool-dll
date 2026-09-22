cmake_minimum_required(VERSION 3.19)
project(exiftool VERSION 0.1.0 LANGUAGES C)
include(ExternalProject)
include(GNUInstallDirs)

set(PERL_MODULES bytes.pm Carp.pm Compress Config.pm Config_git.pl constant.pm
    Digest DynaLoader.pm Exporter.pm Exporter Fcntl.pm File integer.pm IO IO.pm
    List overload.pm overloading.pm overload Scalar SelectSaver.pm strict.pm
    Symbol.pm Time utf8.pm vars.pm warnings.pm warnings XSLoader.pm)
set(PERL_EXTS Compress/Raw/Zlib Digest/MD5 Fcntl File/Glob IO List/Util Time/HiRes Time/Piece)

# Build perl with autotools
set(PERL_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/perl5)
set(EXIF_PREFIX ${CMAKE_CURRENT_SOURCE_DIR}/exiftool)
if(MSVC)
    list(APPEND PERL_MODULES base.pm Config_heavy.pl Tie Win32API)
    list(APPEND PERL_EXTS Win32API/File)
    list(JOIN PERL_EXTS " " PERL_EXTS_STRING)
    set(PERL_BUILD cd win32 && nmake CCTYPE=MSVC142
        BUILD_STATIC=define STATIC_EXT=${PERL_EXTS_STRING})
    set(PERL_STATIC_LIB ${PERL_PREFIX}/perl536s.lib)
    list(TRANSFORM PERL_EXTS REPLACE "([^\/]+)$" "\\1/\\1.lib")
else()
    list(JOIN PERL_EXTS " " PERL_EXTS_STRING)
    set(PERL_BUILD ./Configure -des -Dusemultiplicity
        -Dfirstmakefile=Makefile0 -Dstatic_ext=${PERL_EXTS_STRING})
    if(CMAKE_OSX_ARCHITECTURES)
        list(APPEND PERL_BUILD "-Accflags=-arch ${CMAKE_OSX_ARCHITECTURES}")
        list(APPEND PERL_BUILD "-Aldflags=-arch ${CMAKE_OSX_ARCHITECTURES}")
    endif()
    list(APPEND PERL_BUILD -Accflags=-fPIC -Dprivlib=${PERL_PREFIX}/lib && make -j)
    set(PERL_STATIC_LIB ${PERL_PREFIX}/libperl.a)
    list(TRANSFORM PERL_EXTS REPLACE "([^\/]+)$" "\\1/\\1.a")
endif()

list(TRANSFORM PERL_EXTS PREPEND ${PERL_PREFIX}/lib/auto/)
file(MAKE_DIRECTORY ${PERL_PREFIX}/lib/CORE)
ExternalProject_Add(perl5
    SOURCE_DIR ${PERL_PREFIX} CONFIGURE_COMMAND "" INSTALL_COMMAND ""
    BUILD_COMMAND ${PERL_BUILD} BUILD_IN_SOURCE true
    BUILD_BYPRODUCTS ${PERL_STATIC_LIB} ${PERL_EXTS} ${PERL_PREFIX}/perl)

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
elseif(APPLE)
    target_compile_definitions(perl INTERFACE MULTIPLICITY PERL_DARWIN PERL_USE_SAFE_PUTENV)
    target_link_libraries(perl INTERFACE ${PERL_STATIC_LIB} ${PERL_EXTS})
    target_link_options(perl INTERFACE -mmacosx-version-min=12.2 -fstack-protector-strong)
else()
    target_compile_definitions(perl INTERFACE MULTIPLICITY)
    target_link_libraries(perl INTERFACE ${PERL_STATIC_LIB} ${PERL_EXTS} m crypt)
    target_link_options(perl INTERFACE -fstack-protector-strong)
endif()

# Copy perl modules at configuration step
list(TRANSFORM PERL_MODULES PREPEND ${PERL_PREFIX}/lib/)
list(APPEND PERL_MODULES ${EXIF_PREFIX}/lib/File ${EXIF_PREFIX}/lib/Image)
file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/cp.cmake
    "file(COPY ${PERL_MODULES} DESTINATION fatlib)\n")

# Pack perl sources into object file
add_executable(bin2obj bin2obj.c)
set(FATPACKER ${CMAKE_CURRENT_SOURCE_DIR}/FatPacker.pm)
set(LOADER ${CMAKE_CURRENT_SOURCE_DIR}/loader.pl)
if(UNIX AND NOT APPLE)
    add_custom_command(OUTPUT packed.obj
        COMMAND ${CMAKE_COMMAND} -P cp.cmake
        COMMAND ${PERL_PREFIX}/perl ${FATPACKER} ${LOADER} > packed.pl
        COMMAND "objcopy" "-I" "binary" "-O" "elf64-x86-64" packed.pl packed.obj
        DEPENDS perl bin2obj ${FATPACKER} ${LOADER})
else()
add_custom_command(OUTPUT packed.obj
    COMMAND ${CMAKE_COMMAND} -P cp.cmake
    COMMAND ${PERL_PREFIX}/perl ${FATPACKER} ${LOADER} > packed.pl
    COMMAND "$<TARGET_FILE:bin2obj>" packed.pl packed.obj
    DEPENDS perl bin2obj ${FATPACKER} ${LOADER})
endif()
set_source_files_properties(packed.obj PROPERTIES EXTERNAL_OBJECT true GENERATED true)

add_custom_command(OUTPUT perlxsi.c
    COMMAND ${PERL_PREFIX}/perl -MExtUtils::Embed -e xsinit
    DEPENDS perl)
set_source_files_properties(perlxsi.c PROPERTIES GENERATED true)

if(UNIX AND NOT APPLE)
    add_custom_command(OUTPUT p3dci.obj
        COMMAND "cp" ${CMAKE_CURRENT_SOURCE_DIR}/p3dci.icc ./p3dci.icc
        COMMAND "objcopy" "-I" "binary" "-O" "elf64-x86-64" p3dci.icc p3dci.obj)
else()
    add_custom_command(OUTPUT p3dci.obj
        COMMAND "$<TARGET_FILE:bin2obj>" ${CMAKE_CURRENT_SOURCE_DIR}/p3dci.icc p3dci.obj
        DEPENDS bin2obj)
endif()
set_source_files_properties(p3dci.obj PROPERTIES EXTERNAL_OBJECT true GENERATED true)

# Build exiftool-dll C interface
add_library(exiftool SHARED exiftool.h exiftool.c perlxsi.c packed.obj)
if(UNIX AND NOT APPLE)
    target_compile_options(exiftool PRIVATE -fPIC)
endif()
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
if(UNIX AND NOT APPLE)
    target_compile_options(test PRIVATE -fPIC)
    #target_link_libraries(test PRIVATE perl)
endif()
target_link_libraries(test PRIVATE exiftool)

# Install dll and headers
install(TARGETS exiftool EXPORT exiftool)
install(FILES exiftool.h DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(FILES ${PERL_PREFIX}/lib/Config_git.pl DESTINATION ${CMAKE_INSTALL_LIBDIR})
