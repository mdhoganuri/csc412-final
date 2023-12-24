//
//  main.c
//  Final Project CSC412
//
//  Created by Jean-Yves Hervé on 2020-12-01, rev. 2023-12-04
//
//	This is public domain code.  By all means appropriate it and change is to your
//	heart's content.

#include <iostream>
#include <string>
#include <random>
//
#include <cstdio>
#include <cstdlib>
#include <ctime>
//
#include "gl_frontEnd.h"

#include <unistd.h>
#include <thread>
#include <mutex>

void displayGridPaneFunc(void);
void displayStatePaneFunc(void);

//	feel free to "un-use" std if this is against your beliefs.
using namespace std;

struct TravelerThreadInfo {
	Traveler *traveler;
	int *sleepTime;
	unsigned int counter = 0; // counter for when segments will be added
};

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Private Functions' Prototypes
//-----------------------------------------------------------------------------
#endif

void initializeApplication(void);
	void moveThreaded (TravelerThreadInfo *info);
	vector<Direction> getLegalDirectionList (Traveler &traveler);
	Direction getNewDirection (Traveler &traveler, vector<Direction> legalDirectionList);
	bool isLegalDirection (Direction check, vector<Direction> legalDirectionList);
	void removeSegment(Traveler *traveler);
GridPosition getNewFreePosition(void);
Direction newDirection(Direction forbiddenDir = Direction::NUM_DIRECTIONS);
TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd);
void generateWalls(void);
void generatePartitions(void);

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Application-level Global Variables
//-----------------------------------------------------------------------------
#endif

//	Don't rename any of these variables
//-------------------------------------
//	The state grid and its dimensions (arguments to the program)
SquareType** grid;
unsigned int numRows = 0;	//	height of the grid
unsigned int numCols = 0;	//	width
unsigned int numTravelers = 0;	//	initial number
unsigned int numAddSegments = 0;
unsigned int numTravelersDone = 0;
unsigned int numLiveThreads = 0;		//	the number of live traveler threads
vector<Traveler> travelerList;
vector<SlidingPartition> partitionList;
GridPosition	exitPos;	//	location of the exit
vector<thread> threadList;
std::mutex travelerMutex;

//	travelers' sleep time between moves (in microseconds)
const int MIN_SLEEP_TIME = 10;
int travelerSleepTime = 1000;

//	An array of C-string where you can store things you want displayed
//	in the state pane to display (for debugging purposes?)
//	Dont change the dimensions as this may break the front end
const int MAX_NUM_MESSAGES = 8;
const int MAX_LENGTH_MESSAGE = 32;
char** message;
time_t launchTime;

//	Random generators:  For uniform distributions
const unsigned int MAX_NUM_INITIAL_SEGMENTS = 6;
random_device randDev;
default_random_engine engine(randDev());
uniform_int_distribution<unsigned int> unsignedNumberGenerator(0, numeric_limits<unsigned int>::max());
uniform_int_distribution<unsigned int> segmentNumberGenerator(0, MAX_NUM_INITIAL_SEGMENTS);
uniform_int_distribution<unsigned int> segmentDirectionGenerator(0, static_cast<unsigned int>(Direction::NUM_DIRECTIONS)-1);
uniform_int_distribution<unsigned int> headsOrTails(0, 1);
uniform_int_distribution<unsigned int> rowGenerator;
uniform_int_distribution<unsigned int> colGenerator;

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Functions called by the front end
//-----------------------------------------------------------------------------
#endif
//==================================================================================
//	These are the functions that tie the simulation with the rendering.
//	Some parts are "don't touch."  Other parts need your intervention
//	to make sure that access to critical section is properly synchronized
//==================================================================================

void drawTravelers (void) {
	//	You may have to sychronize things here
	for (unsigned int k=0; k<travelerList.size(); k++) {
		drawTraveler(travelerList[k]);
	}
}

void updateMessages (void) {
	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live robot threads will
	//	always get displayed.  No need to pass a message about it.
	unsigned int numMessages = 4;
	sprintf(message[0], "We created %d travelers", numTravelers);
	sprintf(message[1], "%d travelers solved the maze", numTravelersDone);
	sprintf(message[2], "CSC 412 Final Project");
	sprintf(message[3], "Simulation run time: %ld s", time(NULL)-launchTime);
	
	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//---------------------------------------------------------
	drawMessages(numMessages, message);
}

void handleKeyboardEvent (unsigned char c, int x, int y) {
    int ok = 0;

    switch (c) {
        case 27: // 'esc' to quit
            exit(0);
            break;
        case 'a':
            slowdownTravelers();
            ok = 1;
            break;
        case 'd':
            speedupTravelers();
            ok = 1;
            break;
        default:
            ok = 1;
            break;
    }

    if (!ok) {
        // do something?
    }
}

//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
void speedupTravelers (void) {
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * travelerSleepTime) / 10;
	
	if (newSleepTime > MIN_SLEEP_TIME) {
		travelerSleepTime = newSleepTime;
	}
}

void slowdownTravelers (void) {
	//	increase sleep time by 20%.  No upper limit on sleep time.
	//	We can slow everything down to admistrative pace if we want.
	travelerSleepTime = (12 * travelerSleepTime) / 10;
}

//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function besides
//	initialization of the various global variables and lists
//------------------------------------------------------------------------
int main (int argc, char* argv[]) {
	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of travelers, etc.
	//	So far, I hard code-some values
	numRows = std::atoi(argv[2]);
	numCols = std::atoi(argv[1]);
	numTravelers = std::atoi(argv[3]);
	(argc > 4) ? numAddSegments = std::atoi(argv[4]) : numAddSegments = INT_MAX;
	numLiveThreads = 0;
	numTravelersDone = 0;

	//	Even though we extracted the relevant information from the argument
	//	list, I still need to pass argc and argv to the front-end init
	//	function because that function passes them to glutInit, the required call
	//	to the initialization of the glut library.
	initializeFrontEnd(argc, argv);

	//	Now we can do application-level initialization
	initializeApplication();

	launchTime = time(NULL);

	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that 
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();

	// Join all threads.
	for (unsigned int k = 0; k < threadList.size(); k++) {
		threadList[k].join();
	}
	
	//	Free allocated resource before leaving (not absolutely needed, but
	//	just nicer.  Also, if you crash there, you know something is wrong
	//	in your code.
	for (unsigned int i=0; i< numRows; i++)
		delete []grid[i];
	delete []grid;
	for (int k=0; k<MAX_NUM_MESSAGES; k++)
		delete []message[k];
	delete []message;
	
	//	This will probably never be executed (the exit point will be in one of the
	//	call back functions).
	return 0;
}


//==================================================================================
//
//	This is a function that you have to edit and add to.
//
//==================================================================================
void initializeApplication (void) {
	//	Initialize some random generators
	rowGenerator = uniform_int_distribution<unsigned int>(0, numRows-1);
	colGenerator = uniform_int_distribution<unsigned int>(0, numCols-1);

	//	Allocate the grid
	grid = new SquareType*[numRows];
	for (unsigned int i=0; i<numRows; i++) {
		grid[i] = new SquareType[numCols];
		for (unsigned int j=0; j< numCols; j++)
			grid[i][j] = SquareType::FREE_SQUARE;
	}

	message = new char*[MAX_NUM_MESSAGES];

	for (unsigned int k=0; k<MAX_NUM_MESSAGES; k++)
		message[k] = new char[MAX_LENGTH_MESSAGE+1];
	
	//---------------------------------------------------------------
	//	All the code below to be replaced/removed
	//	I initialize the grid's pixels to have something to look at
	//---------------------------------------------------------------
	//	Yes, I am using the C random generator after ranting in class that the C random
	//	generator was junk.  Here I am not using it to produce "serious" data (as in a
	//	real simulation), only wall/partition location and some color

	srand((unsigned int) time(NULL));

	//	generate a random exit
	exitPos = getNewFreePosition();
	grid[exitPos.row][exitPos.col] = SquareType::EXIT;

	//	Generate walls and partitions
	generateWalls();
	generatePartitions();
	
	//	Initialize traveler info structs
	//	You will probably need to replace/complete this as you add thread-related data
	float** travelerColor = createTravelerColors(numTravelers);

	for (unsigned int k = 0; k < numTravelers; k++) {
		GridPosition pos = getNewFreePosition();
		//	Note that treating an enum as a sort of integer is increasingly
		//	frowned upon, as C++ versions progress
		Direction dir = static_cast<Direction>(segmentDirectionGenerator(engine));

		TravelerSegment seg = {pos.row, pos.col, dir};
		Traveler traveler;
		traveler.segmentList.push_back(seg);
		grid[pos.row][pos.col] = SquareType::TRAVELER;

		//    I add 0-n segments to my travelers
 	   unsigned int numAddSegments = segmentNumberGenerator(engine);
 	   TravelerSegment currSeg = traveler.segmentList[0];
 	   bool canAddSegment = true;
 	   cout << "Traveler " << k << " at (row=" << pos.row << ", col=" <<
 	   pos.col << "), direction: " << dirStr(dir) << ", with up to " << numAddSegments << " additional segments" << endl;
 	   cout << "\t";

    	for (unsigned int s=0; s<numAddSegments && canAddSegment; s++){
    	    	TravelerSegment newSeg = newTravelerSegment(currSeg, canAddSegment);
   	     	if (canAddSegment){
    	        traveler.segmentList.push_back(newSeg);
    	        grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
    	        currSeg = newSeg;
    	        cout << dirStr(newSeg.dir) << "  ";
    	    }
    	}	
    	cout << endl;

		for (unsigned int c=0; c<4; c++)
			traveler.rgba[c] = travelerColor[k][c];
		
		travelerList.push_back(traveler);
	}
	
	//	free array of colors
	for (unsigned int k=0; k<numTravelers; k++)
		delete []travelerColor[k];
	delete []travelerColor;

	// Now that we have the traveler list, we can start the threads
	for (unsigned int k = 0; k < numTravelers; k++) {
		TravelerThreadInfo *info = new TravelerThreadInfo;
		info->traveler = &travelerList[k];
		info->sleepTime = &travelerSleepTime;

		threadList.push_back(thread(moveThreaded, info));
		numLiveThreads++;
	}
}

void moveThreaded (TravelerThreadInfo *info) {
	Traveler *traveler = info->traveler;

	// ..step once (if not at the exit already)!
	while (!(traveler->segmentList[0].row == exitPos.row && traveler->segmentList[0].col == exitPos.col)) {
		usleep((*info->sleepTime) * 1000);

		// Lock
		lock_guard<mutex> lock(travelerMutex);

		// Begin
		std::cout << "BEGIN moveTraveler()" << endl;
		vector<Direction> legalDirectionList = getLegalDirectionList(*traveler);

		Direction newDirection = getNewDirection(*traveler, legalDirectionList);
		std::cout << "\tTRAVELER START AT ROW " << traveler->segmentList[0].row << " COL " << traveler->segmentList[0].col << endl;

		TravelerSegment seg;

		switch (newDirection) {
			case Direction::NORTH:
				std::cout << "\tNORTH" << endl;
				grid[traveler->segmentList[0].row - 1][traveler->segmentList[0].col] = SquareType::TRAVELER;
				seg = {traveler->segmentList[0].row - 1, traveler->segmentList[0].col, newDirection};
				traveler->segmentList.insert(traveler->segmentList.begin(), seg);
				if (numAddSegments != info->counter) {
					removeSegment(traveler);
				}
				else {
					info->counter = 0;
				}
				info->counter++;
				break;
			case Direction::SOUTH:
				std::cout << "\tSOUTH" << endl;
				grid[traveler->segmentList[0].row + 1][traveler->segmentList[0].col] = SquareType::TRAVELER;
				seg = {traveler->segmentList[0].row + 1, traveler->segmentList[0].col, newDirection};
				traveler->segmentList.insert(traveler->segmentList.begin(), seg);
				if (numAddSegments != info->counter) {
					removeSegment(traveler);
				}
				else {
					info->counter = 0;
				}
				info->counter++;
				break;
			case Direction::EAST:
				std::cout << "\tEAST" << endl;
				grid[traveler->segmentList[0].row][traveler->segmentList[0].col + 1] = SquareType::TRAVELER;
				seg = {traveler->segmentList[0].row, traveler->segmentList[0].col + 1, newDirection};
				traveler->segmentList.insert(traveler->segmentList.begin(), seg);
				if (numAddSegments != info->counter) {
					removeSegment(traveler);
				}
				else {
					info->counter = 0;
				}
				info->counter++;
				break;
			case Direction::WEST:
				std::cout << "\tWEST" << endl;
				grid[traveler->segmentList[0].row][traveler->segmentList[0].col - 1] = SquareType::TRAVELER;
				seg = {traveler->segmentList[0].row, traveler->segmentList[0].col - 1, newDirection};
				traveler->segmentList.insert(traveler->segmentList.begin(), seg);
				if (numAddSegments != info->counter) {
					removeSegment(traveler);
				}
				else {
					info->counter = 0;
				}
				info->counter++;
				break;
			case Direction::NUM_DIRECTIONS:
				removeSegment(traveler);
				break;
		}

		// while (traveler->segmentList.size() > 2) {
		// 	TravelerSegment seg = traveler->segmentList.back();
		// 	traveler->segmentList.pop_back();
		// 	grid[seg.row][seg.col] = SquareType::FREE_SQUARE;
		// }

		std::cout << "\tTRAVELER MOVE TO ROW " << traveler->segmentList[0].row << " COL " << traveler->segmentList[0].col << endl;
		std::cout << "\tEND      ROW " << exitPos.row << " COL " << exitPos.col << endl;

		if (traveler->segmentList[0].row == exitPos.row && traveler->segmentList[0].col == exitPos.col) {
			numTravelersDone++;
			std::cout << "\tTRAVELER DONE" << endl;
			grid[exitPos.row][exitPos.col] = SquareType::EXIT;
		}

		std::cout << "END moveTraveler()" << endl;
	}
	
	while (traveler->segmentList.size() > 1) {
		usleep((*info->sleepTime) * 1000);

		TravelerSegment seg = traveler->segmentList.back();
		traveler->segmentList.pop_back();
		grid[seg.row][seg.col] = SquareType::FREE_SQUARE;
	}
	numLiveThreads--;
}

void removeSegment(Traveler *traveler) {
	if (traveler->segmentList.size() > 1) {
		TravelerSegment seg = traveler->segmentList.back();
		traveler->segmentList.pop_back();
		grid[seg.row][seg.col] = SquareType::FREE_SQUARE;
	}
}

string checkSpace(int row, int col) {
	if (grid[row][col] == SquareType::FREE_SQUARE) {
			return "FREE";
	}
	if (grid[row][col] == SquareType::EXIT) {
			return "Exit";
	}
	if (grid[row][col] == SquareType::WALL) {
			return "Wall";
	}
	if (grid[row][col] == SquareType::HORIZONTAL_PARTITION || grid[row ][col] == SquareType::VERTICAL_PARTITION) {
			return "Partition";
	}
	if (grid[row][col] == SquareType::TRAVELER) {
			return "Traveler";
	}

	return "Out of Bounds";
}

vector<Direction> getLegalDirectionList (Traveler &traveler) {
	std::cout << "BEGIN getLegalDirectionList()" << endl;
	vector<Direction> legalDirectionList(0);

	std::cout << traveler.segmentList.size() << " Size" << endl;
	int row = traveler.segmentList[0].row;
	int col = traveler.segmentList[0].col;

	/* Check NORTH. */
	if (row - 1 >= 0) {
		std::cout << "\tChecking NORTH" << endl;
		if (grid[row - 1][col] == SquareType::FREE_SQUARE || grid[row - 1][col] == SquareType::EXIT) {
			legalDirectionList.push_back(Direction::NORTH);
		}
		std::cout << "\tNorth: " << checkSpace(row - 1, col) << endl;
		std::cout << "\tChecking NORTH-END" << endl;
	}

	/* Check SOUTH. */
	if (row + 1 < numRows) {
		std::cout << "\tChecking SOUTH" << endl;
		if (grid[row + 1][col] == SquareType::FREE_SQUARE || grid[row + 1][col] == SquareType::EXIT) {
			legalDirectionList.push_back(Direction::SOUTH);
		}
		std::cout << "\tSouth: " << checkSpace(row + 1, col) << endl;
		std::cout << "\tChecking SOUTH-END" << endl;
	}

	/* Check EAST. */
	if (col + 1 < numCols) {
		std::cout << "\tChecking EAST" << endl;
		if (grid[row][col + 1] == SquareType::FREE_SQUARE || grid[row][col + 1] == SquareType::EXIT) {
			legalDirectionList.push_back(Direction::EAST);
		}
		std::cout << "\tEast: " << checkSpace(row, col + 1) << endl;
		std::cout << "\tChecking EAST-END" << endl;
	}

	/* Check WEST. */
	if (col - 1 >= 0) {
		std::cout << "\tChecking WEST" << endl;
		if (grid[row][col - 1] == SquareType::FREE_SQUARE || grid[row][col - 1] == SquareType::EXIT) {
			legalDirectionList.push_back(Direction::WEST);
		}
		std::cout << "\tWest: " << checkSpace(row, col - 1) << endl;
		std::cout << "\tChecking WEST-END" << endl;
	}

	std::cout << "\tFound all legal directions." << endl;

	/* Print the list. */
	for (int i = 0; i < legalDirectionList.size(); i++) {
		std::cout << "\tLEGAL DIRECTION: " << dirStr(legalDirectionList[i]) << endl;
	}

	std::cout << "END getLegalDirectionList()" << endl;

	return legalDirectionList;
}

Direction getNewDirection (Traveler &traveler, vector<Direction> legalDirectionList) {
	if (legalDirectionList.size() == 0) {
		std::cout << "HARD STOP" << endl;
		return Direction::NUM_DIRECTIONS;
		//exit(1);
	}

	int xDist = traveler.segmentList[0].col - exitPos.col;
	int yDist = traveler.segmentList[0].row - exitPos.row;

	cout << "xDist: " << xDist << endl;
	cout << "yDist: " << yDist << endl;

	if (abs(xDist) > abs(yDist)) {
		if (xDist < 0) {
			if (isLegalDirection(Direction::EAST, legalDirectionList)) {
				cout << "EAST" << endl;
				return Direction::EAST;
			}
			return legalDirectionList[rand() % legalDirectionList.size()];
		}
		if (xDist > 0) {
			if (isLegalDirection(Direction::WEST, legalDirectionList)) {
				cout << "WEST" << endl;
				return Direction::WEST;
			}
			return legalDirectionList[rand() % legalDirectionList.size()];
		}
	}
	
	if (abs(xDist) < abs(yDist)) {
		if (yDist > 0) {
			if (isLegalDirection(Direction::NORTH, legalDirectionList)) {
				cout << "NORTH" << endl;
				return Direction::NORTH;
			}
			return legalDirectionList[rand() % legalDirectionList.size()];
		}
		if (yDist < 0) {
			if (isLegalDirection(Direction::SOUTH, legalDirectionList)) {
				cout << "SOUTH" << endl;
				return Direction::SOUTH;
			}
			return legalDirectionList[rand() % legalDirectionList.size()];
		}
	}
	return legalDirectionList[rand() % legalDirectionList.size()];
}

bool isLegalDirection (Direction check, vector<Direction> legalDirectionList) {
	for (int i = 0; i < legalDirectionList.size(); i++) {
		if (check == legalDirectionList[i]) {
			return true;
		}
	}

	return false;
}

//------------------------------------------------------
#if 0
#pragma mark -
#pragma mark Generation Helper Functions
#endif
//------------------------------------------------------

GridPosition getNewFreePosition(void)
{
	GridPosition pos;

	bool noGoodPos = true;
	while (noGoodPos)
	{
		unsigned int row = rowGenerator(engine);
		unsigned int col = colGenerator(engine);
		if (grid[row][col] == SquareType::FREE_SQUARE)
		{
			pos.row = row;
			pos.col = col;
			noGoodPos = false;
		}
	}
	return pos;
}

Direction newDirection(Direction forbiddenDir)
{
	bool noDir = true;

	Direction dir = Direction::NUM_DIRECTIONS;
	while (noDir)
	{
		dir = static_cast<Direction>(segmentDirectionGenerator(engine));
		noDir = (dir==forbiddenDir);
	}
	return dir;
}

TravelerSegment newTravelerSegment(const TravelerSegment& currentSeg, bool& canAdd)
{
	TravelerSegment newSeg;
	switch (currentSeg.dir)
	{
		case Direction::NORTH:
			if (	currentSeg.row < numRows-1 &&
					grid[currentSeg.row+1][currentSeg.col] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row+1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(Direction::SOUTH);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::SOUTH:
			if (	currentSeg.row > 0 &&
					grid[currentSeg.row-1][currentSeg.col] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row-1;
				newSeg.col = currentSeg.col;
				newSeg.dir = newDirection(Direction::NORTH);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::WEST:
			if (	currentSeg.col < numCols-1 &&
					grid[currentSeg.row][currentSeg.col+1] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col+1;
				newSeg.dir = newDirection(Direction::EAST);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;

		case Direction::EAST:
			if (	currentSeg.col > 0 &&
					grid[currentSeg.row][currentSeg.col-1] == SquareType::FREE_SQUARE)
			{
				newSeg.row = currentSeg.row;
				newSeg.col = currentSeg.col-1;
				newSeg.dir = newDirection(Direction::WEST);
				grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
				canAdd = true;
			}
			//	no more segment
			else
				canAdd = false;
			break;
		
		default:
			canAdd = false;
	}
	
	return newSeg;
}

void generateWalls(void)
{
	const unsigned int NUM_WALLS = (numCols+numRows)/4;

	//	I decide that a wall length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_WALL_LENGTH = 3;
	const unsigned int MAX_HORIZ_WALL_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_WALL_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodWall = true;
	
	//	Generate the vertical walls
	for (unsigned int w=0; w< NUM_WALLS; w++)
	{
		goodWall = false;
		
		//	Case of a vertical wall
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_WALLS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*HSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int row=startRow, i=0; i<length && goodWall; i++, row++)
					{
						grid[row][col] = SquareType::WALL;
					}
				}
			}
		}
		// case of a horizontal wall
		else
		{
			goodWall = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodWall; k++)
			{
				//	let's be hopeful
				goodWall = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_WALLS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_WALLS/2-1))*VSP;
				unsigned int length = MIN_WALL_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_WALL_LENGTH-MIN_WALL_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodWall = false;
				}
				
				//	if the wall first, add it to the grid
				if (goodWall)
				{
					for (unsigned int col=startCol, i=0; i<length && goodWall; i++, col++)
					{
						grid[row][col] = SquareType::WALL;
					}
				}
			}
		}
	}
}


void generatePartitions(void)
{
	const unsigned int NUM_PARTS = (numCols+numRows)/4;

	//	I decide that a partition length  cannot be less than 3  and not more than
	//	1/4 the grid dimension in its Direction
	const unsigned int MIN_PARTITION_LENGTH = 3;
	const unsigned int MAX_HORIZ_PART_LENGTH = numCols / 3;
	const unsigned int MAX_VERT_PART_LENGTH = numRows / 3;
	const unsigned int MAX_NUM_TRIES = 20;

	bool goodPart = true;

	for (unsigned int w=0; w< NUM_PARTS; w++)
	{
		goodPart = false;
		
		//	Case of a vertical partition
		if (headsOrTails(engine))
		{
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int HSP = numCols/(NUM_PARTS/2+1);
				unsigned int col = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*HSP + HSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_VERT_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startRow = unsignedNumberGenerator(engine)%(numRows-length);
				for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the partition is possible,
				if (goodPart)
				{
					//	add it to the grid and to the partition list
					SlidingPartition part;
					part.isVertical = true;
					for (unsigned int row=startRow, i=0; i<length && goodPart; i++, row++)
					{
						grid[row][col] = SquareType::VERTICAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
				}
			}
		}
		// case of a horizontal partition
		else
		{
			goodPart = false;
			
			//	I try a few times before giving up
			for (unsigned int k=0; k<MAX_NUM_TRIES && !goodPart; k++)
			{
				//	let's be hopeful
				goodPart = true;
				
				//	select a column index
				unsigned int VSP = numRows/(NUM_PARTS/2+1);
				unsigned int row = (1+ unsignedNumberGenerator(engine)%(NUM_PARTS/2-2))*VSP + VSP/2;
				unsigned int length = MIN_PARTITION_LENGTH + unsignedNumberGenerator(engine)%(MAX_HORIZ_PART_LENGTH-MIN_PARTITION_LENGTH+1);
				
				//	now a random start row
				unsigned int startCol = unsignedNumberGenerator(engine)%(numCols-length);
				for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
				{
					if (grid[row][col] != SquareType::FREE_SQUARE)
						goodPart = false;
				}
				
				//	if the wall first, add it to the grid and build SlidingPartition object
				if (goodPart)
				{
					SlidingPartition part;
					part.isVertical = false;
					for (unsigned int col=startCol, i=0; i<length && goodPart; i++, col++)
					{
						grid[row][col] = SquareType::HORIZONTAL_PARTITION;
						GridPosition pos = {row, col};
						part.blockList.push_back(pos);
					}
					partitionList.push_back(part);
				}
			}
		}
	}
}