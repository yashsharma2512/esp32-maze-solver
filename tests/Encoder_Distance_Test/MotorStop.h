#pragma once

bool motorStopRequested();
bool enableMotorDrive();
void requestMotorStop();
void beginMotorStop();
int readUsbCommand();
bool requestMotorResume();
bool takeMotorResumeRequest();
bool prepareRobotReset();
void restartRobot();
