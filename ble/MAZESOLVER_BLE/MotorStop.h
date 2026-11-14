#pragma once

bool motorStopRequested();
bool enableMotorDrive();
void requestMotorStop();
void beginMotorStop();
int readUsbCommand();
bool motorResumeRequested();
bool requestMotorResume();
bool takeMotorResumeRequest();
