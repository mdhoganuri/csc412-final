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

//	feel free to "un-use" std if this is against your beliefs.
using namespace std;

#if 0
//-----------------------------------------------------------------------------
#pragma mark -
#pragma mark Private Functions' Prototypes
//-----------------------------------------------------------------------------
#endif

void initializeApplication(void);
	void startTravelers(void);
	pair<int, int> getDist(GridPosition start);
	vector<Direction> getLegalDirections (Traveler &traveler);
	Direction getOptimalDirection (Traveler &traveler);
	void moveTraveler (Traveler &traveler, Direction dir);
	bool checkSegments (Traveler &traveler, int row, int col);
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
unsigned int numTravelersDone = 0;
unsigned int numLiveThreads = 0;		//	the number of live traveler threads
vector<Traveler> travelerList;
vector<SlidingPartition> partitionList;
GridPosition	exitPos;	//	location of the exit

//	travelers' sleep time between moves (in microseconds)
const int MIN_SLEEP_TIME = 1000;
int travelerSleepTime = 100000;

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

unsigned int numSegmentsPerTraveler = segmentNumberGenerator(engine);

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

void drawTravelers(void)
{
	//-----------------------------
	//	You may have to sychronize things here
	//-----------------------------
	for (unsigned int k=0; k<travelerList.size(); k++)
	{
		//	here I would test if the traveler thread is still live
		drawTraveler(travelerList[k]);
	}
}

void updateMessages(void)
{
	//	Here I hard-code a few messages that I want to see displayed
	//	in my state pane.  The number of live robot threads will
	//	always get displayed.  No need to pass a message about it.
	unsigned int numMessages = 4;
	sprintf(message[0], "We created %d travelers", numTravelers);
	sprintf(message[1], "%d travelers solved the maze", numTravelersDone);
	sprintf(message[2], "I like cheese");
	sprintf(message[3], "Simulation run time: %ld s", time(NULL)-launchTime);
	
	//---------------------------------------------------------
	//	This is the call that makes OpenGL render information
	//	about the state of the simulation.
	//
	//	You *must* synchronize this call.
	//---------------------------------------------------------
	drawMessages(numMessages, message);
}

void handleKeyboardEvent(unsigned char c, int x, int y)
{
	int ok = 0;

	switch (c)
	{
		//	'esc' to quit
		case 27:
			exit(0);
			break;

		//	slowdown
		case ',':
			slowdownTravelers();
			ok = 1;
			break;

		//	speedup
		case '.':
			speedupTravelers();
			ok = 1;
			break;

		default:
			ok = 1;
			break;
	}
	if (!ok)
	{
		//	do something?
	}
}


//------------------------------------------------------------------------
//	You shouldn't have to touch this one.  Definitely if you don't
//	add the "producer" threads, and probably not even if you do.
//------------------------------------------------------------------------
void speedupTravelers(void)
{
	//	decrease sleep time by 20%, but don't get too small
	int newSleepTime = (8 * travelerSleepTime) / 10;
	
	if (newSleepTime > MIN_SLEEP_TIME)
	{
		travelerSleepTime = newSleepTime;
	}
}

void slowdownTravelers(void)
{
	//	increase sleep time by 20%.  No upper limit on sleep time.
	//	We can slow everything down to admistrative pace if we want.
	travelerSleepTime = (12 * travelerSleepTime) / 10;
}




//------------------------------------------------------------------------
//	You shouldn't have to change anything in the main function besides
//	initialization of the various global variables and lists
//------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	/*
		WIDTH, HEIGHT, NUM_TRAVELERS, NUM_MOVES_PER_SEGMENT
	*/

	//	We know that the arguments  of the program  are going
	//	to be the width (number of columns) and height (number of rows) of the
	//	grid, the number of travelers, etc.
	//	So far, I hard code-some values
	numRows = 30; //std::atoi(argv[2]); // Height
	numCols = 35; //std::atoi(argv[1]); // Width
	numTravelers = 1; //std::atoi(argv[3]);
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

	startTravelers();

	//	Now we enter the main loop of the program and to a large extend
	//	"lose control" over its execution.  The callback functions that 
	//	we set up earlier will be called when the corresponding event
	//	occurs
	glutMainLoop();
	
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


void initializeApplication(void)
{
	//	Initialize some random generators
	rowGenerator = uniform_int_distribution<unsigned int>(0, numRows-1);
	colGenerator = uniform_int_distribution<unsigned int>(0, numCols-1);

	//	Allocate the grid
	grid = new SquareType*[numRows];
	for (unsigned int i=0; i<numRows; i++)
	{
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
	for (unsigned int k=0; k<numTravelers; k++) {
		Direction dir = static_cast<Direction>(segmentDirectionGenerator(engine));

		Traveler traveler;
		traveler.pos = getNewFreePosition();
		TravelerSegment seg = {traveler.pos.row, traveler.pos.col, dir};
		traveler.segmentList.push_back(seg);
		grid[traveler.pos.row][traveler.pos.col] = SquareType::TRAVELER;         //X and Y starting positions of traveler

        //    I add 0-n segments to my travelers
        unsigned int numAddSegments = numSegmentsPerTraveler;
        TravelerSegment currSeg = traveler.segmentList[0];
        bool canAddSegment = true;
		cout << "IDX: " << traveler.index << endl;
        cout << "Traveler " << k << " at (row=" << traveler.pos.row << ", col=" <<
        traveler.pos.col << "), direction: " << dirStr(dir) << ", with up to " << numAddSegments << " additional segments" << endl;
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
}

void startTravelers () {
	/* Controls the movement (and, in later versions, threading) of the travelers. */
	for (int i = 0; i < travelerList.size(); i++) {
		cout << "Traveler " << i << " is moving..." << endl;

		/* Step toward the goal position. */
		do {
			/* Get the optimal direction to travel in. */
			Direction optimalDirection = getOptimalDirection(travelerList[i]);
			
			/* Move in that direction. */
			moveTraveler(travelerList[i], optimalDirection);
			
		} while (!(travelerList[i].pos.row == exitPos.row && travelerList[i].pos.col == exitPos.col));

		numTravelersDone ++;
		cout << "...DONE!" << endl;
	}
}

pair<int, int> getDist(GridPosition start) {
	pair<int, int> distances;
	
	distances.first = exitPos.col - start.col;
	distances.second = exitPos.row - start.row;

	return distances;
}

/**
 * @brief Creates a list of directions that are legal for a given traveler.
 * 
 * @param traveler The traveler at hand!
 * @return vector<Direction> List of possible directions the traveler can move in.
 */
vector<Direction> getLegalDirections (Traveler &traveler) {
	cout << "\tBEGIN getLegalDirections()" << endl;

	vector<Direction> legalDirections;

	/* NORTH */
	if (traveler.pos.row - 1 > 0
	&& grid[traveler.pos.row - 1][traveler.pos.col] == SquareType::FREE_SQUARE
	&& checkSegments(traveler, traveler.pos.row - 1, traveler.pos.col)) {
		legalDirections.push_back(Direction::NORTH);
	}

	cout << "\t\tPASSED north" << endl;

	/* SOUTH */
	if (traveler.pos.row + 1 < numRows
	&& grid[traveler.pos.row + 1][traveler.pos.col] == SquareType::FREE_SQUARE
	&& checkSegments(traveler, traveler.pos.row + 1, traveler.pos.col)) {
		legalDirections.push_back(Direction::SOUTH);
	}

	cout << "\t\tPASSED south" << endl;

	/* EAST */
	if (traveler.pos.col + 1 < numCols
	&& grid[traveler.pos.row][traveler.pos.col + 1] == SquareType::FREE_SQUARE
	&& checkSegments(traveler, traveler.pos.row, traveler.pos.col + 1)) {
		legalDirections.push_back(Direction::EAST);
	}

	cout << "\t\tPASSED east" << endl;

	/* WEST */
	if (traveler.pos.col - 1 > 0
	&& grid[traveler.pos.row][traveler.pos.col - 1] == SquareType::FREE_SQUARE
	&& checkSegments(traveler, traveler.pos.row, traveler.pos.col - 1)) {
		legalDirections.push_back(Direction::WEST);
	}

	cout << "\t\tPASSED west" << endl;

	cout << "\tEND getLegalDirections()" << endl;

	return legalDirections;
}

/**
 * @brief Get the optimal direction for a given traveler using a list of legal directions.
 * 
 * @param traveler The traveler at hand!
 * @return Direction The direction the traveler should move in.
 */
Direction getOptimalDirection (Traveler &traveler) {
	cout << "\tBEGIN getOptimalDirection()" << endl;

	// /* Get the distances (x, y) from the traveler to the goal. */
	// pair<int, int> distances = getDist(traveler.pos);
	
	// /* Get the list of legal directions. */
	vector<Direction> legalDirections = getLegalDirections(traveler);

	if (legalDirections.size() == 0) {
	 	cerr << "\t\tERROR: No legal directions found." << endl;
	}
	cout << "\tTest: Legal Direction" << endl;
	// /* If there is only one legal direction, return it. */
	// if (legalDirections.size() == 1) {
	// 	return legalDirections[0];
	// }

	// for (int i = 0; i < legalDirections.size(); i++) {
	// 	/* If the traveler is closer to the goal in the NORTH direction, return NORTH. */
	// 	if (legalDirections[i] == Direction::NORTH && distances.second < 0) {
	// 		cout << "NORTH" << endl;
	// 		return Direction::NORTH;
	// 	}
	// 	/* If the traveler is closer to the goal in the EAST direction, return EAST. */
	// 	else if (legalDirections[i] == Direction::EAST && distances.first > 0) {
	// 		cout << "EAST" << endl;
	// 		return Direction::EAST;
	// 	}
	// 	/* If the traveler is closer to the goal in the SOUTH direction, return SOUTH. */
	// 	else if (legalDirections[i] == Direction::SOUTH && distances.second > 0) {
	// 		cout << "SOUTH" << endl;
	// 		return Direction::SOUTH;
	// 	}
	// 	/* If the traveler is closer to the goal in the WEST direction, return WEST. */
	// 	else if (legalDirections[i] == Direction::WEST && distances.first < 0) {
	// 		cout << "WEST" << endl;
	// 		return Direction::WEST;
	// 	}
	// }

	/* If the traveler is equidistant from the goal in both directions, return a random direction. */
	Direction returnValue = legalDirections[rand() % legalDirections.size()];
	cout << "\tRandom: " << dirStr(returnValue) << endl;

	cout << "\tEND getOptimalDirection()" << endl;

	return returnValue;
}

/**
 * @brief Make a traveler move in a given direction.
 * 
 * @param traveler The traveler at hand!
 * @param dir The direction the traveler should move in.
 */
void moveTraveler (Traveler &traveler, Direction dir) {
	cout << "\tBEGIN moveTraveler()" << endl;
    grid[traveler.pos.row][traveler.pos.col] = SquareType::FREE_SQUARE;
	cout << "\t\t ROW: " << traveler.pos.row << ", COL: " << traveler.pos.col << " -> " << dirStr(dir) << endl;

	switch (dir) {
		case Direction::NORTH:
			traveler.pos.row --;
			break;
		case Direction::SOUTH:
			traveler.pos.row ++;
			break;
		case Direction::EAST:
			traveler.pos.col ++;
			break;
		case Direction::WEST:
			traveler.pos.col --;
			break;
		default:
			cerr << "ERROR: Invalid movement detected." << endl;
			break;
	}

	/* Adjust this traveler's segment list to account for movement. */
	TravelerSegment newSeg = {traveler.pos.row, traveler.pos.col, dir};
    grid[newSeg.row][newSeg.col] = SquareType::TRAVELER;
	traveler.segmentList.push_back(newSeg);
	traveler.segmentList.pop_back();

	cout << "\tEND moveTraveler()" << endl;
}

/**
 * @brief Check the traveler's segments
 * 
 * @param traveler The traveler at hand!
 * @param row The row direction the traveler should move in.
 * @param col The column direction the traveler should move in.
*/
bool checkSegments (Traveler &traveler, int row, int col) {
	cout << "\tBEGIN checkSegments()" << endl;

	// check segments
	for (int i = 0; i < traveler.segmentList.size(); i++) {
		if (traveler.segmentList[i].col == col && traveler.segmentList[i].row == row) {
			cout << "\tEND checkSegments()" << endl;
			return false;
		}
	}

	cout << "\tEND checkSegments()" << endl;

	return true;
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

