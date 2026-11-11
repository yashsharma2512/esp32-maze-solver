void solveMaze() {
#if defined(LEFTWALLFOLLOW) && !defined(RIGHTWALLFOLLOW) && !defined(DFS) && !defined(FLOODFILL)
  _tick = micros();
  pinMode(WALL_SELECT_PIN, INPUT_PULLUP);
  if (!digitalRead(WALL_SELECT_PIN))
    while (true)
      wallFollow(RIGHT);
  else
    while (true)
      wallFollow(LEFT);
#elif !defined(LEFTWALLFOLLOW) && defined(RIGHTWALLFOLLOW) && !defined(DFS) && !defined(FLOODFILL)
  while (true)
    WallFollow(RIGHT);
#elif !defined(LEFTWALLFOLLOW) && !defined(RIGHTWALLFOLLOW) && defined(DFS) && !defined(FLOODFILL)
  dfs();
#elif !defined(LEFTWALLFOLLOW) && !defined(RIGHTWALLFOLLOW) && !defined(DFS) && defined(FLOODFILL)
  floodFill();
#else
#error "Please select a solving algorithm. Also, please make sure only one algorithm is enabled"
#endif
}

void update_coordinates() {
  switch (HEADING) {
    case 0:  // NORTH
      GLOBAL_Y++;
      SerialDebug.print("Y++\t");
      break;
    case 1:  // EAST
      GLOBAL_X++;
      SerialDebug.print("X++\t");
      break;
    case 2:  // SOUTH
      GLOBAL_Y--;
      SerialDebug.print("Y--\t");
      break;
    case 3:  // WEST
      GLOBAL_X--;
      SerialDebug.print("X--\t");
      break;
    default:
      break;
  }
}

void printCoordinates(bool lw, bool rw) {
  SerialDebug.print("<");
  SerialDebug.print(heading[HEADING]);
  SerialDebug.print(">\t(");
  SerialDebug.print(GLOBAL_X);
  SerialDebug.print(",");
  SerialDebug.print(GLOBAL_Y);
  SerialDebug.print(")\t");
  SerialDebug.print((lw) ? "L" : "-");
  SerialDebug.print(((fl + fr) / 2 > 50) ? "F" : "-");
  SerialDebug.println((rw) ? "R" : "-");
}

void wallFollow(int wallToFollow) {
  mpu.update();
  updateProximitySensors();
  if (handleFloorColor()) return;

  //AI stuff
  // if (AI_ENABLE) {
  //   readSerial();
  //   interpretSerial();
  // }
  victimDetection();

  //IR
  // interpretIR(false);

  if ((micros() - _tick) > 2000) {
    _tick = micros();
    //////////////////////////////////////////////////////////////////////////////////
    // Solving algorithm
    //////////////////////////////////////////////////////////////////////////////////
    bool leftWall = (dl_filtered > leftTurnSense);
    bool rightWall = (dr_filtered > rightTurnSense);
    bool frontWall = ((fl + fr) / 2 > (leftTurnSense + rightTurnSense) / 2);
    int8_t toTurn = 0;
    int8_t _turn = detectTurn(leftWall, rightWall);

    moveStraightAligned();

    interpretClicks();

    if (_turn != 0) {
      if (_turn == -1 and wallToFollow == LEFT)
      {
        SerialDebug.println("LEFT");
        if (!MoveStrightToDistance(OPENING_ADVANCE_MM)) return;
        delayWithSensorUpdate(100);
        turnBot(85, LEFT, false);
        delay(100);
        if (!MoveStrightToDistance(-TURN_BACKUP_MM)) return;
        delay(100);
      } else if (_turn == 1 and wallToFollow == RIGHT) {
        SerialDebug.println("RIGHT");
        if (!MoveStrightToDistance(OPENING_ADVANCE_MM)) return;
        delayWithSensorUpdate(100);
        turnBot(90, RIGHT, false);
        delay(100);
        if (!MoveStrightToDistance(-TURN_BACKUP_MM)) return;
        delay(100);
      }
      for (int i = 0; i < 10; i++) {
        delayWithSensorUpdate(20);
        leftWall = (dl_filtered > leftTurnSense);
        rightWall = (dr_filtered > rightTurnSense);
        _turn = detectTurn(leftWall, rightWall);
      }
    }
    if ((fl + fr) / 2 > FRONT_WALL_THRESHOLD) {
      moveMotor(0, 0);
      delayWithSensorUpdate(200);
      leftWall = (dl_filtered > leftTurnSense);
      rightWall = (dr_filtered > rightTurnSense);
      if (leftWall and rightWall) {
        turnBot(90, LEFT);
        delayWithSensorUpdate(100);
        turnBot(90, LEFT);
        delayWithSensorUpdate(100);
        moveMotor(-100, -100);
        delayWithSensorUpdate(1000);
        moveMotor(0, 0);
        mpu.resetAngleZ();
        delayWithSensorUpdate(100);
        mpu.resetAngleZ();
      } else if (!leftWall and rightWall) {
        turnBot(90, LEFT);
        delayWithSensorUpdate(100);
        MoveStrightToDistance(-TURN_BACKUP_MM);
        delay(100);
      } else if (leftWall and !rightWall) {
        turnBot(90, RIGHT);
        delayWithSensorUpdate(100);
        MoveStrightToDistance(-TURN_BACKUP_MM);
        delay(100);
      } else if (!leftWall and !rightWall) {
        turnBot(90, wallToFollow);
        delayWithSensorUpdate(100);
        MoveStrightToDistance(-TURN_BACKUP_MM);
        delay(100);
      }
    }
  }
}

void dfs()  // TODO
{
  while (1) {
  }
}

void floodFill()  // TODO
{
  while (1) {
  }
}
