#include <stdio.h>
#include <SDL.h>
#include <sys/time.h>
#include <math.h>

#define BPP                 4
#define DEPTH               32
#define FPS                 100

#define GRID_BLOCK_HEIGHT   14
#define GRID_BLOCK_WIDTH    6
#define BLOCK_HEIGHT        50
#define BLOCK_WIDTH         50
#define BLOCK_COLUMN_LENGTH 3
#define BLOCKS_TO_MATCH     3

// Constants
const int GRID_HEIGHT = GRID_BLOCK_HEIGHT * BLOCK_HEIGHT;
const int GRID_WIDTH  = GRID_BLOCK_WIDTH * BLOCK_WIDTH;

// Window Title
const char* title = "Blocks!";

///////////////////////////////////////////////////////////////////////////////
// Linked List for tracking blocks to remove when clearing
///////////////////////////////////////////////////////////////////////////////

struct rem_list_el {
  int x;
  int y;
  struct rem_list_el* next;
};

typedef struct rem_list_el BlockRemItem;

void addItemToRemList(BlockRemItem* head, BlockRemItem* next) {
  // Iterate over head to make sure we hit the end of the list before adding
  BlockRemItem* curr = head;
  while(1) {
    if(curr->next == NULL) {
      curr->next = next;
      break;
    }
    curr = curr->next;
    //printf("Looking for the end!\n");
  }    
}

int getItemToRemListLength(BlockRemItem* head) {
  int len = 0;
  BlockRemItem* curr = head;
  while(curr) {
    len++;
    curr = curr->next;
  }    

  return len;
}

///////////////////////////////////////////////////////////////////////////////
// Enums and Structs
///////////////////////////////////////////////////////////////////////////////

typedef enum { false, true } bool;

typedef struct {
  int occupied;
  int x;
  int y;
  int *color;
} Block;

///////////////////////////////////////////////////////////////////////////////
// Function declarations
///////////////////////////////////////////////////////////////////////////////

void   zeroBlock(Block*);
void   initGameState();
void   spawnColumn();
void   drawRect(SDL_Surface*, int, int, int, int, int*);
void   renderBlock(SDL_Surface*, Block);
void   renderColumn(SDL_Surface*);
void   renderPlacedBlocks(SDL_Surface*);
void   DrawScreen(SDL_Surface*);
double hires_time_in_seconds();
float  min(double, double);
void   moveColumnDown(double);
int*   getRandomColor();
int    getRandomX();
void   moveColumnRight();
void   moveColumnLeft();
void   compactBlocks(double);
void   shiftColumnColors();
void   clearAndScore();

///////////////////////////////////////////////////////////////////////////////
// Global Variables
///////////////////////////////////////////////////////////////////////////////

// Colors
int COLOR_BLACK[3]  = {0x00, 0x00, 0x00};
int COLOR_RED[3]    = {0xFF, 0x00, 0x00};
int COLOR_GREEN[3]  = {0x00, 0xFF, 0x00};
int COLOR_BLUE[3]   = {0x00, 0x00, 0xFF};
int COLOR_ORANGE[3] = {0xFF, 0xA5, 0x00};
int COLOR_YELLOW[3] = {0xFF, 0xFF, 0x00};
int COLOR_PURPLE[3] = {0xFF, 0x00, 0xFF};

// Block compacting interval
double blockCompactingInterval = .012;

// Column dowards movement interval
double columnDownInterval = .1;

///////////////////////////////////////////////////////////////////////////////
// Game State
///////////////////////////////////////////////////////////////////////////////

// An array of blocks representing the column
Block  columnBlocks[BLOCK_COLUMN_LENGTH];
double lastColumnDownMove = 0.0;

// A matrix of blocks representing the blocks that have been placed
Block placedBlocks[GRID_BLOCK_WIDTH][GRID_BLOCK_HEIGHT];
double lastCompactBlocksMove = 0.0;

// An array of 
int occupiedSlots[GRID_BLOCK_WIDTH];

// Are we still playing the game?
int gameOver = 0;

///////////////////////////////////////////////////////////////////////////////
// Main Game Loop
///////////////////////////////////////////////////////////////////////////////

int main(int argc, char* argv[]) {

  initGameState();

  SDL_Surface *screen;
  SDL_Event   event;

  int keypress = 0;
  int h        = 0; 

  if (SDL_Init(SDL_INIT_VIDEO) < 0 ) return 1;

  if (!(screen = SDL_SetVideoMode(GRID_WIDTH, GRID_HEIGHT, DEPTH, 
                                  SDL_RESIZABLE|SDL_HWSURFACE))) {
    SDL_Quit();
    return 1;
  }

  SDL_WM_SetCaption(title, title);

  // Initial column spawn
  spawnColumn();
  
  double currentTime = hires_time_in_seconds();
  double FPS_dt      = (double)1/FPS;

  int gameOn = 1;

  while(gameOn) {

    // User Input
    while(SDL_PollEvent(&event)) {
      if(event.type == SDL_QUIT) {
        gameOn = 0;
        break;
      } else if(event.type == SDL_KEYDOWN) {
        switch(event.key.keysym.sym) {  
          case SDLK_LEFT:
            moveColumnLeft();
            break;
          case SDLK_RIGHT:
            moveColumnRight();
            break;
          case SDLK_UP:
            break;
          case SDLK_DOWN:
            break;
          case SDLK_SPACE:
            shiftColumnColors();
            break;
          default:
            break;
        }
      }
    }    

    // Calculations
    moveColumnDown(currentTime);
    compactBlocks(currentTime);

    // Keep a constant framerate
    double newTime = hires_time_in_seconds();
    while(newTime - currentTime < FPS_dt) {
      SDL_Delay(20);
      newTime = hires_time_in_seconds();          
    } 

    currentTime = newTime;

    // Render the screen
    DrawScreen(screen);
  }

  SDL_Quit();
  
  return 0;
}

///////////////////////////////////////////////////////////////////////////////
// Functions
///////////////////////////////////////////////////////////////////////////////

void zeroBlock(Block *block) {
  block->occupied = 0;
  block->x        = 0;
  block->y        = 0;
  block->color    = 0;
}

void initGameState() {
  // Zero out the placeBlocks matrix
  int x,y;
  for(x=0; x < GRID_BLOCK_WIDTH; x++) {
    for(y=0; y < GRID_BLOCK_HEIGHT; y++) {
      zeroBlock(&placedBlocks[x][y]);
    }
  }

  // Zero out the columnBlocks Array
  for(x=0; x < BLOCK_COLUMN_LENGTH; x++) {
    zeroBlock(&columnBlocks[x]);
  }

  // Init the occupied slots array
  // Note: we are setting the occupied slot for each X value to the max 
  //       Y value i.e. the size of the grid
  for(x=0; x < GRID_BLOCK_WIDTH; x++) {
    occupiedSlots[x] = GRID_BLOCK_HEIGHT;
  }
}

void spawnColumn() {
  int blockX = getRandomX() * BLOCK_WIDTH;

  int c;
  for(c=0; c < BLOCK_COLUMN_LENGTH; c++) {
    columnBlocks[c].occupied = true;
    columnBlocks[c].x        = blockX;
    columnBlocks[c].y        = (c-2)*BLOCK_HEIGHT;
    columnBlocks[c].color    = getRandomColor();
  }
}

int* getRandomColor() {
  int r = rand()%5;

  switch(r) {
    case 0:  return COLOR_RED;
    case 1:  return COLOR_GREEN;
    case 2:  return COLOR_BLUE;
    case 3:  return COLOR_ORANGE;
    case 4:  return COLOR_YELLOW;
    case 5:  return COLOR_PURPLE;
    default: return COLOR_RED;
  }
}

int getRandomX() {
  return rand()%(GRID_BLOCK_WIDTH-1);
}

void drawRect(SDL_Surface *screen, int x, int y, int w, int h, int *rgb) {
  SDL_Rect rect    = {x,y,w,h};
  Uint32   myColor = SDL_MapRGB(screen->format, rgb[0], rgb[1], rgb[2]);
  SDL_FillRect(screen, &rect, myColor);
}

void renderBlock(SDL_Surface *screen, Block block) {
  drawRect(screen, block.x, block.y, BLOCK_WIDTH, BLOCK_HEIGHT, block.color);
}

void renderColumn(SDL_Surface *screen) {
  int i=0;
  for(i=0; i < BLOCK_COLUMN_LENGTH; i++) {
    Block tBlock = columnBlocks[i];
    renderBlock(screen, tBlock);
  }
}

void renderPlacedBlocks(SDL_Surface *screen) {
  int x, y;
  for(x=0; x < GRID_BLOCK_WIDTH; x++) {
    for(y=0; y < GRID_BLOCK_HEIGHT; y++) {
      Block tBlock = placedBlocks[x][y];
      if(tBlock.occupied) {
        renderBlock(screen, tBlock);
      }
    }
  }
}

void DrawScreen(SDL_Surface* screen) { 
  if(SDL_MUSTLOCK(screen)) {
    if(SDL_LockSurface(screen) < 0) return;
  }

  // Render Background
  drawRect(screen, 0, 0, GRID_WIDTH, GRID_HEIGHT, COLOR_BLACK);

  // Render Column
  renderColumn(screen);

  // Render Placed Blocks
  renderPlacedBlocks(screen);

  if(SDL_MUSTLOCK(screen)) SDL_UnlockSurface(screen);

  SDL_Flip(screen); 
}

/** Returnt the current time in seconds */
double hires_time_in_seconds() {
  struct timeval tv;
  gettimeofday(&tv, NULL);
  return (double)((tv.tv_sec * 1000000LL) + tv.tv_usec) / 1000000LL;
}

float min(double a, double b) {
  return (float)(a < b ? a : b);
}

/** Slide blocks with un-occupied slots underneath them down. This should
compact the grid of blocks. Note: We need to clear and score after each
cycle. */
void compactBlocks(double currentTime) {
  if(gameOver == 1) return;

  if(lastCompactBlocksMove <= 0) {
    lastCompactBlocksMove = currentTime;
  }

  if(currentTime - lastCompactBlocksMove < blockCompactingInterval) {
    return;
  }

  lastCompactBlocksMove = currentTime;

  int amnt       = ceil(BLOCK_HEIGHT/2);
  int lowerBound = GRID_HEIGHT - BLOCK_HEIGHT;

  int x, y;
  for(x=0; x < GRID_BLOCK_WIDTH; x++) {
    for(y=0; y < GRID_BLOCK_HEIGHT; y++) {
      
      // Only move occupied blocks
      if(!placedBlocks[x][y].occupied) continue;

      // If we aren't on the bottom of the grid look one block ahead
      if(y+1 < GRID_BLOCK_HEIGHT) {

        // If the next block is not occupied then move into it
        if(!placedBlocks[x][y+1].occupied) {

          int lowerBound = GRID_HEIGHT - BLOCK_HEIGHT;
          int maxY       = placedBlocks[x][y].y + amnt;
          int nextGridY  = ceil((double)maxY/(double)BLOCK_HEIGHT)-1;
          int gridX      = ceil(placedBlocks[x][y].x/BLOCK_WIDTH);          

          // Move the block downward until either it hits the bottom of
          // the grid or we move into the next slot
          if(nextGridY >= (y+1)) {

            int oldGridY = placedBlocks[x][y].y / BLOCK_HEIGHT;

            // Align the block's position with the grid
            placedBlocks[x][y].y = round((placedBlocks[x][y].y/BLOCK_HEIGHT)*BLOCK_HEIGHT);

            // Calculate the y place in the grid this block
            int gridY = placedBlocks[x][y].y / BLOCK_HEIGHT;            

            placedBlocks[gridX][gridY].x        = placedBlocks[x][y].x;
            placedBlocks[gridX][gridY].y        = placedBlocks[x][y].y;
            placedBlocks[gridX][gridY].color    = placedBlocks[x][y].color;
            placedBlocks[gridX][gridY].occupied = true;            

            placedBlocks[x][y].x        = x * BLOCK_WIDTH;
            placedBlocks[x][y].y        = y * BLOCK_HEIGHT;
            placedBlocks[x][y].color    = COLOR_BLACK;
            placedBlocks[x][y].occupied = false;
          } else {
            placedBlocks[x][y].y += amnt;
          }
        }
      }
    }
  }

  // Reset the occupied slot values
  for(x=0; x < GRID_BLOCK_WIDTH; x++) {
    for(y=0; y < GRID_BLOCK_HEIGHT; y++) {
      if(placedBlocks[x][y].occupied) {
        occupiedSlots[x] = y-1;
        break;
      }      
    }
  }
}

/** Move the column down the grid. Take the current game time to
make sure we don't move the column down too often. Note: the
column down interval determines how often we move the column down. */
void moveColumnDown(double currentTime) {
  if(gameOver == 1) return;

  if(lastColumnDownMove <= 0) {
    lastColumnDownMove = currentTime;
  }

  if(currentTime - lastColumnDownMove < columnDownInterval) {
    return;
  }

  lastColumnDownMove = currentTime;

  int amnt       = ceil(BLOCK_HEIGHT/2);
  int lowerBound = GRID_HEIGHT - BLOCK_HEIGHT;
  int maxY       = columnBlocks[BLOCK_COLUMN_LENGTH-1].y + amnt;
  int nextGridY  = ceil((double)maxY/(double)BLOCK_HEIGHT)-1;
  int gridX      = ceil(columnBlocks[BLOCK_COLUMN_LENGTH-1].x/BLOCK_WIDTH);
  int maxGridY   = occupiedSlots[gridX];
  
  // If no blocks have dropped in this gridX then use the bottom of the grid
  // as the maxGridY value
  if(maxGridY >= GRID_BLOCK_HEIGHT) {
    maxGridY = GRID_BLOCK_HEIGHT-1;
  }

  // Check lower bound i.e. the bottom of the grid
  // and the gridX's for blocks
  if(maxY > lowerBound || nextGridY >= maxGridY) {
    // We've hit the bottom of the board
    // Shift the blockColumn into the placedBlocks array
    int c;
    for(c=0; c < BLOCK_COLUMN_LENGTH; c++) {

      int oldGridY = columnBlocks[c].y / BLOCK_HEIGHT;

      // Align the block's position with the grid
      columnBlocks[c].y = round((columnBlocks[c].y/BLOCK_HEIGHT)*BLOCK_HEIGHT);

      // Calculate the y place in the grid this block
      int gridY = columnBlocks[c].y / BLOCK_HEIGHT;

      // Move the block to the placed block array for rendering
      placedBlocks[gridX][gridY].x        = columnBlocks[c].x;
      placedBlocks[gridX][gridY].y        = columnBlocks[c].y;
      placedBlocks[gridX][gridY].color    = columnBlocks[c].color;
      placedBlocks[gridX][gridY].occupied = true;
      
      // Add an entry to the occupiedSlots map for easy collision lookup
      occupiedSlots[gridX] = nextGridY - BLOCK_COLUMN_LENGTH;
    }

    // Clear and score
    clearAndScore();
    
    // Make sure we haven't hit the top of the grid i.e. "GAME OVER"
    if(nextGridY - BLOCK_COLUMN_LENGTH <= 0) {
      gameOver = true;
      return;
    }

    spawnColumn();

  // Move the column down
  } else {
    int c;
    for(c=0; c < BLOCK_COLUMN_LENGTH; c++) {
      columnBlocks[c].y += amnt;
    }
  }
}

/** Move the whole column to the right 1 grid square i.e. the width of the blocks */
void moveColumnRight() {
  if(gameOver) return;

  int c;
  for(c=0; c < BLOCK_COLUMN_LENGTH; c++) {
    int ox       = columnBlocks[c].x;
    int nx       = columnBlocks[c].x + BLOCK_WIDTH;
    int nGridX   = (nx/BLOCK_WIDTH);
    int gridY    = ceil((columnBlocks[BLOCK_COLUMN_LENGTH-1].y)/BLOCK_HEIGHT)-1;
    int maxGridY = occupiedSlots[nGridX];

    if(nx > GRID_WIDTH-BLOCK_WIDTH) {
      nx = GRID_WIDTH-BLOCK_WIDTH;
    } else if(gridY >= maxGridY) {
      continue;
    }

    columnBlocks[c].x = nx;
  }
}

/** Move the whole column to the left 1 grid square i.e. the width of the blocks */
void moveColumnLeft() {
  if(gameOver) return;
  int gridX = ceil((columnBlocks[BLOCK_COLUMN_LENGTH-1].x)/BLOCK_WIDTH);

  int c;
  for(c=0; c < BLOCK_COLUMN_LENGTH; c++) {
    int nx       = columnBlocks[c].x - BLOCK_WIDTH;
    int nGridX   = (nx/BLOCK_WIDTH);
    int gridY    = ceil((columnBlocks[BLOCK_COLUMN_LENGTH-1].y)/BLOCK_HEIGHT)-1;
    int maxGridY = occupiedSlots[nGridX];    

    if(nx < 0) {
      nx = 0;
    } else if(gridY >= maxGridY) {
      continue;
    }

    columnBlocks[c].x = nx;
  }
}

/** Shift the column colors down. */
void shiftColumnColors() {
  if(gameOver) return;
  int*  color0 = columnBlocks[0].color;
  int*  color1 = columnBlocks[1].color;
  int*  color2 = columnBlocks[2].color;
  columnBlocks[0].color = color2;
  columnBlocks[1].color = color0;
  columnBlocks[2].color = color1;
}

void clearAndScore() {
  // When a block moves from the blockColumn to the placedBlocks array 
  // then we need to check each of it's blocks for >= <BLOCKS_TO_MATCH> block matches
  int foundBlocksToRemove = false;

  // Loop through the blocks that have just landed
  int bc;
  int x,y;
  //for(bc=0; bc < BLOCK_COLUMN_LENGTH; bc++) {

  for(x=0; x < GRID_BLOCK_WIDTH; x++) {
    for(y=0; y < GRID_BLOCK_HEIGHT; y++) {

      Block block = placedBlocks[x][y]; //columnBlocks[bc];

      if(!block.occupied) continue;
    
      int gridX = block.x / BLOCK_WIDTH;
      int gridY = block.y / BLOCK_HEIGHT;

      // Initialize linked lists for tracking matching blocks
      BlockRemItem hBlocksToRemove;
      hBlocksToRemove.x    = -1;
      hBlocksToRemove.y    = -1;
      hBlocksToRemove.next = NULL;    

      BlockRemItem vBlocksToRemove;
      vBlocksToRemove.x    = -1;
      vBlocksToRemove.y    = -1;
      vBlocksToRemove.next = NULL;        

      BlockRemItem fdBlocksToRemove;
      fdBlocksToRemove.x    = -1;
      fdBlocksToRemove.y    = -1;
      fdBlocksToRemove.next = NULL;        

      BlockRemItem bdBlocksToRemove;
      bdBlocksToRemove.x    = -1;
      bdBlocksToRemove.y    = -1;
      bdBlocksToRemove.next = NULL;     

      // Temp pointer to be used for allocating new temp blocks to remove below
      BlockRemItem* tItem;

      // Horizontal
      int hrGridX = gridX;
      int hlGridX = gridX;

      // Vertical
      int vuGridY = gridY;
      int vdGridY = gridY;

      // Forward Diagonal
      int fdGridX = gridX;
      int fdGridY = gridY;
      int fuGridX = gridX;
      int fuGridY = gridY;
  
      // Backward Diagonal  
      int bdGridX = gridX;
      int bdGridY = gridY;
      int buGridX = gridX;
      int buGridY = gridY;

      int blValid = true;
      int urValid = true;
      int lrValid = true;
      int ulValid = true;

      int hrValid = true;
      int hlValid = true;
      int vuValid = true;
      int vdValid = true;
    
      while(blValid || urValid || lrValid || ulValid || hrValid || hlValid || vuValid || vdValid) {

        // Moving to the bottom left
        fdGridX--;
        fdGridY++;
        blValid = blValid && fdGridX >= 0 && fdGridY < GRID_BLOCK_HEIGHT;
    
        if(blValid) {
          Block tBlock = placedBlocks[fdGridX][fdGridY];
          if(tBlock.occupied && tBlock.color == block.color) {
            tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
            tItem->x = tBlock.x;
            tItem->y = tBlock.y;
            tItem->next = NULL;          
          
            addItemToRemList(&fdBlocksToRemove, tItem);
          } else {
            blValid = false;
          }
        }
    
        // Moving to the upper right
        fuGridX++;
        fuGridY--;
        urValid = urValid && fuGridX < GRID_BLOCK_WIDTH && fuGridY >= 0;
    
        if(urValid) {
          Block tBlock = placedBlocks[fuGridX][fuGridY];
          if(tBlock.occupied && tBlock.color == block.color) {
            tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
            tItem->x = tBlock.x;
            tItem->y = tBlock.y;
            tItem->next = NULL;          
          
            addItemToRemList(&fdBlocksToRemove, tItem);   
          } else {
            urValid = false;
          }
        }    
    
        // Moving to the lower right
        bdGridX++;
        bdGridY++;
        lrValid = lrValid && bdGridX < GRID_BLOCK_WIDTH && bdGridY < GRID_BLOCK_HEIGHT;

        if(lrValid) {
          Block tBlock = placedBlocks[bdGridX][bdGridY];
          if(tBlock.occupied && tBlock.color == block.color) {
            tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
            tItem->x = tBlock.x;
            tItem->y = tBlock.y;
            tItem->next = NULL;          
          
            addItemToRemList(&bdBlocksToRemove, tItem);
          } else {
            lrValid = false;
          }
        }    
    
        // Moving to the upper left
        buGridX--;
        buGridY--;
        ulValid = ulValid && buGridX >= 0 && buGridY >= 0;
    
        if(ulValid) {
          Block tBlock = placedBlocks[buGridX][buGridY];
          if(tBlock.occupied && tBlock.color == block.color) {
            tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
            tItem->x = tBlock.x;
            tItem->y = tBlock.y;
            tItem->next = NULL;          
          
            addItemToRemList(&bdBlocksToRemove, tItem);
          } else {
            ulValid = false;
          }
        }

        // Move to the horizonal right
        hrGridX++;
        hrValid = hrValid && hrGridX < GRID_BLOCK_WIDTH;
    
        if(hrValid) {
          Block tBlock = placedBlocks[hrGridX][gridY];
          if(tBlock.occupied && tBlock.color == block.color) {
            tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
            tItem->x = tBlock.x;
            tItem->y = tBlock.y;
            tItem->next = NULL;              

            addItemToRemList(&hBlocksToRemove, tItem);
          } else {
            hrValid = false;
          }
        }
    
        // Move to the horizonal left
        hlGridX --;
        hlValid = hlValid && hlGridX >= 0;
    
        if(hlValid) {
          Block tBlock = placedBlocks[hlGridX][gridY];
          if(tBlock.occupied && tBlock.color == block.color) {
            tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
            tItem->x = tBlock.x;
            tItem->y = tBlock.y;
            tItem->next = NULL;

            addItemToRemList(&hBlocksToRemove, tItem);
          } else {
            hlValid = false;
          }
        } 

      
        // Move to the top
        vuGridY--;
        vuValid = vuValid && vuGridY >= 0;
        
        if(vuValid) {
          Block tBlock = placedBlocks[gridX][vuGridY];
          if(tBlock.occupied && tBlock.color == block.color) {
            tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
            tItem->x = tBlock.x;
            tItem->y = tBlock.y;
            tItem->next = NULL;          

            addItemToRemList(&vBlocksToRemove, tItem);
          } else {
            vuValid = false;
          }
        }

        // Move to the bottom
        vdGridY++;
        vdValid = vdValid && vdGridY < GRID_HEIGHT;
        
        if(vdValid) {
          Block tBlock = placedBlocks[gridX][vdGridY];
          if(tBlock.occupied && tBlock.color == block.color) {
            tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
            tItem->x = tBlock.x;
            tItem->y = tBlock.y;
            tItem->next = NULL;          
          
            addItemToRemList(&vBlocksToRemove, tItem);
          } else {
            vdValid = false;
          }
        }        
      }
  
      if(getItemToRemListLength(&hBlocksToRemove) >= BLOCKS_TO_MATCH) {
        foundBlocksToRemove = true;

        tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
        tItem->x = block.x;
        tItem->y = block.y;
        tItem->next = NULL;
        addItemToRemList(&hBlocksToRemove, tItem);      

        BlockRemItem* curr = &hBlocksToRemove;
        while(curr) {
          BlockRemItem temp = *curr;
          if(curr->x >= 0 && curr->y >= 0) {
            placedBlocks[curr->x/BLOCK_WIDTH][curr->y/BLOCK_HEIGHT].occupied = false;
            placedBlocks[curr->x/BLOCK_WIDTH][curr->y/BLOCK_HEIGHT].color    = COLOR_BLACK;
          }

          curr = curr->next;
        }
      }

      if(getItemToRemListLength(&vBlocksToRemove) >= BLOCKS_TO_MATCH) {
        foundBlocksToRemove = true;

        tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
        tItem->x = block.x;
        tItem->y = block.y;
        tItem->next = NULL;
        addItemToRemList(&vBlocksToRemove, tItem);      

        BlockRemItem* curr = &vBlocksToRemove;
        while(curr) {
          if(curr->x >= 0 && curr->y >= 0) {
            placedBlocks[curr->x/BLOCK_WIDTH][curr->y/BLOCK_HEIGHT].occupied = false;
            placedBlocks[curr->x/BLOCK_WIDTH][curr->y/BLOCK_HEIGHT].color    = COLOR_BLACK;
          }
          curr = curr->next;
        }
      }

      if(getItemToRemListLength(&fdBlocksToRemove) >= BLOCKS_TO_MATCH) {
        foundBlocksToRemove = true;

        tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
        tItem->x = block.x;
        tItem->y = block.y;
        tItem->next = NULL;
        addItemToRemList(&fdBlocksToRemove, tItem);      

        BlockRemItem* curr = &fdBlocksToRemove;
        while(curr) {
          BlockRemItem* temp = curr;
          if(curr->x >= 0 && curr->y >= 0) {
            placedBlocks[curr->x/BLOCK_WIDTH][curr->y/BLOCK_HEIGHT].occupied = false;
            placedBlocks[curr->x/BLOCK_WIDTH][curr->y/BLOCK_HEIGHT].color    = COLOR_BLACK;
          }
         curr = curr->next;
        }
      }    

      if(getItemToRemListLength(&bdBlocksToRemove) >= BLOCKS_TO_MATCH) {
        foundBlocksToRemove = true;

        tItem = (BlockRemItem *)malloc(sizeof(BlockRemItem));       
        tItem->x = block.x;
        tItem->y = block.y;
        tItem->next = NULL;
        addItemToRemList(&bdBlocksToRemove, tItem);          

        BlockRemItem* curr = &bdBlocksToRemove;
        while(curr) {
          if(curr->x >= 0 && curr->y >= 0) {
            placedBlocks[curr->x/BLOCK_WIDTH][curr->y/BLOCK_HEIGHT].occupied = false;
            placedBlocks[curr->x/BLOCK_WIDTH][curr->y/BLOCK_HEIGHT].color    = COLOR_BLACK;
          }
          curr = curr->next;
        }                        
      }    
    }
  }
}