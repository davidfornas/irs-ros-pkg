cmake_minimum_required(VERSION 2.8.3)
project(laser_stripe_reconstruction)

find_package(catkin REQUIRED COMPONENTS
  roscpp
  mar_perception
  mar_action
  mar_robot_arm5e
  mar_ros_bridge
)
find_package(VISP REQUIRED)

catkin_package(
  CATKIN_DEPENDS    
    roscpp
	mar_perception
	mar_action
	mar_robot_arm5e
	mar_ros_bridge
  DEPENDS
    VISP
)
add_definitions(${VISP_DEFINITIONS})
link_directories(${VISP_LIBRARY_DIRS})
include_directories(${catkin_INCLUDE_DIRS} ${VISP_INCLUDE_DIRS})

add_executable(laser_stripe_reconstruction src/laser_stripe_reconstruction.cpp)
target_link_libraries(${PROJECT_NAME} ${catkin_LIBRARIES})

install(
  TARGETS
    laser_stripe_reconstruction
  ARCHIVE DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  LIBRARY DESTINATION ${CATKIN_PACKAGE_LIB_DESTINATION}
  RUNTIME DESTINATION ${CATKIN_PACKAGE_BIN_DESTINATION}
)
