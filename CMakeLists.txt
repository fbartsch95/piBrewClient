cmake_minimum_required(VERSION 2.6)

find_package(PkgConfig REQUIRED)

pkg_search_module(GATTLIB REQUIRED gattlib)
pkg_search_module(PCRE REQUIRED libpcre)

set(brew_center_SRCS brew_center_main.c)

add_executable(brew_center ${brew_center_SRCS})
target_link_libraries(brew_center ${GATTLIB_LIBRARIES} ${GATTLIB_LDFLAGS} ${PCRE_LIBRARIES} pthread)
