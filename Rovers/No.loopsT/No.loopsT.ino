//*the wheels rotate clockwise only (the robot doesn't move backward)
//*the integral part of the PID is restricted to not exceed half the maximum omega of the body, to prevent the accumulation of error and overshooting
//*the dt used by the integral and differential terms is fixed by making the PID calculated using a timer interrupt every 0.05 seconds (0.05 sec to make sure that the sensors' readings are faster than the PID calculation)
//*the dt is embedded in the integral and differential terms
//*the motors' velocities are checked to make sure that the omega of each motor doesn't exceed the maximum omega (with keeping the difference between both omegas to keep direction), and that no omega is less than zero
//*MPU is calibrated by making sure that for twenty consecutive MPU readings(with delay of 0.1 sec between them), each two consecutive readings don't have more than half a degree difference

#include <Wire.h>
#include <I2Cdev.h>
//#include "RedBot.h"
#include <math.h>
//#include <NewPing.h>
#include <MPU_init.h>
//**//needs a change here
//put the libraries used by the GPS and the compass
//**//


#define right_motor 9 ////(EN1)3 on Uno  //avoid pins 11 and 12 on Mega as they utilize timer1
#define left_motor 10 ////(EN2)11 on Uno  //avoid pins 9 and 10 on UNO as they utilize timer1
#define A11 4 
#define A22 5 
#define A33 6 
#define A44 7 
#define motors_init 
#define Kp 0.5//1.7  //**/ should be modified when changing the robot (according to the tuning)
//float Kp = 1.1;
#define Ki 0//0.04 //**/ should be modified when changing the robot (according to the tuning)
#define Kd 0.04//6 //**/ should be modified when changing the robot (according to the tuning)
#define Pi 3.14
#define Width 0.1 //**/ should be modified when changing the robot (according to the dimensions)
#define Radius 0.032 //**/ should be modified when changing the robot (according to the dimensions)
#define max_rpm 100 //**/ should be modified when changing the robot (according to the motors)
//#define right_encoder A1 
//#define left_encoder A0 
//#define numTicks 16 

#define TRIGGER_PIN  A0  // Arduino pin tied to trigger pin on the ultrasonic sensor.
#define ECHO_PIN     A1  // Arduino pin tied to echo pin on the ultrasonic sensor.
#define MAX_DISTANCE 200 // Maximum distance we want to ping for (in centimeters). Maximum sensor distance is rated at 400-500cm.

//**//needs a change here
//the absolute values of the final goal's GPS
#define x_dest 10 ////the x-coordinate of the final destination in world frame
#define y_dest 0 ////the y-coordinate of the final destination in world frame
//**//

float max_rad = max_rpm*2.0*Pi/60.0; //maximum radian/second, as all the equations below depend on rad/sec and not rpm
float theta = 0; //the angle from the robot axis to the final destination
float thetaOld = 0;
float thetaint = 0;
float gyroAngles[] = {0, 0, 0}; //yaw, pitch, roll
float omega = 0;//the angular velocity of the robot itself
float thetadot, vel, omegaLeft, omegaRight;//omegaleft and omegaright are the angular velocities of the left and right wheel
//long TickOldR = 0;
//long TickR = 0;
//long TickOldL = 0;
//long TickL = 0;
float max_thetaint = (max_rad * Radius / Width) / Ki; //from the equation of omega right and left, divided by two to make sure that th integral term doesn't equal the max rpm
//RedBotEncoder Encoders(left_encoder, right_encoder);
float x = 0;//the x-coordinate of the robot in the world frame
float y = 0;//the y-coordinate of the robot in the world frame
int car_mode = 0;
//int counter = 0;
int num=0;
int distance=700;
float phi=0.169;
MPU_init accelgyro; 
//NewPing sonar(TRIGGER_PIN, ECHO_PIN, MAX_DISTANCE); // NewPing setup of pins and maximum distance.
//**//needs a change here
//initiatiate the compass and gps objects
//**//


void getThetaError(); //the angle from the robot axis to the final destination, in radians
float getYaw();//the yaw angle from the gyroscope or compass angle between robot and north
void getOmega();//apply PID to get the omega of the body needed, the function is called by timer interrupt every 0.05 sec to make dt fixed (dt is embedded in the Ki and Kd gains)
void getPosition(); //get the x and y of the body in the world frame
void timer1_init();//initialize timer1 which is used by getOmega function (PID)
void omegaToPwm(void);//the omega of each wheel is then converted to PWM of each motor
void motorSaturation(void);//make sure that the omega of each motor doesn't exceed the maximum omega (with keeping the difference between both omegas to keep direction), and that no omega is less than zero
ISR(TIMER1_COMPA_vect);
void NO_loops();

void dmpDataReady();//check for new MPU readings
//**//put any additional functions headers here needed for gps and compass
  
void setup(){
  //motors initialization
  pinMode(right_motor, OUTPUT);
  pinMode(left_motor, OUTPUT);
  pinMode(A11, OUTPUT);
  pinMode(A22, OUTPUT);
  pinMode(A33, OUTPUT);
  pinMode(A44, OUTPUT);
  digitalWrite(A11, HIGH);//the wheels rotate clockwise only
  digitalWrite(A33, HIGH);
  
  Serial.begin(115200); /////
  
  attachInterrupt(0, dmpDataReady, RISING);
  accelgyro.init();//initialize MPU
  //**//needs a change here
  //put gps and compass initialization, and attach them to interrupts if needed
  //**//
  
  timer1_init();
  
  accelgyro.calibrate();//MPU is calibrated by making sure that for twenty consecutive MPU readings(with delay of 0.1 sec between them), each two consecutive readings don't have more than half a degree difference
  //**//needs a change here
  //put gps and compass calibration if there is any
  //**//
}

void loop(){

   getThetaError(); //theta in radian ////
   NO_loops();
  if (car_mode == 1) vel = 0;
  else vel = 0.15; 
 
  
  //if (abs(x_dest - x) <= 0.2 && abs(y_dest - y) <= 0.2) vel = 0;//stop if the distance from the goal is 20 centimeters(to compensate for inertia)
  //put a suffiecient velocity here (put the exact velocity needed if there are encoders to estimate it)
  omegaRight = (2. * vel + omega * Width) / (2. * Radius);//in rad/sec
  omegaLeft = (2. * vel - omega * Width) / (2. * Radius);
  motorSaturation();
  omegaToPwm();
 
  getPosition();
   
  /*PC
  Serial.println(car_mode);
  Serial.print(omegaRight);
  Serial.print("\t");
  Serial.print(omegaLeft);
  Serial.print("\n");
  //*/
  num=num+1;
  Serial.println(num);
}
void NO_loops() { //get the ultrasonic readings

//      number_of_loops =(distance+73.6)/0.2108

  if(num > ((105+73.6)/0.2108)){
    car_mode = 1;
    
  }
}
float getYaw(){ //the angle from the compass between the robot direction and the north
  if (accelgyro.mpuReadingReady()){
    accelgyro.getAngles(&gyroAngles[0]);
  }
  return -gyroAngles[0]; //the clockwise rotation is negative
  
  //write here the code of the compass here, and make it return the value of the angle between the robot and the north
  //return **;
}
//**//

//**// needed for the mpu only
void dmpDataReady() {//check for new MPU readings
	accelgyro.mpuInterrupt = true;
}
//**//
//**//needs a change here
void getThetaError(){ //the angle from the robot axis to the final destination, in radians, it should be the angle between the goal and the north, plus the angle between the robot direction and the north
//take care of the signs and wether clockwise is positive or negative
//float phi;


 // phi = atan2(y_dest - y, x_dest - x);
 
 /* if (car_mode == 2)
{
  phi += 0.52; //Kp = 1.45;
  car_mode=0;
}
  else{
    phi = atan2(y_dest - y, x_dest - x); //phi is the angle between the goal and the north, get it by getting the coordinates of any point in the north axis, and arctan(atan2) between this point and the coordinates of the goal (replace ** by the coordiantes of the point)
  }*/
  
  if (phi < 0) phi += 6.28; //if the phi is negative, add 360 degree (6.28 rad)
 theta = phi + getYaw(); //theta is the angle between the robot axis and the goal
 
  if (theta > 3.14) theta -= 6.28; //if theta is more than 180 degree, substract 360 degrees (to mak e the robot rotate in the other direction)
  if (abs(theta) < 0.035) theta = 0; //if theta is less than 2 degrees error, ignore the error

  ///*PC
 Serial.print(theta);Serial.print("\n");
  //*/
}
void timer1_init(){//initialize timer1 which is used by getOmega function (PID)
  //set timer1 interrupt at 20Hz
  TCCR1A = 0;// set entire TCCR1A register to 0
  TCCR1B = 0;// same for TCCR1B
  TCNT1  = 0;//initialize counter value to 0
  // set compare match register for 1hz increments
  OCR1A = 12499;// = (16*10^6) / ((1/0.05)*64) - 1 (must be <65536)
  // turn on CTC mode
  TCCR1B |= (1 << WGM12);
  // Set CS10 and CS12 bits for 64 prescaler
  TCCR1B |= (1 << CS11) | (1 << CS10);  
  // enable timer compare interrupt
  TIMSK1 |= (1 << OCIE1A);
  sei();
}

void getOmega(){ //apply PID to get the omega of the body needed, the function is called by timer interrupt every 0.05 sec to make dt fixed (dt is embedded in the Ki and Kd gains)
  thetadot = theta - thetaOld;
  thetaint += theta;
  
  //to make sure the integral term don't accumulate too much (more than the maximum omega possible) to prevent overshooting
  if (thetaint > max_thetaint) thetaint = max_thetaint;
  else if (thetaint < -max_thetaint) thetaint = -max_thetaint;
  
  omega = Kp * theta + Kd * thetadot + Ki * thetaint;
  thetaOld = theta;
}

ISR(TIMER1_COMPA_vect){//timer for PID to fix the time interval dt (dt is long enough so that the function is not faster than the update of theta)
  getOmega();
}

//**//needs a change here
void getPosition(void){ //get the x and y of the body in the world frame
  //put the gps code here
  //x the absolute x coordinate of the robot, y is the absolute y coordinate of the robot
  //x = 
  //y = 
}
//**//

void omegaToPwm(){//the omega of each wheel is then converted to PWM of each motor
  int PWMRight = omegaRight * 255.0 / max_rad;
  int PWMLeft = omegaLeft * 255.0 / max_rad;
  
  if (PWMRight < 50) PWMRight = 0; ////when PWM is less than nearly 20% 
  if (PWMLeft < 50) PWMLeft = 0; ////the current motors don't work at all (should be changed with other motors)
  
 // /*PC
  Serial.print(PWMRight);
  Serial.print("\t");
  Serial.print(PWMLeft);
  Serial.print("\n");
  //*/
  
  analogWrite(right_motor, PWMRight);
  analogWrite(left_motor,  PWMLeft);
}

void motorSaturation(void){ //make sure that the omega of each motor doesn't exceed the maximum omega (with keeping the difference between both omegas to keep direction), and that no omega is less than zero
  if (omegaRight >= omegaLeft && omegaRight > max_rad){
    float difference = omegaRight - max_rad;
    omegaRight = max_rad;
    omegaLeft -= difference;
  }
  else if (omegaLeft > omegaRight && omegaLeft > max_rad){
    float difference = omegaLeft - max_rad;
    omegaLeft = max_rad;
    omegaRight -= difference;
  }
  
  if ((omegaRight < 0) && (omegaLeft < 0)){ //if the car wants to go backward, turn it
    omegaRight =0;
    omegaLeft = max_rad;
  }
  else if (omegaRight < 0) omegaRight = 0; //if only one is negative
  else if (omegaLeft < 0) omegaLeft = 0;
  
  //PC
  //Serial.print(omegaRight);Serial.print("\t");Serial.print(omegaLeft);Serial.print("\n");
  ////
}







