cmake_minimum_required( VERSION 2.8.11 )

project( InputMap )

if( WIN32 AND CMAKE_SIZEOF_VOID_P EQUAL 8 ) 
	# On Windows 64bit: check that the right boost binaries are set before continuing
	if( NOT DEFINED BOOST_LIBRARYDIR OR BOOST_LIBRARYDIR STREQUAL "BOOST_LIBRARYDIR-NOT-SET" )
		set( BOOST_LIBRARYDIR "BOOST_LIBRARYDIR-NOT-SET" CACHE PATH "Location of the Boost library binaries" FORCE )
		message( FATAL_ERROR "BOOST_LIBRARYDIR is not set (we need the 64bit version). Before continuing, please set it to the correct binary path (depending on if you want to link with 32 or 64bit version)." )
	endif()
	
endif()

# Force Boost.Thread V4 use for all projects, including dependencies.
add_definitions( -DBOOST_THREAD_VERSION=4 -DBOOST_RESULT_OF_USE_DECLTYPE )

set( Boost_USE_STATIC_LIBS        ON )
set( Boost_USE_MULTITHREADED      ON )
set( Boost_USE_STATIC_RUNTIME    OFF )
find_package( Boost 1.55.0 REQUIRED COMPONENTS ${ARGV} )

if( NOT Boost_FOUND )
	message( SEND_ERROR "Boost libraries NOT FOUND!" )
endif()

include_directories( ${Boost_INCLUDE_DIR} )

add_subdirectory( ./test )

