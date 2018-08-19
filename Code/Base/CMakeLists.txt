cmake_minimum_required(VERSION 3.1)

file(GLOB_RECURSE background-processor_src
	"*.h"	
	"*.cpp"
)

#-------------------------------------------------------------------------------------------
# ISPC integration
#-------------------------------------------------------------------------------------------
file(GLOB_RECURSE ispc_files
	"*.ispc"	
)

IF (CMAKE_SIZEOF_VOID_P EQUAL 8)
	set(ispc_architecture "x86-64")
ELSE()
	set(ispc_architecture "x86")
ENDIF()


set(ispc_out_objs "")
set(ispc_out_headers "")
set(ispc_out_dirs "")

FOREACH (src ${ispc_files})
	# create a directory in the binary output dir for the ispc_out-files
	# and declare+append the output filenames
	file(RELATIVE_PATH ispc_out ${PROJECT_SOURCE_DIR} ${src})
	string(CONCAT ispc_out "${PROJECT_BINARY_DIR}/${ispc_out}")
	get_filename_component(ispc_out_dir ${ispc_out} DIRECTORY)
	file(MAKE_DIRECTORY ${ispc_out_dir})
	
	string(REPLACE ".ispc" ".o" ispc_out_obj ${ispc_out})
	string(REPLACE ".ispc" "_ispc.h" ispc_out_header ${ispc_out})
	
	set(ispc_out_objs ${ispc_out_objs} ${ispc_out_obj})
	set(ispc_out_headers ${ispc_out_headers} ${ispc_out_header})
	set(ispc_out_dirs ${ispc_out_dirs} ${ispc_out_dir})
	
	# set a custom build step for the src ispc file, to run the ispc process
	# to update the header and generate an object file
	add_custom_command(
		OUTPUT ${ispc_out_obj}
		COMMAND ispc 
			--header-outfile=${ispc_out_header}
			--outfile=${ispc_out_obj}
			--arch=${ispc_architecture}
			${src}
		MAIN_DEPENDENCY ${src}
		)
	# also, while cmake is running, execute ispc specificlly to output the
	# header file if it does not exist (cmake won't include the file otherwise)
	IF (NOT EXISTS ${ispc_out_header})
		message("Running ispc to generate ${ispc_out_header}")
		execute_process(
			COMMAND ispc
				--header-outfile=${ispc_out_header}
				${src}
			)
	ENDIF()
ENDFOREACH()

add_executable(background-processor ${background-processor_src} ${ispc_files} ${ispc_out_headers})
source_group("Source Files\\ispc" FILES ${ispc_files})


#-------------------------------------------------------------------------------------------
# Options
#-------------------------------------------------------------------------------------------

#-------------------------------------------------------------------------------------------
# Sub-projects
#-------------------------------------------------------------------------------------------

#-------------------------------------------------------------------------------------------
# Defines
#-------------------------------------------------------------------------------------------
add_definitions(-DEASTL_EASTDC_VSNPRINTF=0)

#-------------------------------------------------------------------------------------------
# Library definition
#-------------------------------------------------------------------------------------------

#-------------------------------------------------------------------------------------------
# Compiler Flags
#-------------------------------------------------------------------------------------------
if (MSVC)
	set_target_properties(background-processor PROPERTIES COMPILE_FLAGS "/YuPch.h")
	set_source_files_properties(Pch.cpp PROPERTIES COMPILE_FLAGS "/YcPch.h")
endif(MSVC)

#-------------------------------------------------------------------------------------------
# Include dirs
#-------------------------------------------------------------------------------------------
include_directories(.)
include_directories(${ispc_out_dirs})

#-------------------------------------------------------------------------------------------
# Libraries
#-------------------------------------------------------------------------------------------
target_link_libraries(background-processor EASTL)
target_link_libraries(background-processor ${ispc_out_objs} )

#-------------------------------------------------------------------------------------------
# Installation
#-------------------------------------------------------------------------------------------


