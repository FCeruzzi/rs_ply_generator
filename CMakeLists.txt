cmake_minimum_required( VERSION 3.5 )

# Require C++17 (or later)
set( CMAKE_CXX_STANDARD 17 )
set( CMAKE_CXX_STANDARD_REQUIRED ON )
set( CMAKE_CXX_EXTENSIONS OFF )

# Create Project
project( rs_ply_generator )
add_executable( rs_ply_generator rs_ply_generator.hpp rs_ply_generator.cpp main.cpp )

# Set StartUp Project
set_property( DIRECTORY PROPERTY VS_STARTUP_PROJECT "rs_ply_generator" )

# Find Package
# librealsense2
find_package( realsense2 REQUIRED )

# OpenCV
find_package( OpenCV 4.2 REQUIRED )

# PCL
find_package(PCL 1.7 REQUIRED)

if( realsense2_FOUND AND OpenCV_FOUND AND PCL_FOUND )
  # Additional Include Directories
  include_directories( ${realsense_INCLUDE_DIR} )
  include_directories( ${OpenCV_INCLUDE_DIRS} )
  include_directories(${PCL_INCLUDE_DIRS})
  link_directories(${PCL_LIBRARY_DIRS})
  add_definitions(${PCL_DEFINITIONS})

  # Additional Dependencies
  target_link_libraries(rs_ply_generator ${realsense2_LIBRARY})
  target_link_libraries(rs_ply_generator ${OpenCV_LIBS})
  target_link_libraries(rs_ply_generator ${PCL_LIBRARIES})
  if( NOT WIN32 )
      target_link_libraries(rs_ply_generator stdc++fs)
  endif()
endif()

