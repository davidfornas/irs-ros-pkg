#include "ARM5UJIONEControl.h"
#include <visp/vpTime.h>
#include <std_msgs/Int32.h>
#include <std_msgs/Float32.h>

#include <pthread.h>

void *readfunc(void *coms) {
   while (1) {
	((ARM5Coms*)coms)->readMessage();
	usleep(20000);
  }
}

ARM5Control::ARM5Control()
{
  coms= new ARM5Coms();
  std::cerr << "Opening port" << std::endl;
  coms_fd=coms->OpenPort("/dev/ttyS0");
  std::cerr << "ARM5Control::ARM5Control Port opened with fd " << coms_fd << std::endl;

  coms->Channel1.ValuePID(0,0,0,255,4,0);
  coms->Channel2.ValuePID(0,0,0,255,4,0);
  coms->Channel3.ValuePID(0,0,0,255,4,0);
  coms->Channel4.ValuePID(0,0,0,255,4,0);
  coms->Channel5.ValuePID(0,0,0,255,4,0);

  axis_lmin.resize(5);
  axis_ratios.resize(5);
  axis_d.resize(5);
  axis_r.resize(5);
  axis_aoffsets.resize(5);
  model_offsets.resize(5);
  axis_ranges.resize(5);
  for (int i=0; i<5; i++) {
	axis_lmin[i]=lmin[i];
	axis_ratios[i]=ratios[i];
	axis_d[i]=d[i];
	axis_r[i]=r[i];
	axis_aoffsets[i]=aoffsets[i];
	model_offsets[i]=moffsets[i];
	axis_ranges[i]=ranges[i];
  }

  
  tick_offsets.resize(5);
  rawticks.resize(5);
  rticks.resize(5);
  ticks.resize(5); 
  axis_lengths.resize(5);
  axis_alpha.resize(5);
  q.resize(5);
  qdot.resize(5);

  rpmlimit.resize(5);
  for (int i=0;i<5;i++) 
	rpmlimit[i]=RPM_LIMIT;
  setCurrentLimit(ARM5_CURRENT_LIMIT);
  setGraspCurrentLimit(ARM5_GRASPCURRENT_LIMIT);
  setSpeedLimit(ARM5_SPEED_LIMIT);
 
  tick_offsets=0;
  rticks=0;
  ticks=0;
  axis_lengths=0;
  axis_alpha=0;
  q=0;
  qdot=0;
  position_ujione=0;
  offsets_defined=false;
  securityStopEnabled=true;
  emergencyStop=false;
  emergencyStopEnabled=false;
  lastSignalTimer=vpTime::measureTimeSecond();

  maxCurrentEnabled=false;

  service = nh_.advertiseService("setZero", &ARM5Control::setZero,this);

  js_rawticks_pub = nh_.advertise<sensor_msgs::JointState>("/arm5e/joint_state_rawticks",1);
  js_rticks_pub = nh_.advertise<sensor_msgs::JointState>("/arm5e/joint_state_rticks",1);
  js_ticks_pub = nh_.advertise<sensor_msgs::JointState>("/arm5e/joint_state_ticks",1);
  js_ticks_sub = nh_.subscribe<sensor_msgs::JointState>("/arm5e/command_ticks", 10, &ARM5Control::commandTicks, this);
  js_length_pub = nh_.advertise<sensor_msgs::JointState>("/arm5e/joint_state_length",1);
  js_length_sub = nh_.subscribe<sensor_msgs::JointState>("/arm5e/command_length", 10, &ARM5Control::commandlength, this);
  js_angle_pub = nh_.advertise<sensor_msgs::JointState>("/arm5e/joint_state_angle",1);
  js_angle_sub = nh_.subscribe<sensor_msgs::JointState>("/arm5e/command_angle", 10, &ARM5Control::commandAngle, this);
  PIDService = nh_.advertiseService("/arm5e/setPID", &ARM5Control::setPID_callback, this);
  //ParamService = nh_.advertiseService("/arm5e/setParams", &ARM5Control::setParams_callback, this);

  ujione_vel=nh_.advertise<std_msgs::Int32>("/dynamixel/turn",1);
  //ujione_position=nh_.subscribe<std_msgs::Float32>("/ujione_hand/pos", 1, &ARM5Control::ujionePosition, this);

  //Start read thread
//  pthread_t readt;
//  pthread_create(&readt,NULL,readfunc, coms);

  //Set PIDS, and set zero velocity
  controlCycle();
  setTicksVelocity(0,0,0,0,0);
}

bool ARM5Control::setPID_callback(ARM5Controller::setPID::Request  &req,
	ARM5Controller::setPID::Response &res )
{
  coms->Channel2.ValuePID(0,0,0,req.PID[0],req.PID[1],req.PID[2]);
  coms->Channel2.pidupdate();
  coms->Channel1.ValuePID(0,0,0,req.PID[0],req.PID[1],req.PID[2]);
  coms->Channel1.pidupdate();
  coms->Channel3.ValuePID(0,0,0,req.PID[0],req.PID[1],req.PID[2]);
  coms->Channel3.pidupdate();
  coms->Channel4.ValuePID(0,0,0,req.PID[0],req.PID[1],req.PID[2]);
  coms->Channel4.pidupdate();
  coms->Channel5.ValuePID(0,0,0,req.PID[0],req.PID[1],req.PID[2]);
  coms->Channel5.pidupdate();

  res.success=true;
  return true;
}

/*
bool ARM5Control::setParams_callback(ARM5Controller::setParams::Request  &req,
	ARM5Controller::setParams::Response &res )
{
	this->securityStopEnabled=req.securityStopEnabled;
	res.success=true;
	return true;
}
*/

void ARM5Control::setTicksVelocity(vpColVector &rpm) {
  //security measure: stop if there is an emergency:
  static vpColVector lastrpm(5);
  //if (lastrpm.euclideanNorm()!=0 && rpm.euclideanNorm()!=0 && (rpm-lastrpm).euclideanNorm()>ARM5_VELDISCONT_LIMIT) {
//	std::cerr << "WARNING: High velocity discontinuity. Stopping the arm" << std::endl;		
//	coms->Channel2.SpeedDemand(0, slimit, climit);
//	coms->Channel1.SpeedDemand(0, slimit, climit);
//	coms->Channel3.SpeedDemand(0, slimit, climit);
//	coms->Channel4.SpeedDemand(0, slimit, climit);
//	coms->Channel5.SpeedDemand(0, slimit, gclimit);
  if (emergencyStop) {
	std::cerr << "WARNING: Emergency stop. High current detected." << std::endl;
	coms->Channel2.SpeedDemand(0, slimit, climit);
	coms->Channel1.SpeedDemand(0, slimit, climit);
	coms->Channel3.SpeedDemand(0, slimit, climit);
	coms->Channel4.SpeedDemand(0, slimit, climit);
	coms->Channel5.SpeedDemand(0, slimit, gclimit);
   } else {
	lastrpm=rpm;
	
	//check the difference between current motor velocity and the limit velocity
        double maxdiff=fabs(rpm[0])-fabs(rpmlimit[0]);
        int maxindex=0;
        vpColVector filtered_rpm(5);
        for (unsigned int i=1;i<5;i++) {
                if ((fabs(rpm[i])-fabs(rpmlimit[i]))>maxdiff) {
                  maxdiff=fabs(rpm[i])-fabs(rpmlimit[i]);
                  maxindex=i;
                }
        }
        //if there is a motor velocity above the limit, scale all of them in order to keep them below the limit
        double ratio=0;
        if (maxdiff>0 && rpm[maxindex]!=0) {
                std::cerr << "WARNING: Motor " << maxindex << " (starting from 0) is in velocity limit: " << rpm[maxindex] << " RPM. Scaling..." << std::endl;
                ratio=fabs(rpmlimit[maxindex])/fabs(rpm[maxindex]);
        } else ratio=1;

        for (int i=0; i<5; i++)
                filtered_rpm[i]=ratio*rpm[i];

	//TODO: If there is a joint near the limit do not allow further motion on that direction.
	if (offsets_defined) {
		bool near_limit=false;
		int near_limit_joint=0;
	
		filtered_rpm[2]=-filtered_rpm[2]; //Invert sign for elbow
		for (int i=0; i<3; i++) {
			if (((ranges[i]+minlimits[i])-q[i]<LIMIT_SECURITY_RANGE && filtered_rpm[i]>0) || ((q[i]-minlimits[i])<LIMIT_SECURITY_RANGE && filtered_rpm[i]<0)) {
				near_limit=true;
				near_limit_joint=i;
			} 
		}
		filtered_rpm[2]=-filtered_rpm[2]; //Restore sign for elbow
		
		if (near_limit) {
			filtered_rpm=0;
			std::cerr << "WARNING: Aborting velocity on joint " << near_limit_joint << ". Close to limit." << std::endl;
		}
	}

	coms->Channel2.SpeedDemand((int)filtered_rpm[0],slimit, climit);
	coms->Channel1.SpeedDemand((int)filtered_rpm[1],slimit, climit);
	coms->Channel3.SpeedDemand((int)filtered_rpm[2],slimit, climit);
	coms->Channel4.SpeedDemand((int)filtered_rpm[3],slimit, climit);
	coms->Channel5.SpeedDemand((int)filtered_rpm[4],slimit, gclimit);
  }
  lastSignalTimer=vpTime::measureTimeSecond();
}

void ARM5Control::setTicksVelocity(double rpm0, double rpm1, double rpm2, double rpm3, double rpm4) {
	vpColVector rpm(5);
	rpm[0]=rpm0;
	rpm[1]=rpm1;
	rpm[2]=rpm2;
	rpm[3]=rpm3;
	rpm[4]=rpm4;
	setTicksVelocity(rpm);
}

/*
void ARM5Control::setTicksPosition(double q0, double q1, double q2, double q3, double q4) {
	vpColVector qref(5);
	qref[0]=q0;
	qref[1]=q1;
	qref[2]=q2;
	qref[3]=q3;
	qref[4]=q4;
	while ((qref-q).euclideanNorm()>10) {
		//Control towards qref until tick error below 10
		vpColVector ticksVel(5);
		ticksVel=(qref-q);
		setTicksVelocity(ticksVel[0],ticksVel[1],ticksVel[2],ticksVel[3],ticksVel[4]);
		
	}
}
*/


bool ARM5Control::setZero(ARM5Controller::setZero::Request  &req,
			  ARM5Controller::setZero::Response &res )
{
	if (req.zeroOffsets.size()==5) {
		for (int i=0; i<5; i++) tick_offsets[i]=req.zeroOffsets[i];
		offsets_defined=true;
		res.success=true;
	} else {
		ROS_INFO("ARM5Control::setZero ERROR: Wrong size of zero offset vector");
		res.success=false;
	}
	return true;
}




void ARM5Control::commandTicks(const sensor_msgs::JointState::ConstPtr& js)
{
  //TODO: position control

  //ticks velocity control
  if (js->velocity.size()==5) {
	setTicksVelocity(js->velocity[0],js->velocity[1],js->velocity[2],js->velocity[3],js->velocity[4]);
  } else {
	ROS_ERROR("ARM5Control::commandTicks: Input JointState message contains more than 5 values.");
  }

}

void ARM5Control::commandlength(const sensor_msgs::JointState::ConstPtr& js)
{
  //TODO: position control

  //TODO: axis translation velocity control
  //Convert axis translation velocity to RPM and call SpeedDemand
  //m/s ---> RPM. Depends just on gear ratio

  //coms->sendMessage();
  //coms->readMessage();
}

void ARM5Control::commandAngle(const sensor_msgs::JointState::ConstPtr& js)
{
  //position control: rad ---> m ----> motor ticks
  if (js->position.size()>0  && js->velocity.size()==0) {
	if (offsets_defined) {
	  vpColVector aq(5), alpha(5), jlen(5), absticks(5);
	  for (int i=0; i<5; i++)
		aq[i]=js->position[i]-model_offsets[i];
	  alpha=aq+axis_aoffsets;
	  alpha[2]=-aq[2]+axis_aoffsets[2];	//change sign of elbow

	  //inverse model (given the desired alpha angle, compute the axis linear translation, and finally the position ticks)
	  jlen[0]=sqrt(axis_d[0]*axis_d[0]+axis_r[0]*axis_r[0] - 2 * axis_d[0]*axis_r[0]*cos(alpha[0]));
	  absticks[0]=(jlen[0]-axis_lmin[0])/axis_ratios[0];
	  jlen[1]=sqrt(axis_d[1]*axis_d[1]+axis_r[1]*axis_r[1] - 2 * axis_d[1]*axis_r[1]*cos(alpha[1]));
	  absticks[1]=(jlen[1]-axis_lmin[1])/axis_ratios[1];
	  jlen[2]=sqrt(axis_d[2]*axis_d[2]+axis_r[2]*axis_r[2] - 2 * axis_d[2]*axis_r[2]*cos(alpha[2]));
	  absticks[2]=(jlen[2]-axis_lmin[2])/axis_ratios[2];
	  jlen[3]=0;	//revolute joint (wrist)
	  absticks[3]=aq[3]/axis_ratios[3];
	  jlen[4]=0;
	  absticks[4]=aq[4]/axis_ratios[4];
	
	  //Simple proportional control towards a position reference
	  vpColVector rpm(5);
	  rpm=PGAIN*(absticks-ticks);
	  setTicksVelocity(rpm);
	  //std::cerr << "commandAngle: axisLength: " << jlen.t() << " absticks: " << absticks.t() << std::endl;  
	}
  } else if (js->velocity.size()>0) {
	  //angle velocity control
	  //rad/s --->  m/s ----> RPM.
	  if (offsets_defined) {
		  vpColVector ldot(5), rpm(5);
		  for (int i=0; i<5; i++) {
		    if (axis_d[i]!=0) {
			//inverse derivative model (given the current angle, compute the axis linear velocity for moving the joint at 1 rad/s
			ldot[i]=(axis_d[i]*axis_r[i]*sin(axis_alpha[i]))/(sqrt(axis_d[i]*axis_d[i]+axis_r[i]*axis_r[i]-2*axis_d[i]*axis_r[i]*cos(axis_alpha[i])));
			//Scale linear axis velocity to the reference velocity and convert to motor RPM
			rpm[i]=js->velocity[i]*ldot[i]/axis_ratios[i];
		    } else {
			//revolute joint (wrist)
			ldot[i]=0;
			rpm[i]=js->velocity[i]/axis_ratios[i];
		    }
		  }
		  rpm[2]=-rpm[2]; //Invert sign for elbow
		  //std::cerr << "Joint ldot: " << ldot.t() << std::endl;
		  //std::cerr << "Joint RPM: " << rpm.t() << std::endl;
		  std_msgs::Int32 m;
		  if(rpm[4]>100 ){
			  m.data=700;
			  ujione_vel.publish(m);
		  }
		  else if(rpm[4]<-100){
			  m.data=-700;
			  ujione_vel.publish(m);
		  }
		  else {
			  m.data=0;
			  ujione_vel.publish(m);
		  }
		  rpm[4]=0;
		  setTicksVelocity(rpm);
	  } else {
		setTicksVelocity(0,0,0,0,0);
	  }
  }
}

vpColVector lToAlpha(vpColVector &axis_l, vpColVector &axis_d, vpColVector &axis_r ) {
	vpColVector alpha(5);
	alpha[0]=acos((axis_d[0]*axis_d[0]+axis_r[0]*axis_r[0]-axis_l[0]*axis_l[0])/(2*axis_d[0]*axis_r[0]));
	alpha[1]=acos((axis_d[1]*axis_d[1]+axis_r[1]*axis_r[1]-axis_l[1]*axis_l[1])/(2*axis_d[1]*axis_r[1]));
	alpha[2]=acos((axis_d[2]*axis_d[2]+axis_r[2]*axis_r[2]-axis_l[2]*axis_l[2])/(2*axis_d[2]*axis_r[2]));
	alpha[3]=axis_l[3];
	alpha[4]=axis_l[4];
	return alpha;
}

void ARM5Control::controlCycle() {
	static double oldt=0;

	coms->sendMessage();
	usleep(10000);
	coms->readMessage();

	vpColVector old_rawticks(5), old_rticks(5);
	old_rawticks=rawticks;
	old_rticks=rticks;
	//get raw position ticks
	rawticks[0]=coms->Channel2.rawPosition();
	rawticks[1]=coms->Channel1.rawPosition();
	rawticks[2]=coms->Channel3.rawPosition();
	rawticks[3]=coms->Channel4.rawPosition();
	rawticks[4]=coms->Channel5.rawPosition();

	//get relative position ticks
	rticks[0]=coms->Channel2.disPosition();
	rticks[1]=coms->Channel1.disPosition();
	rticks[2]=coms->Channel3.disPosition();
	rticks[3]=coms->Channel4.disPosition();
	rticks[4]=coms->Channel5.disPosition();

	vpColVector oldq(5);
	oldq=0;
	if (offsets_defined) {
	  //get absolute ticks and compute absolute ticks velocities
	  ticks=rticks-tick_offsets;

	  //get axis lengths and compute axis lengths velocities
	  for (int i=0; i<5; i++) axis_lengths[i]=axis_lmin[i]+ticks[i]*axis_ratios[i];

	  //get angles and compute angle velocities
	  axis_alpha=lToAlpha(axis_lengths,axis_d,axis_r);
	  oldq=q;
	  q=axis_alpha-axis_aoffsets;
	  q[2]=-(axis_alpha[2]-axis_aoffsets[2]);	//Change sign to elbow
	  q=q+model_offsets;
	  //velocities:
	  double t=vpTime::measureTimeSecond();
	  double deltat=t-oldt;
	  //std::cerr << "Changed " << (q-oldq).t() << " in " << deltat << " sec" << std::endl;
	  oldt=t;
	  qdot=(q-oldq)/deltat;
	}

	//get master current
	masterCurrent=coms->MasterCurrent();

	//security measure: if there is a discontinuity in the position signal, the node needs to be restarted
//	if (offsets_defined && emergencyStopEnabled && (q-oldq).euclideanNorm()>0.2) {
//		std::cerr << "EMERGENCY STOP" << std::endl;
//		std::cerr << "Previous q euclideanNorm: " << oldq.euclideanNorm() << std::endl;
//		std::cerr << "Previous q: " << oldq.t() << std::endl;
//		std::cerr << "Current q: " << q.t() << std::endl;
//		std::cerr << "Previous rawticks: " << old_rawticks.t() << std::endl;
//		std::cerr << "Current rawticks: " << rawticks.t() << std::endl;
//		std::cerr << "Previous rticks: " << old_rticks.t() << std::endl;
//		std::cerr << "Current rticks: " << rticks.t() << std::endl;
//		emergencyStop=true;
//	}	
	if (offsets_defined && !emergencyStopEnabled) emergencyStopEnabled=true;

	//security measure: If the current is over a threshold, emergency stop
	if (masterCurrent>ARM5_SEC_CURRENT) {
	  if(maxCurrentEnabled==false){
	    maxCurrentTime=ros::Time::now().toSec();
	    maxCurrentEnabled=true;
	  }
	  else{
	    if((ros::Time::now().toSec()-maxCurrentTime)>=maxT){
	      emergencyStop=true;
	      maxCurrentEnabled=false;
	    }
	  }
	}
	else
	  maxCurrentEnabled=false;
	//setTicksVelocity(0,0,0,0,0);
	//emergencyStop=true;

	//security measure: if more than 500 ms without receiving a control signal, set a zero velocity on all the joints
	if (securityStopEnabled && (vpTime::measureTimeSecond()>(lastSignalTimer+0.5))) {
		setTicksVelocity(0,0,0,0,0);
		//std::cerr << "More than 500ms without control signal. Stopping arm." << std::endl;
	}
	
}


void ARM5Control::ujionePosition(const std_msgs::Float32::ConstPtr& m){
	position_ujione=m->data;
}


int main(int argc, char** argv)
{
	ros::init(argc, argv, "ARM5Control");

	ARM5Control csipcontrol;

	ros::Rate r(50);
	while (ros::ok()) {
		ros::spinOnce();

		csipcontrol.controlCycle();
			
		//Publish position, velocity, and master current, in ticks, lengths and angles
		//Master Current is replicated for each joint and placed into the effort field
		sensor_msgs::JointState js_rawticks, js_rticks, js_ticks, js_length, js_angle;
		for (int i=0; i<5; i++) {
			char name[4];
			sprintf(name,"q%d",i);
			js_rawticks.name.push_back(std::string(name));
			js_rticks.name.push_back(std::string(name));
			js_ticks.name.push_back(std::string(name));
			js_length.name.push_back(std::string(name));
			js_angle.name.push_back(std::string(name));

			js_rawticks.position.push_back(csipcontrol.rawticks[i]);
			js_rticks.position.push_back(csipcontrol.rticks[i]);
			js_ticks.position.push_back(csipcontrol.ticks[i]);
			js_length.position.push_back(csipcontrol.axis_lengths[i]);
			if(i==4){
				js_angle.position.push_back(csipcontrol.position_ujione);
			}
			else{
				js_angle.position.push_back(csipcontrol.q[i]);
			}
			js_angle.velocity.push_back(csipcontrol.qdot[i]);

			js_rawticks.effort.push_back(csipcontrol.masterCurrent);
			js_rticks.effort.push_back(csipcontrol.masterCurrent);
			js_ticks.effort.push_back(csipcontrol.masterCurrent);
			js_length.effort.push_back(csipcontrol.masterCurrent);
			js_angle.effort.push_back(csipcontrol.masterCurrent);
		}
		csipcontrol.js_rawticks_pub.publish(js_rawticks);
		csipcontrol.js_rticks_pub.publish(js_rticks);
		if (csipcontrol.offsets_defined) {
			csipcontrol.js_ticks_pub.publish(js_ticks);
			csipcontrol.js_length_pub.publish(js_length);
			csipcontrol.js_angle_pub.publish(js_angle);
		}

		r.sleep();
	}


//	robot.setVelocity(0,0,0,0,0);
//	robot.controlCycle();
}


