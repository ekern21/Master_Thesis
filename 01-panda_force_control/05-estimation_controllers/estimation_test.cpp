#include "Sai2Model.h"
#include "redis/RedisClient.h"
#include "timer/LoopTimer.h"
#include "Sai2Primitives.h"
#include "parameter_estimation/RecursiveLeastSquare.h"
#include "parameter_estimation/LeastSquare.h"



#include <signal.h>
#include <iostream>
#include <fstream>
#include <string>
#include <tinyxml2.h>
#include <chrono>
bool runloop = true;
void sighandler(int sig)
{ runloop = false; }

using namespace std;
using namespace Eigen;

const string robot_file = "../../resources/01-panda_force_control/panda_arm.urdf";
const string robot_name = "FRANKA-PANDA";

unsigned long long controller_counter = 0;

const bool flag_simulation = true;
// const bool flag_simulation = false;

const bool inertia_regularization = true;
// redis keys:
// - write:
string JOINT_TORQUES_COMMANDED_KEY;

// - read:
string JOINT_ANGLES_KEY;
string JOINT_VELOCITIES_KEY;
string JOINT_ACCELERATIONS_KEY;
string EE_FORCE_SENSOR_FORCE_KEY;
string ACCELEROMETER_DATA_KEY;
string GYROSCOPE_DATA_KEY;


// - model
string MASSMATRIX_KEY;
string CORIOLIS_KEY;
string ROBOT_GRAVITY_KEY;

// - offline processing
string LINEAR_ACCELERATION_LOCAL_KEY;
string ANGULAR_VELOCITY_LOCAL_KEY;
string EE_FORCE_SENSOR_KEY;
string QUATERNION_KEY;

string LINEAR_VEL_KF_KEY;

// - inertial parameters
string INERTIAL_PARAMS_KEY;
string INERTIAL_PARAMS_LS_KEY;
string INERTIAL_PARAMS_DEBUG_KEY; //computed with functions, not the library

// - kinematics:
string POSITION_KEY;
string LINEAR_VEL_KIN_KEY;
string LINEAR_ACC_KIN_KEY;
string ORIENTATION_QUATERNION_KEY;
string ANGULAR_VEL_KIN_KEY;
string ANGULAR_ACC_KIN_KEY;

string LINEAR_ACC_KEY;
string ANGULAR_VEL_KEY;
string ANGULAR_ACC_KEY;
string LOCAL_GRAVITY_KEY;

#define  GOTO_INITIAL_CONFIG 	 0
#define	 MOVE_FIRST			     1
#define  MOVE_SECOND	         2
#define  MOVE_THIRD				 3
#define  MOVE_X					 4 
#define  MOVE_Y					 5
#define  MOVE_Z					 6
#define  MOVE_X_BACK			 7
#define  MOVE_Y_BACK			 8
#define  MOVE_Z_BACK			 9
#define	 MOVE_XY				 10
#define	 MOVE_XZ				 11
#define	 MOVE_YZ				 12
#define	 MOVE_XY_BACK			 13
#define	 MOVE_XZ_BACK			 14
#define	 MOVE_YZ_BACK			 15
#define	 REST 					 16
#define  POS_1  				 17
#define  POS_2  				 18
#define  POS_3  				 19
#define  POS_4  				 20
#define  POS_5  				 21




int main() {
	if(flag_simulation)
	{
		JOINT_TORQUES_COMMANDED_KEY = "sai2::DemoApplication::Panda::actuators::fgc";
		JOINT_ANGLES_KEY  = "sai2::DemoApplication::Panda::sensors::q";
		JOINT_VELOCITIES_KEY = "sai2::DemoApplication::Panda::sensors::dq";
		JOINT_ACCELERATIONS_KEY = "sai2::DemoApplication::Panda::sensors::ddq";

		EE_FORCE_SENSOR_FORCE_KEY = "sai2::DemoApplication::Panda::simulation::virtual_force";

		// LOCAL_GRAVITY_KEY =  "sai2::DemoApplication::simulation::Panda::g_local";
		QUATERNION_KEY = "sai2::DemoApplication::simulation::Panda::controller::logging::quaternion";
		

		//InertialParameters
		INERTIAL_PARAMS_KEY = "sai2::DemoApplication::Panda::controller::phi";
		INERTIAL_PARAMS_LS_KEY = "sai2::DemoApplication::Panda::controller::phiLS";
		INERTIAL_PARAMS_DEBUG_KEY = "sai2::DemoApplication::Panda::controller::phidebug";

		//Kinematics
		POSITION_KEY = "sai2::DemoApplication::Panda::kinematics::pos";
		LINEAR_VEL_KIN_KEY = "sai2::DemoApplication::Panda::kinematics::vel";
		LINEAR_ACC_KIN_KEY = "sai2::DemoApplication::Panda::kinematics::accel";
		ORIENTATION_QUATERNION_KEY =  "sai2::DemoApplication::Panda::kinematics::ori::quats";
		ANGULAR_VEL_KIN_KEY = "sai2::DemoApplication::Panda::kinematics::avel";
		ANGULAR_ACC_KIN_KEY = "sai2::DemoApplication::Panda::kinematics::aaccel";



		LINEAR_ACC_KEY = "sai2::DemoApplication::Panda::simulation::linear_acc";
		ANGULAR_VEL_KEY = "sai2::DemoApplication::Panda::simulation::angular_vel";
		ANGULAR_ACC_KEY = "sai2::DemoApplication::Panda::simulation::angular_acc";
		LOCAL_GRAVITY_KEY =  "sai2::DemoApplication::Panda::simulation::g_local";

	}
	else
	{
		JOINT_TORQUES_COMMANDED_KEY = "sai2::FrankaPanda::Clyde::actuators::fgc";
		EE_FORCE_SENSOR_FORCE_KEY = "sai2::optoforceSensor::6Dsensor::force";
		JOINT_ANGLES_KEY  = "sai2::FrankaPanda::Clyde::sensors::q";
		JOINT_VELOCITIES_KEY = "sai2::FrankaPanda::Clyde::sensors::dq";
		MASSMATRIX_KEY = "sai2::FrankaPanda::Clyde::sensors::model::massmatrix";
		CORIOLIS_KEY = "sai2::FrankaPanda::Clyde::sensors::model::coriolis";
		ROBOT_GRAVITY_KEY = "sai2::FrankaPanda::Clyde::sensors::model::robot_gravity";
		ACCELEROMETER_DATA_KEY = "sai2::3spaceSensor::data::accelerometer";      
		GYROSCOPE_DATA_KEY ="sai2::3spaceSensor::data::gyroscope";    

		//corrected sensor data(accelerometer: gravity removed, right frame, Gyroscope: right frame)
		LINEAR_ACCELERATION_LOCAL_KEY = "sai2::DemoApplication::FrankaPanda::controller::accel";
		ANGULAR_VELOCITY_LOCAL_KEY = "sai2::DemoApplication::FrankaPanda::controller::avel";
		QUATERNION_KEY = "sai2::DemoApplication::Panda::controller::quaternion";
		POSITION_KEY = "sai2::DemoApplication::FrankaPanda::controller::pos";

		LINEAR_VEL_KIN_KEY = "sai2::DemoApplication::FrankaPanda::Clyde::kinematics::vel";
		LINEAR_ACC_KIN_KEY = "sai2::DemoApplication::FrankaPanda::Clyde::kinematics::accel";
		ANGULAR_VEL_KIN_KEY = "sai2::DemoApplication::Panda::sensors::avel";
		ANGULAR_ACC_KIN_KEY = "sai2::DemoApplication::Panda::sensors::aaccel";

		LINEAR_VEL_KF_KEY = "sai2::DemoApplication::FrankaPanda::Clyde::KF::velocity";
		
	}

	//Read Bias file and write force torque bias in "force_torque_bias" vector
	VectorXd force_moment = VectorXd::Zero(6);
	VectorXd force_torque_bias = VectorXd::Zero(6); //FT Bias
	ifstream bias;
	bias.open("../../02-utilities/FT_data1.txt");
	if (!bias)  
	{                     // if it does not work
        cout << "Can't open Data!" << endl;
    }
    else
    {
    	
    	for (int row=0 ; row<6; row++)
    	{
    		double value = 0.0;
    		bias >> value;
    		force_torque_bias(row) = value;
    	}
    cout << "bias read" << force_torque_bias << endl;
    bias.close();
	}
    cout << "test" << endl;			
    // start redis client
	auto redis_client = RedisClient();
	redis_client.connect();

	// set up signal handler
	signal(SIGABRT, &sighandler);
	signal(SIGTERM, &sighandler);
	signal(SIGINT, &sighandler);



	// load robots
	auto robot = new Sai2Model::Sai2Model(robot_file, true);

	// read from Redis
	robot->_q = redis_client.getEigenMatrixJSON(JOINT_ANGLES_KEY);
	robot->_dq = redis_client.getEigenMatrixJSON(JOINT_VELOCITIES_KEY);
	if(flag_simulation)
	{
		robot->_ddq = redis_client.getEigenMatrixJSON(JOINT_ACCELERATIONS_KEY);
	}

	////////////////////////////////////////////////
	///        Prepare the controllers         /////
	////////////////////////////////////////////////

	robot->updateModel();


	int dof = robot->dof();
	VectorXd command_torques = VectorXd::Zero(dof);
	VectorXd coriolis = VectorXd::Zero(dof);
	MatrixXd N_prec = MatrixXd::Identity(dof,dof);



	Vector3d vel_sat = Vector3d(0.2,0.2,0.2);
	Vector3d avel_sat = Vector3d(M_PI/3.0, M_PI/3.0, M_PI/3.0);
	// pos ori controller
	const string link_name = "link7";
	const Eigen::Vector3d pos_in_link = Vector3d(0,0,0.15);
	auto posori_task = new Sai2Primitives::PosOriTask(robot, link_name, pos_in_link);
	posori_task->_max_velocity = 0.11;

	posori_task->_kp_pos = 100.0;
	posori_task->_kv_pos = 2.1*sqrt(posori_task->_kp_pos);
	posori_task->_kp_ori = 70.0;
	posori_task->_kv_pos = 2.1*sqrt(posori_task->_kp_ori);
	posori_task->_velocity_saturation = true;
	posori_task->_linear_saturation_velocity = vel_sat;
	posori_task->_angular_saturation_velocity = avel_sat;
	VectorXd posori_task_torques = VectorXd::Zero(dof);


	// Vector3d pos_des_1 = Vector3d(.5,0.4,0.1);
	// Vector3d pos_des_2 = Vector3d(.5,-0.4,0.6);
	// Vector3d pos_des_3 = Vector3d(.5,-0.4,0.1);
	// Vector3d pos_des_4 = Vector3d(.5,0.4,0.6);
	Vector3d pos_des_1 = Vector3d(.5,-0.4,0.5);
	Vector3d pos_des_2 = Vector3d(.5,-0.2,0.2);
	Vector3d pos_des_3 = Vector3d(.5,0.0,0.5);
	Vector3d pos_des_4 = Vector3d(.5,0.2,0.2);
	Vector3d pos_des_5 = Vector3d(.5,0.4,0.5);

	double delta_x = 0.1;
	double delta_y = 0.1;
	double delta_z = 0.1;

	double x_des = 0.7;
	double y_des = 0.5;
	double z_des = 0.6;

	double x_des_back =  0.1;
	double y_des_back = -0.5;
	double z_des_back =  0.2;

	//joint controller
	auto joint_task = new Sai2Primitives::JointTask(robot);
	joint_task->_max_velocity = M_PI/4.0;
	joint_task->_kp = 50.0;
	joint_task->_kv = 2.4 * sqrt(joint_task->_kp);
	VectorXd joint_task_torques = VectorXd::Zero(dof);
	VectorXd desired_initial_configuration = VectorXd::Zero(dof);
	desired_initial_configuration << 0,  -45, 0, -115, 0, 60, 0;
	// desired_initial_configuration << 0, 10, 0, -125, 0, 135, 0;


	desired_initial_configuration *= M_PI/180.0;
	joint_task->_goal_position = desired_initial_configuration;


	Matrix3d current_orientation = Matrix3d::Zero();



	Matrix3d R_link = Matrix3d::Zero();

	Vector3d kf_vel = Vector3d::Zero();
	Vector3d accel_kin = Vector3d::Zero();

	//For Inertial Parameter Estimation


	double lambda_factor = 0.07;
	double lambda_factor_2 = 0.07;
	MatrixXd Lambda = lambda_factor*MatrixXd::Identity(6,6);
	MatrixXd Lambda_2 = lambda_factor_2 * MatrixXd::Identity(6,6);
	// Lambda << 0.014,  0.0, 0.0, 0.0, 0.0, 0.0,
	// 		    0.0, 0.014, 0.0, 0.0, 0.0, 0.0,
	//           0.0, 0.0, 0.014, 0.0, 0.0, 0.0,
	//           0.0, 0.0, 0.0, 0.012, 0.0, 0.0,
	//           0.014, 0.0, 0.0, 0.0, 0.012, 0.0,
	//           0.014, 0.0, 0.0, 0.0, 0.0, 0.017;

	int filter_size = 3;
	int filter_size_2 =10;

	auto RLS_2 = new ParameterEstimation::RecursiveLeastSquare(filter_size_2,Lambda_2);
	auto RLS = new ParameterEstimation::RecursiveLeastSquare(filter_size,Lambda);


	Vector3d accel = Vector3d::Zero(); //object linear acceleration in base frame
	Vector3d avel = Vector3d::Zero(); //object angular velocity in base frame
	Vector3d aaccel = Vector3d::Zero(); //object angular acceleration in base frame
	Vector3d accel_local = Vector3d::Zero(); // object linear acceleration in sensor frame
	Vector3d aaccel_local = Vector3d::Zero(); // object angular acceleration in sensor frame
	Vector3d avel_local = Vector3d::Zero(); //object angular velocity in sensor frame
	Vector3d g_local = Vector3d::Zero(); //gravity vector in sensor frame
	VectorXd phi_RLS = VectorXd::Zero(10); //inertial parameter vector
	VectorXd phi_RLS_2 = VectorXd::Zero(10); //inertial parameter vector
	Vector3d force_sensed = Vector3d::Zero();
	Matrix3d inertia_tensor_RLS = Matrix3d::Zero();
	Vector3d center_of_mass_RLS = Vector3d::Zero();
	Matrix3d inertia_tensor_RLS_2 = Matrix3d::Zero();
	Vector3d center_of_mass_RLS_2 = Vector3d::Zero();




	//position and orientation
	Vector3d position =  Vector3d::Zero();
	Vector3d velocity =  Vector3d::Zero();
	Matrix3d orientation = Matrix3d::Zero();
	Quaterniond orientation_quaternion;
	VectorXd orientation_quaternion_aux = VectorXd::Zero(4);


	int state = GOTO_INITIAL_CONFIG;

	// create a loop timer
	double control_freq = 1000;
	LoopTimer timer;
	timer.setLoopFrequency(control_freq);   // 1 KHz
	// timer.setThreadHighPriority();  // make timing more accurate. requires running executable as sudo.
	timer.setCtrlCHandler(sighandler);    // exit while loop on ctrl-c
	timer.initializeTimer(1000000); // 1 ms pause before starting loop
	// create timer
	chrono::high_resolution_clock::time_point t_start;
	chrono::duration<double> t_elapsed;
	// while window is open:
	while (runloop) {

		// wait for next scheduled loop
		timer.waitForNextLoop();


		// read from Redis
		robot->_q = redis_client.getEigenMatrixJSON(JOINT_ANGLES_KEY);
		robot->_dq = redis_client.getEigenMatrixJSON(JOINT_VELOCITIES_KEY);

		force_moment = redis_client.getEigenMatrixJSON(EE_FORCE_SENSOR_FORCE_KEY);
		force_sensed << force_moment(0), force_moment(1), force_moment(2);
		// robot->rotation(R_link,link_name);  // g_local =
		R_link.transpose()*robot->_world_gravity; // update robot model
		if(flag_simulation) 
		{     
			robot->_ddq =redis_client.getEigenMatrixJSON(JOINT_ACCELERATIONS_KEY);

			robot->updateModel();
			robot->coriolisForce(coriolis);

			accel_local = redis_client.getEigenMatrixJSON(LINEAR_ACC_KEY);
			avel_local = redis_client.getEigenMatrixJSON(ANGULAR_VEL_KEY);
			aaccel_local = redis_client.getEigenMatrixJSON(ANGULAR_ACC_KEY);
			g_local = redis_client.getEigenMatrixJSON(LOCAL_GRAVITY_KEY);
			// robot->linearAcceleration(accel,link_name, Vector3d::Zero());
			// robot->angularAcceleration(aaccel,link_name);
			// robot->angularVelocity(avel,link_name);

			// accel_local = R_link.transpose()*accel;
			// accel_local += g_local;
		 // 	aaccel_local = R_link.transpose()*aaccel;
			// avel_local = R_link.transpose()*avel;	


			// orientation = R_link.transpose();
			// orientation_quaternion = orientation;
			// orientation_quaternion.normalize();
			// orientation_quaternion_aux << orientation_quaternion.w() , orientation_quaternion.vec();
			// robot->position(position, link_name, Vector3d::Zero());
			// position = R_link.transpose()*position;
			// robot->linearVelocity(velocity, link_name);
			// velocity = R_link.transpose()*velocity;

		}
		else
		{
			 robot->updateKinematics();
			 robot->_M = redis_client.getEigenMatrixJSON(MASSMATRIX_KEY);
			 if(inertia_regularization)
			 {
			 	robot->_M(4,4) += 0.07;
			 	robot->_M(5,5) += 0.07;
			 	robot->_M(6,6) += 0.07;
			 }
			 robot->_M_inv = robot->_M.inverse();

			 coriolis = redis_client.getEigenMatrixJSON(CORIOLIS_KEY);

		}



		if(state != GOTO_INITIAL_CONFIG)
		{	

			if(controller_counter%2 == 0)
			{
				RLS_2->addData(force_moment, accel_local, avel_local, aaccel_local, g_local);
				phi_RLS_2 = RLS_2->getInertialParameterVector();


				RLS->addData(force_moment, accel_local, avel_local, aaccel_local, g_local);
				phi_RLS = RLS->getInertialParameterVector();


			// cout << "ft: " <<  force_moment.transpose() << " a: " << accel_local.transpose() << " omega: "<<  avel_local.transpose() << " alpha: " << aaccel_local.transpose() << " phi " << phi_RLS.transpose() << endl; 
			center_of_mass_RLS << phi_RLS(1)/phi_RLS(0), phi_RLS(2)/phi_RLS(0), phi_RLS(3)/phi_RLS(0); 
			inertia_tensor_RLS << phi_RLS(4), phi_RLS(5), phi_RLS(6), phi_RLS(5), phi_RLS(7), phi_RLS(8), phi_RLS(6), phi_RLS(8), phi_RLS(9);
			// cout << "1 current inertial parameters for filter size  " <<  filter_size << " and lambda_factor: " << lambda_factor << " mass: "<<  phi_RLS(0) << " center of mass: " << center_of_mass_RLS.transpose() << " inertia tensor:  "  << inertia_tensor_RLS << endl; 

			center_of_mass_RLS_2 << phi_RLS_2(1)/phi_RLS_2(0), phi_RLS_2(2)/phi_RLS_2(0), phi_RLS_2(3)/phi_RLS_2(0); 
			inertia_tensor_RLS_2 << phi_RLS_2(4), phi_RLS_2(5), phi_RLS_2(6), phi_RLS_2(5), phi_RLS_2(7), phi_RLS_2(8), phi_RLS_2(6), phi_RLS_2(8), phi_RLS_2(9);

			// cout << "2 current inertial parameters for filter size  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 <<  " mass: "<<  phi_RLS(0) << " center of mass: " << center_of_mass_RLS_2.transpose() << " inertia tensor:  "  << inertia_tensor_RLS_2 << endl; 

			}



		}
if (controller_counter % 500 == 0)
{
				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
}


		if(state == GOTO_INITIAL_CONFIG)
		{	

			// update tasks models
			N_prec.setIdentity();
			joint_task->updateTaskModel(N_prec);

			// compute task torques
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques + coriolis;

			VectorXd config_error = desired_initial_configuration - joint_task->_current_position;
			if(config_error.norm() < 0.1)
			{

				joint_task->reInitializeTask();	
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);
				posori_task->_goal_position = pos_des_1;		    
			    joint_task->_goal_position(6) = M_PI;


			    state = POS_1;
				
			}
		}

		else if(state == MOVE_FIRST)
		{
			posori_task->_goal_position = pos_des_1;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
				posori_task->_goal_position = pos_des_2;
			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = MOVE_SECOND;		
			}


		}

		
		else if(state == MOVE_SECOND)
		{

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
				posori_task->_goal_position = pos_des_3;
			    joint_task->_goal_position(6) -= 2.0*M_PI;

				state = MOVE_THIRD;		
			}


		}
		else if(state == MOVE_THIRD)
		{

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
				posori_task->_goal_position = pos_des_4;
			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = REST;		
			}


		}


		else if(state == MOVE_X)
		{

			posori_task->_goal_position(0) = x_des;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = MOVE_Y;		
			}


		}

		else if(state == MOVE_Y)
		{

			posori_task->_goal_position(1) = y_des;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = MOVE_Z;		
			}
		}
		else if(state == MOVE_Z)
		{

			posori_task->_goal_position(2) = z_des;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = MOVE_X_BACK;		
			}
		}

		else if(state == MOVE_X_BACK)
		{

			posori_task->_goal_position(0) = x_des_back;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = MOVE_Y_BACK;		
			}
		}

		else if(state == MOVE_Y_BACK)
		{

			posori_task->_goal_position(1) = y_des_back;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = MOVE_Z_BACK;		
			}
		}

		else if(state == MOVE_Z_BACK)
		{

			posori_task->_goal_position(2) = z_des_back;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = REST;		
			}
		}

		else if(state == MOVE_XY)
		{
			posori_task->_goal_position(0) = x_des;
			posori_task->_goal_position(1) = y_des;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			
			    joint_task->_goal_position(6) = M_PI;

				state = MOVE_YZ_BACK;		
			}
		}

		else if(state == MOVE_XY_BACK)
		{
			posori_task->_goal_position(0) = x_des_back;
			posori_task->_goal_position(1) = y_des_back;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;

			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = MOVE_Z_BACK;		
			}
		}

		else if(state == MOVE_XZ)
		{
			posori_task->_goal_position(0) = x_des;
			posori_task->_goal_position(2) = z_des;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;

			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = MOVE_XZ_BACK;		
			}
		}

		else if(state == MOVE_XZ_BACK)
		{
			posori_task->_goal_position(0) = x_des_back;
			posori_task->_goal_position(2) = z_des_back;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;

			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = REST;		
			}
		}

		else if(state == MOVE_YZ)
		{
			posori_task->_goal_position(1) = y_des;
			posori_task->_goal_position(2) = z_des;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;

			    joint_task->_goal_position(6) += 2.0*M_PI;

				state = REST;		
			}
		}

		else if(state == MOVE_YZ_BACK)
		{
			posori_task->_goal_position(1) = y_des_back;
			posori_task->_goal_position(2) = z_des_back;

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;

			VectorXd config_error_pos = posori_task->_goal_position - posori_task->_current_position;
			if(config_error_pos.norm() < 0.2)
			{	
				joint_task->reInitializeTask();
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;

			    joint_task->_goal_position(6) = -M_PI;

				state = REST;		
			}
		}
				if(state == POS_1)
		{	

			// update tasks models
			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute task torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques + posori_task_torques+ coriolis;

			VectorXd config_error = posori_task->_goal_position - posori_task->_current_position;
			if(config_error.norm() < 0.02)
			{
				joint_task->reInitializeTask();			
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);    
				posori_task->_goal_position = pos_des_2;
				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;
			

			    state = POS_2;
				
			}
		}
		if(state == POS_2)
		{	

			// update tasks models
			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute task torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques + posori_task_torques+ coriolis;

			VectorXd config_error = posori_task->_goal_position - posori_task->_current_position;
			if(config_error.norm() < 0.02)
			{
				joint_task->reInitializeTask();			
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);    
				posori_task->_goal_position = pos_des_3;


				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;

			    state = POS_3;
				
			}
		}
		if(state == POS_3)
		{	

			// update tasks models
			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute task torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques + posori_task_torques+ coriolis;

			VectorXd config_error = posori_task->_goal_position - posori_task->_current_position;
			if(config_error.norm() < 0.02)
			{
				joint_task->reInitializeTask();			
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);    
				posori_task->_goal_position = pos_des_4;


				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;

			    state = POS_4;
				
			}
		}
		if(state == POS_4)
		{	

			// update tasks models
			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute task torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques + posori_task_torques+ coriolis;

			VectorXd config_error = posori_task->_goal_position - posori_task->_current_position;
			if(config_error.norm() < 0.02)
			{
				joint_task->reInitializeTask();			
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);    
				posori_task->_goal_position = pos_des_5;

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;

			    state = POS_5;
				
			}
		}
		if(state == POS_5)
		{	

			// update tasks models
			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute task torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques + posori_task_torques+ coriolis;

			VectorXd config_error = posori_task->_goal_position - posori_task->_current_position;
			if(config_error.norm() < 0.02)
			{
				joint_task->reInitializeTask();			
				posori_task->reInitializeTask();
				posori_task->enableVelocitySaturation(vel_sat, avel_sat);    

				cout << "RLS_1 for filter size:  " <<  filter_size << " and lambda_factor: " << lambda_factor << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;


				cout << "RLS_2 for filter size:  " <<  filter_size_2 << " and lambda_factor: " << lambda_factor_2 << endl;
				cout << "for state " << state << " the estimated mass is: \n" << phi_RLS_2(0) << endl;
			    cout << "for state " << state << " the estimated center of mass is: \n" << center_of_mass_RLS_2.transpose() << endl;
			    cout << "for state " << state << " the estimated inertia tensor is: \n" << inertia_tensor_RLS_2 << endl;

			    state = GOTO_INITIAL_CONFIG;
				
			}
		}



		else if(state == REST)
		{

			N_prec.setIdentity();
			posori_task->updateTaskModel(N_prec);
			N_prec = posori_task->_N;
			joint_task->updateTaskModel(N_prec);

			// compute torques
			posori_task->computeTorques(posori_task_torques);
			joint_task->computeTorques(joint_task_torques);

			command_torques = joint_task_torques  + posori_task_torques + coriolis;
		}

		redis_client.setEigenMatrixDerived(JOINT_TORQUES_COMMANDED_KEY, command_torques);
		// redis_client.setEigenMatrixDerived(LOCAL_GRAVITY_KEY, g_local);

		redis_client.setEigenMatrixDerived(INERTIAL_PARAMS_KEY, phi_RLS);


		redis_client.setEigenMatrixDerived(POSITION_KEY, position);
		redis_client.setEigenMatrixDerived(LINEAR_VEL_KIN_KEY, velocity);
		redis_client.setEigenMatrixDerived(LINEAR_ACC_KIN_KEY, accel_local);
		redis_client.setEigenMatrixDerived(ORIENTATION_QUATERNION_KEY, orientation_quaternion_aux);
		redis_client.setEigenMatrixDerived(ANGULAR_VEL_KIN_KEY, avel_local);
		redis_client.setEigenMatrixDerived(ANGULAR_ACC_KIN_KEY, aaccel_local);

		redis_client.setEigenMatrixDerived(EE_FORCE_SENSOR_FORCE_KEY, force_moment);


		

		controller_counter++;

	}

    command_torques << 0,0,0,0,0,0,0;
    redis_client.setEigenMatrixDerived(JOINT_TORQUES_COMMANDED_KEY, command_torques);

    double end_time = timer.elapsedTime();
    cout << "\n";
    cout << "Loop run time  : " << end_time << " seconds\n";
    cout << "Loop updates   : " << timer.elapsedCycles() << "\n";
    cout << "Loop frequency : " << timer.elapsedCycles()/end_time << "Hz\n";

	cout << "the estimated mass is: \n" << phi_RLS(0) << endl;
    cout << "the estimated center of mass is: \n" << center_of_mass_RLS.transpose() << endl;
    cout << "the estimated inertia tensor is: \n" << inertia_tensor_RLS << endl;

    return 0;

}






