cmake_minimum_required(VERSION 3.16)
project(ef C)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Compiler warnings
add_compile_options(-Wall -Wextra -Wpedantic -Wno-unused-parameter)

# Need _XOPEN_SOURCE_EXTENDED for wide ncurses and NCURSES_WIDECHAR
add_compile_definitions(_XOPEN_SOURCE_EXTENDED NCURSES_WIDECHAR=1)

# Find ncurses (wide character support)
# On macOS, prefer Homebrew ncurses which has full wide-char support
if(APPLE)
    execute_process(COMMAND brew --prefix ncurses OUTPUT_VARIABLE NCURSES_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(NCURSES_PREFIX)
        set(NCURSES_INCLUDE_DIRS "${NCURSES_PREFIX}/include/ncursesw" "${NCURSES_PREFIX}/include")
        set(NCURSES_LIBRARY_DIRS "${NCURSES_PREFIX}/lib")
        set(NCURSES_LIBRARIES "-L${NCURSES_PREFIX}/lib" "-lncursesw")
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(NCURSES REQUIRED ncursesw)
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(NCURSES REQUIRED ncursesw)
endif()

# Find PCRE2
if(APPLE)
    execute_process(COMMAND brew --prefix pcre2 OUTPUT_VARIABLE PCRE2_PREFIX OUTPUT_STRIP_TRAILING_WHITESPACE)
    if(PCRE2_PREFIX)
        set(PCRE2_INCLUDE_DIRS "${PCRE2_PREFIX}/include")
        set(PCRE2_LIBRARY_DIRS "${PCRE2_PREFIX}/lib")
        set(PCRE2_LIBRARIES "-L${PCRE2_PREFIX}/lib" "-lpcre2-8")
    else()
        find_package(PkgConfig REQUIRED)
        pkg_check_modules(PCRE2 REQUIRED libpcre2-8)
    endif()
else()
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(PCRE2 REQUIRED libpcre2-8)
endif()

# Core editor library (shared between main binary and tests)
add_library(ef_core STATIC
    src/mode.c
    src/runes.c
    src/constants.c
    src/buffer.c
    src/pane.c
    src/input.c
    src/history.c
    src/theme.c
    src/clipboard.c
    src/normal.c
    src/insert.c
    src/visual.c
    src/screen.c
    src/editor.c
    src/search.c
    src/widget.c
    src/autocomplete.c
    src/log.c
    src/config.c
    src/syntax.c
    src/file_watcher.c
    deps/cjson/cJSON.c
)

target_include_directories(ef_core PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/deps/cjson
    ${NCURSES_INCLUDE_DIRS}
    ${PCRE2_INCLUDE_DIRS}
)

target_link_directories(ef_core PUBLIC ${NCURSES_LIBRARY_DIRS} ${PCRE2_LIBRARY_DIRS})
target_link_libraries(ef_core PUBLIC ${NCURSES_LIBRARIES} ${PCRE2_LIBRARIES})

# Main executable
add_executable(ef src/main.c)
target_link_libraries(ef PRIVATE ef_core)

# Install
install(TARGETS ef DESTINATION bin)

# Tests
enable_testing()
add_subdirectory(tests)
