SET(RESOURCE_DIR ${INERTIAL_PARAMERTER_APP_BINARY_DIR}/resources)
FILE(MAKE_DIRECTORY ${RESOURCE_DIR})
SET(APP_RESOURCE_DIR ${RESOURCE_DIR}/01-panda_force_control)
FILE(MAKE_DIRECTORY ${APP_RESOURCE_DIR})
FILE(COPY world.urdf 
		  world_without_gravity.urdf
		  world_robot_with_sensor.urdf
		  world_svh_hand.urdf 
		  svh.urdf
		  svh_test.urdf
		  panda_arm_with_modeled_sensor.urdf 
		  panda_arm.urdf 
		  panda_arm_FT_sensor.urdf 
		  panda_arm_force_sensor.urdf
		    DESTINATION ${APP_RESOURCE_DIR})