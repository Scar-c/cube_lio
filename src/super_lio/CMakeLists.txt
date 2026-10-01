cmake_minimum_required(VERSION 3.16)
project(super_lio)


#DEBUG CONFIGURATION
# set(CMAKE_BUILD_TYPE "Debug")
# set(CMAKE_CXX_STANDARD 17)
# set(CMAKE_CXX_FLAGS_DEBUG "-O0 -Wall -g -pthread -fsanitize=address")

set(CMAKE_BUILD_TYPE Release)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -pthread -fexceptions -flto=auto")
add_definitions(-DROOT=\"${CMAKE_CURRENT_SOURCE_DIR}/\")

# Find packages
find_package(Eigen3 REQUIRED)
find_package(catkin REQUIRED COMPONENTS
  roscpp
  rosbag
  rospy
  basic
  pcl_ros
  std_msgs
  geometry_msgs
  sensor_msgs
  visualization_msgs
  nav_msgs
  message_generation
)
find_package(PythonLibs REQUIRED)
find_package(OpenCV REQUIRED)

add_message_files(
  FILES
  CloudPose.msg
  CloudPose2.msg
)

generate_messages(
  DEPENDENCIES
  std_msgs
  sensor_msgs
  geometry_msgs
)

# Include directories for libraries
include_directories(
  ${catkin_INCLUDE_DIRS}
  ${EIGEN3_INCLUDE_DIRS}
  include
  3rdparty
)

# Package setup for catkin
catkin_package(
  CATKIN_DEPENDS geometry_msgs nav_msgs roscpp rospy std_msgs message_runtime
  DEPENDS EIGEN3 PCL
  INCLUDE_DIRS include
)

# LIO library source files
file(GLOB_RECURSE LIO_LIB_SRC "src/lio/*.cpp" "src/ros/*.cpp" "src/intensity/*.cpp")

file(GLOB_RECURSE LIO_LIB_HEADERS "include/lio/*.hpp" "include/lio/*.h")

# library build
add_library(lio ${LIO_LIB_SRC} ${LIO_LIB_HEADERS})

# Target specific includes
target_include_directories(lio PUBLIC
  include
  3rdparty
  ${EIGEN3_INCLUDE_DIRS}
  ${catkin_INCLUDE_DIRS}
  ${OpenCV_INCLUDE_DIRS}
  ${PCL_INCLUDE_DIRS}
)

# Linking libraries
target_link_libraries(lio
  ${EIGEN3_LIBRARIES}
  ${catkin_LIBRARIES}
  ${PCL_LIBRARIES}
  ${PYTHON_LIBRARIES}
  ${OpenCV_LIBS}
  tbb 
  gflags 
  glog
)


add_executable(super_lio_node src/apps/super_lio_node.cpp)
target_link_libraries(super_lio_node
  lio
  ${catkin_LIBRARIES}
)


add_executable(relocation_node src/apps/relocation_node.cpp)
target_link_libraries(relocation_node
  lio
  ${catkin_LIBRARIES}
)

add_dependencies(lio ${${PROJECT_NAME}_EXPORTED_TARGETS} ${catkin_EXPORTED_TARGETS})
add_executable(cube_offline_node src/apps/cube_offline_node.cpp)
target_link_libraries(cube_offline_node lio ${catkin_LIBRARIES})

add_executable(test_cube_photo test/test_cube_photo.cpp src/intensity/cube_image.cpp)
target_include_directories(test_cube_photo PRIVATE ${OpenCV_INCLUDE_DIRS})
target_link_libraries(test_cube_photo ${OpenCV_LIBS} tbb)
enable_testing()
add_test(NAME cube_photo_jacobians COMMAND test_cube_photo)

add_executable(test_information_budget test/test_information_budget.cpp src/intensity/information_budget.cpp)
target_link_libraries(test_information_budget tbb)
add_test(NAME photo_information_budget COMMAND test_information_budget)
