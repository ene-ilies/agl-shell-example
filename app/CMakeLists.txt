###########################################################################
# Copyright 2017 IoT.bzh
#
# author: Ronan Le Martret <ronan.lemartret@iot.bzh>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
###########################################################################

# Add target to project dependency list
PROJECT_TARGET_ADD(homescreen)

find_package(PkgConfig REQUIRED)
pkg_search_module(WAYLAND_CLIENT REQUIRED wayland-client)

# generating agl-shell protocol header and implementation
find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner)

get_filename_component(agl_shell_xml "protocol/agl-shell.xml" ABSOLUTE)
set(agl_shell_client_header "${CMAKE_CURRENT_BINARY_DIR}/wayland-agl-shell-client-protocol.h")
set(agl_shell_client_code "${CMAKE_CURRENT_BINARY_DIR}/wayland-agl-shell-client-protocol.c")

message("generating agl-shell client files from: ${agl_shell_xml}")

add_custom_command(OUTPUT ${agl_shell_client_header}
	COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${agl_shell_xml} ${agl_shell_client_header}
	DEPENDS ${agl_shell_xml} VERBATIM)

add_custom_command(OUTPUT ${agl_shell_client_code}
	COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${agl_shell_xml} ${agl_shell_client_code}
	DEPENDS ${agl_shell_xml} ${agl_shell_client_header} VERBATIM)

get_filename_component(agl_desktop_shell_xml "protocol/agl-shell-desktop.xml" ABSOLUTE)
set(agl_desktop_shell_client_header "${CMAKE_CURRENT_BINARY_DIR}/wayland-agl-shell-desktop-client-protocol.h")
set(agl_desktop_shell_client_code "${CMAKE_CURRENT_BINARY_DIR}/wayland-agl-shell-desktop-client-protocol.c")

message("generating agl-shell client files from: ${agl_desktop_shell_xml}")

add_custom_command(OUTPUT ${agl_desktop_shell_client_header}
	COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${agl_desktop_shell_xml} ${agl_desktop_shell_client_header}
	DEPENDS ${agl_desktop_shell_xml} VERBATIM)

add_custom_command(OUTPUT ${agl_desktop_shell_client_code}
	COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${agl_desktop_shell_xml} ${agl_desktop_shell_client_code}
	DEPENDS ${agl_desktop_shell_xml} ${agl_desktop_shell_client_header} VERBATIM)

set(SOURCES ${agl_shell_client_header}
	${agl_shell_client_code}
	${agl_desktop_shell_client_header}
	${agl_desktop_shell_client_code}
	ExampleScene.h
	ExampleScene.cpp
	xdg-shell-client-protocol.c
	xdg-shell-client-protocol.h
	${TARGET_NAME}.cpp)

# Define project Targets
ADD_EXECUTABLE(${TARGET_NAME} ${SOURCES})
SET_TARGET_PROPERTIES(${TARGET_NAME} PROPERTIES
	LABELS "EXECUTABLE"
	INCLUDE_DIRECTORIES "${CMAKE_CURRENT_BINARY_DIR}"
	OUTPUT_NAME ${TARGET_NAME}
)
# Library dependencies (include updates automatically)
TARGET_LINK_LIBRARIES(${TARGET_NAME}
	${WAYLAND_CLIENT_LIBRARIES}
	${link_libraries}
)
