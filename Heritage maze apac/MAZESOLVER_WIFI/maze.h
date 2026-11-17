// include all maze related variables here.

#define MAZE_SIZE_X 16
#define MAZE_SIZE_Y 16

int startX = 0;
int startY = 0;

int finishX = 7;
int finishY = 7;

enum wallState
{
  DONTKNOW,
  OPEN,
  CLOSE,
  VIRTUAL
};

struct node
{
  wallState N = DONTKNOW, E = DONTKNOW, S = DONTKNOW, W = DONTKNOW;
  bool visited = false;
};

/* for flood fill algorithm*/
node MAZE_FLOOD[MAZE_SIZE_X][MAZE_SIZE_Y];

/* for DFS algorithm*/
node MAZE_DFS[200];


void resetMaze()
{
  for (int i = 0; i < MAZE_SIZE_X; i++)
  {
    for (int j = 0; j < MAZE_SIZE_Y; j++)
    {
      MAZE_FLOOD[i][j].N = DONTKNOW;
      MAZE_FLOOD[i][j].E = DONTKNOW;
      MAZE_FLOOD[i][j].W = DONTKNOW;
      MAZE_FLOOD[i][j].S = DONTKNOW;
      MAZE_FLOOD[i][j].visited = false;
    }
  }
}

