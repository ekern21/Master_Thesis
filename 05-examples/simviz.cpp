// This example application loads a URDF world file and simulates two robots
// with physics and contact in a Dynamics3D virtual world. A graphics model of it is also shown using 
// Chai3D.

#include "Sai2Model.h"
#include "Sai2Simulation.h"
#include "Sai2Graphics.h"
#include "redis/RedisClient.h"
#include "timer/LoopTimer.h"
#include "force_sensor/ForceSensorSim.h"

#include <GLFW/glfw3.h> //must be loaded after loading opengl/glew

#include <thread>
#include <iostream>
#include <string>
#include <signal.h>
#include <cmath>


using namespace std;

using namespace Eigen;

const string world_file = "resources/world.urdf";
const string robot_file = "resources/panda_arm_FT_sensor.urdf";
const string robot_name = "FRANKA-PANDA-SENSOR";
const string camera_name = "camera_fixed";

// redis keys:
// - read:
const string JOINT_TORQUES_COMMANDED_KEY = "sai2::DemoApplication::Panda::actuators::fgc";
// - write:
const string JOINT_ANGLES_KEY  = "sai2::DemoApplication::Panda::sensors::q";
const string JOINT_VELOCITIES_KEY = "sai2::DemoApplication::Panda::sensors::dq";
const string JOINT_ACCELERATIONS_KEY = "sai2::DemoApplication::Panda::sensors::ddq";
const string SIM_TIMESTAMP_KEY = "sai2::DemoApplication::Panda::simulation::timestamp";

const string FORCE_VIRTUAL_KEY = "sai2::DemoApplication::Panda::simulation::virtual_force";
// local
// linear
const string LINEAR_ACC_KEY = "sai2::DemoApplication::Panda::simulation::linear_acc";
//angular
const string ANGULAR_VEL_KEY = "sai2::DemoApplication::Panda::simulation::angular_vel";
const string ANGULAR_ACC_KEY = "sai2::DemoApplication::Panda::simulation::angular_acc";
const string LOCAL_GRAVITY_KEY = "sai2::DemoApplication::Panda::simulation::g_local";


//-----force sensor link and position in link----
//-----FT sensor (panda_arm_FT_sensor.urdf)------
const string link_name = "link13";

//-----F sensor (panda_arm_force_sensor.urdf)------
//const string link_name = "link10";
const Eigen::Vector3d pos_in_link = Eigen::Vector3d(0,0,0);

bool runloop = false;
void sighandler(int){runloop = false;}

// simulation loop
void simulation(Sai2Model::Sai2Model* robot, Simulation::Sai2Simulation* sim);
unsigned long long sim_counter = 0;

// callback to print glfw errors
void glfwError(int error, const char* description);

// callback when a key is pressed
void keySelect(GLFWwindow* window, int key, int scancode, int action, int mods);

// callback when a mouse button is pressed
void mouseClick(GLFWwindow* window, int button, int action, int mods);

// flags for scene camera movement
bool fTransXp = false;
bool fTransXn = false;
bool fTransYp = false;
bool fTransYn = false;
bool fTransZp = false;
bool fTransZn = false;
bool fRotPanTilt = false;

RedisClient redis_client;

int main() {
	cout << "Loading URDF world model file: " << world_file << endl;

	// start redis client
	HiredisServerInfo info;
	info.hostname_ = "127.0.0.1"; //"172.24.68.64";
	info.port_ = 6379;
	info.timeout_ = { 1, 500000 }; // 1.5 seconds
	redis_client = RedisClient();
	redis_client.serverIs(info);

	// set up signal handler
	signal(SIGABRT, &sighandler);
	signal(SIGTERM, &sighandler);
	signal(SIGINT, &sighandler);

	// load graphics scene
	auto graphics = new Sai2Graphics::Sai2Graphics(world_file, false);
	Eigen::Vector3d camera_pos, camera_lookat, camera_vertical;
	graphics->getCameraPose(camera_name, camera_pos, camera_vertical, camera_lookat);
	// graphics->_world->setShowFrame(true, true);

	// load simulation world
	auto sim = new Simulation::Sai2Simulation(world_file, false);

	// load robots
	auto robot = new Sai2Model::Sai2Model(robot_file, false);

	sim->setCollisionRestitution(0);
	sim->setCoeffFrictionStatic(0.3);

	/*------- Set up visualization -------*/
    // set up error callback
    glfwSetErrorCallback(glfwError);

    // initialize GLFW
    glfwInit();

    // retrieve resolution of computer display and position window accordingly
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(primary);

    // information about computer screen and GLUT display window
	int screenW = mode->width;
    int screenH = mode->height;
    int windowW = 0.8 * screenH;
    int windowH = 0.5 * screenH;
    int windowPosY = (screenH - windowH) / 2;
    int windowPosX = windowPosY;

    // create window and make it current
    glfwWindowHint(GLFW_VISIBLE, 0);
    GLFWwindow* window = glfwCreateWindow(windowW, windowH, "SAI2.0 - Panda", NULL, NULL);
	glfwSetWindowPos(window, windowPosX, windowPosY);
	glfwShowWindow(window);
    glfwMakeContextCurrent(window);
	glfwSwapInterval(1);

    // set callbacks
	glfwSetKeyCallback(window, keySelect);
	glfwSetMouseButtonCallback(window, mouseClick);

	// start the simulation thread first
	runloop = true;
	thread sim_thread(simulation, robot, sim);

	// cache variables
	double last_cursorx, last_cursory;

    // while window is open:
    while (!glfwWindowShouldClose(window))
	{
		// read from Redis

		// update graphics. this automatically waits for the correct amount of time
		int width, height;
		glfwGetFramebufferSize(window, &width, &height);
		graphics->updateGraphics(robot_name, robot);
		graphics->render(camera_name, width, height);

		// swap buffers
		glfwSwapBuffers(window);

		// wait until all GL commands are completed
		glFinish();

		// check for any OpenGL errors
		GLenum err;
		err = glGetError();
		assert(err == GL_NO_ERROR);

	    // poll for events
	    glfwPollEvents();

	    // move scene camera as required
    	// graphics->getCameraPose(camera_name, camera_pos, camera_vertical, camera_lookat);
    	Eigen::Vector3d cam_depth_axis;
    	cam_depth_axis = camera_lookat - camera_pos;
    	cam_depth_axis.normalize();
    	Eigen::Vector3d cam_up_axis;
    	// cam_up_axis = camera_vertical;
    	// cam_up_axis.normalize();
    	cam_up_axis << 0.0, 0.0, 1.0; //TODO: there might be a better way to do this
	    Eigen::Vector3d cam_roll_axis = (camera_lookat - camera_pos).cross(cam_up_axis);
    	cam_roll_axis.normalize();
    	Eigen::Vector3d cam_lookat_axis = camera_lookat;
    	cam_lookat_axis.normalize();
    	if (fTransXp) {
	    	camera_pos = camera_pos + 0.05*cam_roll_axis;
	    	camera_lookat = camera_lookat + 0.05*cam_roll_axis;
	    }
	    if (fTransXn) {
	    	camera_pos = camera_pos - 0.05*cam_roll_axis;
	    	camera_lookat = camera_lookat - 0.05*cam_roll_axis;
	    }
	    if (fTransYp) {
	    	// camera_pos = camera_pos + 0.05*cam_lookat_axis;
	    	camera_pos = camera_pos + 0.05*cam_up_axis;
	    	camera_lookat = camera_lookat + 0.05*cam_up_axis;
	    }
	    if (fTransYn) {
	    	// camera_pos = camera_pos - 0.05*cam_lookat_axis;
	    	camera_pos = camera_pos - 0.05*cam_up_axis;
	    	camera_lookat = camera_lookat - 0.05*cam_up_axis;
	    }
	    if (fTransZp) {
	    	camera_pos = camera_pos + 0.1*cam_depth_axis;
	    	camera_lookat = camera_lookat + 0.1*cam_depth_axis;
	    }	    
	    if (fTransZn) {
	    	camera_pos = camera_pos - 0.1*cam_depth_axis;
	    	camera_lookat = camera_lookat - 0.1*cam_depth_axis;
	    }
	    if (fRotPanTilt) {
	    	// get current cursor position
	    	double cursorx, cursory;
			glfwGetCursorPos(window, &cursorx, &cursory);
			//TODO: might need to re-scale from screen units to physical units
			double compass = 0.006*(cursorx - last_cursorx);
			double azimuth = 0.006*(cursory - last_cursory);
			double radius = (camera_pos - camera_lookat).norm();
			Eigen::Matrix3d m_tilt; m_tilt = Eigen::AngleAxisd(azimuth, -cam_roll_axis);
			camera_pos = camera_lookat + m_tilt*(camera_pos - camera_lookat);
			Eigen::Matrix3d m_pan; m_pan = Eigen::AngleAxisd(compass, -cam_up_axis);
			camera_pos = camera_lookat + m_pan*(camera_pos - camera_lookat);
	    }
	    graphics->setCameraPose(camera_name, camera_pos, cam_up_axis, camera_lookat);
	    glfwGetCursorPos(window, &last_cursorx, &last_cursory);
	}

	// stop simulation
	runloop = false;
	sim_thread.join();

    // destroy context
    glfwDestroyWindow(window);

    // terminate
    glfwTerminate();

	return 0;
}

void simulation(Sai2Model::Sai2Model* robot, Simulation::Sai2Simulation* sim)
{

	int dof = robot->dof();
	Eigen::VectorXd robot_torques = Eigen::VectorXd::Zero(7);
	Eigen::VectorXd robot_torques_full = Eigen::VectorXd::Zero(dof);
	Eigen::VectorXd joint_angles = Eigen::VectorXd::Zero(7);
	Eigen::VectorXd joint_velocities = Eigen::VectorXd::Zero(7);
	Eigen::VectorXd joint_accelerations = Eigen::VectorXd::Zero(7);
	Eigen::VectorXd gravity_torques = Eigen::VectorXd::Zero(dof);

	Eigen::Vector3d accel = Eigen::Vector3d::Zero(); //object linear acceleration in base frame
	Eigen::Vector3d avel = Eigen::Vector3d::Zero(); //object angular velocity in base frame
	Eigen::Vector3d aaccel = Eigen::Vector3d::Zero(); //object angular acceleration in base frame
	Eigen::Vector3d accel_local = Eigen::Vector3d::Zero(); // object linear acceleration in sensor frame
	Eigen::Vector3d aaccel_local = Eigen::Vector3d::Zero(); // object angular acceleration in sensor frame
	Eigen::Vector3d avel_local = Eigen::Vector3d::Zero(); //object angular velocity in sensor frame
	Eigen::Vector3d g_local = Eigen::Vector3d::Zero(); //gravity vector in sensor frame
	Eigen::Matrix3d R_link = Eigen::Matrix3d::Zero(); // rotation matrix



	//initialize redis value
	redis_client.setEigenMatrixDerived(JOINT_TORQUES_COMMANDED_KEY, Eigen::VectorXd::Zero(7));

	Eigen::VectorXd force_virtual = Eigen::VectorXd::Zero(6);

	double k_f = 5e4;
	double d_f = 850;

	double k_t = 5e2;
	double d_t = 50; 



	double F_x = 0;
	double F_y = 0;
	double F_z = 0;
	double Tau_x = 0;
	double Tau_y = 0;
	double Tau_z = 0;


	// create a loop timer 
	double sim_freq = 10000;  // set the simulation frequency. Ideally 10kHz

	LoopTimer timer;
	timer.setLoopFrequency(sim_freq);   // 10 KHz
	// timer.setThreadHighPriority();  // make timing more accurate. requires running executable as sudo.
	timer.setCtrlCHandler(sighandler);    // exit while loop on ctrl-c
	timer.initializeTimer(1000000); // 1 ms pause before starting loop
		std::chrono::high_resolution_clock::time_point t_start;
	std::chrono::duration<double> t_elapsed;
	while (runloop) {
		// wait for next scheduled loop

		timer.waitForNextLoop();

		
		redis_client.getEigenMatrixDerived(JOINT_TORQUES_COMMANDED_KEY, robot_torques);


		// update kinematic models
		sim->getJointPositions(robot_name, robot->_q);
		sim->getJointVelocities(robot_name, robot->_dq);
		sim->getJointAccelerations(robot_name, robot->_ddq);


		robot->updateModel();


		robot->linearAcceleration(accel,link_name,pos_in_link);
		robot->angularAcceleration(aaccel,link_name);
		robot->angularVelocity(avel,link_name);
		robot->rotation(R_link,link_name); 


		accel_local = R_link.transpose()*accel;
		avel_local = R_link.transpose()*avel;
		aaccel_local = R_link.transpose()*aaccel;
		g_local = R_link.transpose()*robot->_world_gravity;




		F_x =  - k_f * robot->_q(7) - d_f * robot->_dq(7);
		F_y =  - k_f * robot->_q(8) - d_f * robot->_dq(8);
		F_z =  - k_f * robot->_q(9) - d_f * robot->_dq(9);

		Tau_x =  - k_t * robot->_q(10) - d_t * robot->_dq(10);
		Tau_y =  - k_t * robot->_q(11) - d_t * robot->_dq(11);
		Tau_z =  - k_t * robot->_q(12) - d_t * robot->_dq(12);
		force_virtual << F_x, F_y, F_z, Tau_x, Tau_y, Tau_z;

		// append controller forces to robot torques
		robot_torques_full << robot_torques(0),robot_torques(1),robot_torques(2),robot_torques(3),robot_torques(4), robot_torques(5), robot_torques(6), F_x, F_y, F_z, Tau_x, Tau_y, Tau_z;
		robot->gravityVector(gravity_torques);
		gravity_torques(7)=0;
		gravity_torques(8)=0;
		gravity_torques(9)=0;
		gravity_torques(10)=0;
		gravity_torques(11)=0;
		gravity_torques(12)=0;

		sim->setJointTorques(robot_name, robot_torques_full + gravity_torques);

		joint_angles << robot->_q(0), robot->_q(1), robot->_q(2), robot->_q(3), robot->_q(4), robot->_q(5), robot->_q(6);
		joint_velocities << robot->_dq(0), robot->_dq(1), robot->_dq(2), robot->_dq(3), robot->_dq(4), robot->_dq(5), robot->_dq(6);
		joint_accelerations << robot->_ddq(0), robot->_ddq(1), robot->_ddq(2), robot->_ddq(3), robot->_ddq(4), robot->_ddq(5), robot->_ddq(6);
		


		// write joint kinematics to redis
		redis_client.setEigenMatrixDerived(JOINT_ANGLES_KEY, joint_angles);
		redis_client.setEigenMatrixDerived(JOINT_VELOCITIES_KEY, joint_velocities);
		redis_client.setEigenMatrixDerived(JOINT_ACCELERATIONS_KEY, joint_accelerations);
		
		redis_client.setEigenMatrixDerived(FORCE_VIRTUAL_KEY, force_virtual);
		redis_client.setEigenMatrixDerived(LINEAR_ACC_KEY, accel_local);
		redis_client.setEigenMatrixDerived(ANGULAR_ACC_KEY, aaccel_local);
		redis_client.setEigenMatrixDerived(ANGULAR_VEL_KEY, avel_local);
		redis_client.setEigenMatrixDerived(LOCAL_GRAVITY_KEY, g_local);


		redis_client.setCommandIs(SIM_TIMESTAMP_KEY,std::to_string(timer.elapsedTime()));

		// update simulation by 1ms
		sim->integrate(1/sim_freq);
		// t_prev = t_curr;
		sim_counter++;

	}

	double end_time = timer.elapsedTime();
    std::cout << "\n";
    std::cout << "Loop run time  : " << end_time << " seconds\n";
    std::cout << "Loop updates   : " << timer.elapsedCycles() << "\n";
    std::cout << "Loop frequency : " << timer.elapsedCycles()/end_time << "Hz\n";

}


//------------------------------------------------------------------------------

void glfwError(int error, const char* description) {
	cerr << "GLFW Error: " << description << endl;
	exit(1);
}


//------------------------------------------------------------------------------

void keySelect(GLFWwindow* window, int key, int scancode, int action, int mods)
{
	bool set = (action != GLFW_RELEASE);
    switch(key) {
		case GLFW_KEY_ESCAPE:
			// exit application
			glfwSetWindowShouldClose(window,GL_TRUE);
			break;
		case GLFW_KEY_RIGHT:
			fTransXp = set;
			break;
		case GLFW_KEY_LEFT:
			fTransXn = set;
			break;
		case GLFW_KEY_UP:
			fTransYp = set;
			break;
		case GLFW_KEY_DOWN:
			fTransYn = set;
			break;
		case GLFW_KEY_A:
			fTransZp = set;
			break;
		case GLFW_KEY_Z:
			fTransZn = set;
			break;
		default:
			break;
    }
}


//------------------------------------------------------------------------------

void mouseClick(GLFWwindow* window, int button, int action, int mods) {
	bool set = (action != GLFW_RELEASE);
	//TODO: mouse interaction with robot
	switch (button) {
		// left click pans and tilts
		case GLFW_MOUSE_BUTTON_LEFT:
			fRotPanTilt = set;
			// NOTE: the code below is recommended but doesn't work well
			// if (fRotPanTilt) {
			// 	// lock cursor
			// 	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
			// } else {
			// 	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
			// }
			break;
		// if right click: don't handle. this is for menu selection
		case GLFW_MOUSE_BUTTON_RIGHT:
			//TODO: menu
			break;
		// if middle click: don't handle. doesn't work well on laptops
		case GLFW_MOUSE_BUTTON_MIDDLE:
			break;
		default:
			break;
	}
}

