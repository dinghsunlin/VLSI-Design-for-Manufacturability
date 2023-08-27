#include <algorithm>
#include <array>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <queue>
#include <set>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#include <omp.h>

#define WINDOW_MOVING_STEP 4
#define SAFE_SPACING 1600

using namespace std;

enum DIRECTION { Horizontal, Vertical };
// Order: Empty < Critical < Spacing < Conductor < Dummy
enum GRIDTYPE { Empty, Critical, Reserved, Spacing, Conductor, Dummy };

struct CRITICALNET
{
	unordered_set<int> netID;
	unordered_map<int, vector<int>> conductorID;
};

struct LAYER
{
	int layerID, minWidth, minSpacing, maxWidth;
	float minDensity, maxDensity, weight;
	DIRECTION direction;
	vector<int> conductorID;

	int gridSize() { return max<int>(minWidth, minSpacing); }
};

struct CONDUCTOR
{
	int conductorID, left, bottom, right, top, netID, layerID;
};

struct DUMMY
{
	bool inserted;
	int dummyID, left, bottom, right, top, layerID;

	int area() { return (right - left) * (top - bottom); }
};

struct GRID
{
	GRIDTYPE type = GRIDTYPE::Empty;
	int x, y, xSeperate = -1, ySeperate = -1;
	bool xDensitySeperate = false, yDensitySeperate = false;
	int density[2][2] = {{0, -1}, {-1, -1}};
	vector<int> conductorID;
	vector<int> dummyID;
};

struct DENSITYGRID
{
	unsigned original = 0, window = 0;
	unordered_set<int> criticalDummyID;
};

void readFile(const char * file,
			  int & xMin, int & xMax, int & yMin, int & yMax, int & window,
			  int & numCritical, int & numLayer, int & numConductor,
			  CRITICALNET & criticalNet, vector<LAYER> & layerInfo,
			  vector<CONDUCTOR> & conductorInfo)
{
	fstream input;
	input.open(file, ios::in);

	input >> xMin >> yMin >> xMax >> yMax >> window
		  >> numCritical >> numLayer >> numConductor;

	for(int i = 0; i < numCritical; i++)
	{
		int tmp;
		input >> tmp;
		criticalNet.netID.emplace(tmp);
		criticalNet.conductorID[tmp];
	}

	layerInfo.resize(numLayer + 1);
	for(int i = 0; i < numLayer; i++)
	{
		int a, b, c, d;
		float e, f, g;
		input >> a >> b >> c >> d >> e >> f >> g;
		LAYER tmp = {.layerID = a, .minWidth = b, .minSpacing = c, .maxWidth = d,
					 .minDensity = e, .maxDensity = f, .weight = g};
		layerInfo[a] = tmp;
	}

	conductorInfo.resize(numConductor + 1);
	for(int i = 0; i < numConductor; i++)
	{
		int a, b, c, d, e, f, g;
		input >> a >> b >> c >> d >> e >> f >> g;
		CONDUCTOR tmp = {.conductorID = a, .left = b, .bottom = c, .right = d, .top = e,
						 .netID = f, .layerID = g};
		conductorInfo[a] = tmp;

		if(criticalNet.netID.find(f) != criticalNet.netID.end())
			criticalNet.conductorID[f].emplace_back(a);

		layerInfo[g].conductorID.emplace_back(a);
	}

	input.close();
}

vector<vector<GRID>> gridCreation(const int & xMin, const int & xMax, const int & yMin, const int & yMax,
								  const int & window, const CRITICALNET & criticalNet, LAYER & layer,
								  const vector<CONDUCTOR> & conductorInfo)
{
	const int gridWidthNum = (xMax - 1 - xMin) / layer.gridSize() + 1;
	const int gridHeightNum = (yMax - 1 - yMin) / layer.gridSize() + 1;
	vector<vector<GRID>> gridInfo (gridWidthNum, vector<GRID> (gridHeightNum));
	
	int horizontal = 0, vertical = 0;
	vector<int> criticalID;
	for(const int & i : layer.conductorID)
	{
		const CONDUCTOR & conductor = conductorInfo[i];

		const int width = conductor.right - conductor.left;
		const int height = conductor.top - conductor.bottom;
		if(width > height)
			horizontal++;
		else if(width < height)
			vertical++;

		if(criticalNet.netID.find(conductor.netID) != criticalNet.netID.end())
			criticalID.emplace_back(i);

		const int left = (conductor.left - xMin) / layer.gridSize();
		const int right = (conductor.right - 1 - xMin) / layer.gridSize();
		const int bottom= (conductor.bottom - yMin) / layer.gridSize();
		const int top= (conductor.top - 1 - yMin) / layer.gridSize();
		for(int x = max<int>(left - 1, 0); (x <= right + 1) && (x < gridWidthNum); x++)
		{
			for(int y = max<int>(bottom - 1, 0); (y <= top + 1) && (y < gridHeightNum); y++)
			{
				GRID & nowGrid = gridInfo[x][y];
				if(x >= left && x <= right && y >= bottom && y <= top)
				{
					nowGrid.type = GRIDTYPE::Conductor;
					nowGrid.conductorID.emplace_back(i);
				}
				else
				{
					if(nowGrid.type < GRIDTYPE::Spacing)
						nowGrid.type = GRIDTYPE::Spacing;
				}
			}
		}
	}

	if(horizontal >= vertical)
		layer.direction = DIRECTION::Horizontal;
	else
		layer.direction = DIRECTION::Vertical;

	for(const int & i : criticalID)
	{
		const CONDUCTOR & conductor = conductorInfo[i];

		const int left = (conductor.left - xMin) / layer.gridSize();
		const int right = (conductor.right - 1 - xMin) / layer.gridSize();
		const int bottom= (conductor.bottom - yMin) / layer.gridSize();
		const int top= (conductor.top - 1 - yMin) / layer.gridSize();

		const int cLeft = max<int>((conductor.left - SAFE_SPACING -xMin) / layer.gridSize(), 0);
		const int cRight = min<int>((conductor.right - 1 + SAFE_SPACING - xMin) / layer.gridSize(), gridWidthNum - 1);
		const int cBottom= max<int>((conductor.bottom - SAFE_SPACING -yMin) / layer.gridSize(), 0);
		const int cTop= min<int>((conductor.top - 1 + SAFE_SPACING - yMin) / layer.gridSize(), gridHeightNum - 1);
		
		for(int y = bottom; y <= top; y++)
		{
			for(int x = max<int>(left - 2, 0); x >= cLeft; x--)
			{
				GRID & nowGrid = gridInfo[x][y];
				if(nowGrid.type == GRIDTYPE::Conductor &&
				   gridInfo[x][max<int>(y - 1, 0)].type == GRIDTYPE::Conductor &&
				   gridInfo[x][min<int>(y + 1, gridHeightNum - 1)].type == GRIDTYPE::Conductor)
					break;
				if(nowGrid.type == GRIDTYPE::Empty)
					nowGrid.type = GRIDTYPE::Critical;
			}
			for(int x = min<int>(right + 2, gridWidthNum - 1); x <= cRight; x++)
			{
				GRID & nowGrid = gridInfo[x][y];
				if(nowGrid.type == GRIDTYPE::Conductor &&
				   gridInfo[x][max<int>(y - 1, 0)].type == GRIDTYPE::Conductor &&
				   gridInfo[x][min<int>(y + 1, gridHeightNum - 1)].type == GRIDTYPE::Conductor)
					break;
				if(nowGrid.type == GRIDTYPE::Empty)
					nowGrid.type = GRIDTYPE::Critical;
			}
		}

		for(int x = left; x <= right; x++)
		{
			for(int y = max<int>(bottom - 2, 0); y >= cBottom; y--)
			{
				GRID & nowGrid = gridInfo[x][y];
				if(nowGrid.type == GRIDTYPE::Conductor &&
				   gridInfo[max<int>(x - 1, 0)][y].type == GRIDTYPE::Conductor &&
				   gridInfo[min<int>(x + 1, gridWidthNum - 1)][y].type == GRIDTYPE::Conductor)
					break;
				if(nowGrid.type == GRIDTYPE::Empty)
					nowGrid.type = GRIDTYPE::Critical;
			}
			for(int y = min<int>(top + 2, gridHeightNum - 1); y <= cTop; y++)
			{
				GRID & nowGrid = gridInfo[x][y];
				if(nowGrid.type == GRIDTYPE::Conductor &&
				   gridInfo[max<int>(x - 1, 0)][y].type == GRIDTYPE::Conductor &&
				   gridInfo[min<int>(x + 1, gridWidthNum - 1)][y].type == GRIDTYPE::Conductor)
					break;
				if(nowGrid.type == GRIDTYPE::Empty)
					nowGrid.type = GRIDTYPE::Critical;
			}
		}
	}

	const int smallWindow = window / WINDOW_MOVING_STEP;
	int xWindow = 1, yWindow = 1;
	for(int x = 0; x < gridWidthNum; x++)
	{
		bool xDensitySeperate = false, xReserveSpacing = false;
		int xSeperate;
		if((x + 1) * layer.gridSize() >= xWindow * smallWindow)
		{
			xDensitySeperate = true;
			xSeperate = xMin + xWindow * smallWindow;
			xWindow++;
		}

		yWindow = 1;
		for(int y = 0; y < gridHeightNum; y++)
		{
			GRID & nowGrid = gridInfo[x][y];
			nowGrid.x = xMin + x * layer.gridSize();
			nowGrid.y = yMin + y * layer.gridSize();
			if(xDensitySeperate)
			{
				nowGrid.xDensitySeperate = true;
				nowGrid.xSeperate = xSeperate;
				nowGrid.density[1][0] = 0;
			}
			if((y + 1) * layer.gridSize() >= yWindow * smallWindow)
			{
				int ySeperate = yMin + yWindow * smallWindow;
				nowGrid.yDensitySeperate = true;
				nowGrid.ySeperate = ySeperate;
				nowGrid.density[0][1] = 0;
				if(xDensitySeperate)
					nowGrid.density[1][1] = 0;
				yWindow++;
			}
		}
	}

	return gridInfo;
}

void dummyInsertion(vector<DUMMY> & dummyInfo, const int & xMax, const int & yMax,
					vector<vector<GRID>> & gridInfo, LAYER & layer,
					const vector<CONDUCTOR> & conductorInfo)
{
	class insertOrderCompare
	{
		DIRECTION direction;
	public:
		insertOrderCompare(const DIRECTION & dir) { direction = dir ;}
		bool operator() (const array<int, 2> & lhs, const array<int, 2> & rhs)
		{
			if(direction == DIRECTION::Horizontal)
				return lhs[1] > rhs[1];
			else if(direction == DIRECTION::Vertical)
				return lhs[0] > rhs[0];
		}
	};
	priority_queue<array<int, 2>, vector<array<int, 2>>, insertOrderCompare> insertOrder(insertOrderCompare(layer.direction));

	auto findNext = [&](int xStart, int yStart) 
    {
		if(layer.direction == DIRECTION::Horizontal)
		{
			for(int x = xStart; x < gridInfo.size(); x++)
			{
				for(int y = yStart; (insertOrder.empty() && y < gridInfo[0].size()) || (!insertOrder.empty() && y < insertOrder.top()[1]); y++)
				{
					if(gridInfo[x][y].type == GRIDTYPE::Empty || gridInfo[x][y].type == GRIDTYPE::Critical)
					{
						insertOrder.push(array<int, 2> {x, y});
						break;
					}
				}
				if(!insertOrder.empty() && insertOrder.top()[1] == yStart)
					break;
			}
		}
		else if(layer.direction == DIRECTION::Vertical)
		{
			for(int y = yStart; y < gridInfo[0].size(); y++)
			{
				for(int x = xStart; (insertOrder.empty() && x < gridInfo.size()) || (!insertOrder.empty() && x < insertOrder.top()[0]); x++)
				{
					if(gridInfo[x][y].type == GRIDTYPE::Empty || gridInfo[x][y].type == GRIDTYPE::Critical)
					{
						insertOrder.push(array<int, 2> {x, y});
						break;
					}
				}
				if(!insertOrder.empty() && insertOrder.top()[0] == xStart)
					break;
			}
		}
    };
	
	if(layer.direction == DIRECTION::Horizontal)
	{
		if(insertOrder.empty())
			findNext(0, 0);

		while(!insertOrder.empty())
		{
			array<int, 2> coordinate = insertOrder.top();
			insertOrder.pop();
			if(gridInfo[coordinate[0]][coordinate[1]].type != GRIDTYPE::Empty && gridInfo[coordinate[0]][coordinate[1]].type != GRIDTYPE::Critical)
				findNext(coordinate[0], coordinate[1]);
			else
			{
				const GRIDTYPE nowType = gridInfo[coordinate[0]][coordinate[1]].type;
				int width, height = layer.maxWidth / layer.gridSize(), height2 = layer.maxWidth / layer.gridSize();
				GRIDTYPE yType = nowType, yType2 = nowType;
				bool same = false;
				for(width = 0; width < layer.maxWidth / layer.gridSize() && coordinate[0] + width < gridInfo.size(); width++)
				{
					if(gridInfo[coordinate[0] + width][coordinate[1]].type != nowType)
					{
						if(nowType == GRIDTYPE::Critical)
						{
							if(gridInfo[coordinate[0] + width][coordinate[1]].type == GRIDTYPE::Empty)
							{
								width--;
								if(same)
								{
									height = height2;
									yType = yType2;
								}
							}
							if(yType == GRIDTYPE::Empty)
								height--;
						}
						break;
					}
					same = false;
					for(int yMove = 1; yMove < height && coordinate[1] + yMove < gridInfo[0].size(); yMove++)
					{
						if(gridInfo[coordinate[0] + width][coordinate[1] + yMove].type != nowType)
						{
							if(width == 0)
							{
								height2 = yMove;
								yType2 = gridInfo[coordinate[0] + width][coordinate[1] + yMove].type;
							}
							else
							{
								height2 = height;
								yType2 = yType;
							}
							height = yMove;
							yType = gridInfo[coordinate[0] + width][coordinate[1] + yMove].type;
							same = true;
						}
					}
				}

				DUMMY newDummy = {.inserted = (nowType == GRIDTYPE::Empty),
								  .dummyID = int(dummyInfo.size()),
								  .left = gridInfo[coordinate[0]][coordinate[1]].x,
								  .bottom = gridInfo[coordinate[0]][coordinate[1]].y,
								  .right = min<int>(gridInfo[coordinate[0]][coordinate[1]].x + width * layer.gridSize(), xMax),
								  .top = min<int>(gridInfo[coordinate[0]][coordinate[1]].y + height * layer.gridSize(), yMax),
								  .layerID = layer.layerID};
				if(newDummy.right - newDummy.left < layer.minWidth || newDummy.top - newDummy.bottom < layer.minWidth)
				{
					findNext(coordinate[0], coordinate[1] + 1);
					findNext(coordinate[0] + 1, coordinate[1]);
					continue;
				}
				
				// cout << " E "; cout.flush();
				dummyInfo.emplace_back(newDummy);
				for(int x = max<int>(coordinate[0] - 1, 0); x <= coordinate[0] + width && x < gridInfo.size(); x++)
				{
					for(int y = coordinate[1]; y <= coordinate[1] + height && y < gridInfo[0].size(); y++)
					{
						GRID & nowGrid = gridInfo[x][y];
						if(x >= coordinate[0] && x < coordinate[0] + width && y >= coordinate[1] && y < coordinate[1] + height)
						{
							if(nowGrid.type == GRIDTYPE::Empty)
								nowGrid.type = GRIDTYPE::Dummy;
							else if(nowGrid.type == GRIDTYPE::Critical)
								nowGrid.type = GRIDTYPE::Reserved;
							nowGrid.dummyID.emplace_back(newDummy.dummyID);
						}
						else
							nowGrid.type = GRIDTYPE::Spacing;
					}

					for(int y = coordinate[1] + height + 1; (insertOrder.empty() && y < gridInfo[0].size()) || (!insertOrder.empty() && y < insertOrder.top()[1]); y++)
					{
						if(gridInfo[x][y].type == GRIDTYPE::Empty || gridInfo[x][y].type == GRIDTYPE::Critical)
						{
							insertOrder.push(array<int, 2> {x, y});
							break;
						}
					}
				}

				findNext(coordinate[0] + width + 1, coordinate[1]);
			}
		}
	}
	else if(layer.direction == DIRECTION::Vertical)
	{
		if(insertOrder.empty())
			findNext(0, 0);
		
		while(!insertOrder.empty())
		{
			array<int, 2> coordinate = insertOrder.top();
			insertOrder.pop();
			
			if(gridInfo[coordinate[0]][coordinate[1]].type != GRIDTYPE::Empty && gridInfo[coordinate[0]][coordinate[1]].type != GRIDTYPE::Critical)
				findNext(coordinate[0], coordinate[1]);
			else
			{
				const GRIDTYPE nowType = gridInfo[coordinate[0]][coordinate[1]].type;
				int width = layer.maxWidth / layer.gridSize(), width2 = layer.maxWidth / layer.gridSize(), height;
				GRIDTYPE xType = nowType, xType2 = nowType;
				bool same = false;
				for(height = 0; height < layer.maxWidth / layer.gridSize() && coordinate[1] + height < gridInfo[0].size(); height++)
				{
					if(gridInfo[coordinate[0]][coordinate[1] + height].type != nowType)
					{
						if(nowType == GRIDTYPE::Critical)
						{
							if(gridInfo[coordinate[0]][coordinate[1] + height].type == GRIDTYPE::Empty)
							{
								height--;
								if(same)
								{
									width = width2;
									xType = xType2;
								}
							}
							if(xType == GRIDTYPE::Empty)
								width--;
						}
						break;
					}
					same = false;
					for(int xMove = 1; xMove < width && coordinate[0] + xMove < gridInfo.size(); xMove++)
					{
						if(gridInfo[coordinate[0] + xMove][coordinate[1] + height].type != nowType)
						{
							if(height == 0)
							{
								width2 = xMove;
								xType2 = gridInfo[coordinate[0] + xMove][coordinate[1] + height].type;
							}
							else
							{
								width2 = width;
								xType2 = xType;
							}
							width = xMove;
							xType = gridInfo[coordinate[0] + xMove][coordinate[1] + height].type;
							same = true;
						}
					}
				}

				DUMMY newDummy = {.inserted = (nowType == GRIDTYPE::Empty),
								  .dummyID = int(dummyInfo.size()),
								  .left = gridInfo[coordinate[0]][coordinate[1]].x,
								  .bottom = gridInfo[coordinate[0]][coordinate[1]].y,
								  .right = min<int>(gridInfo[coordinate[0]][coordinate[1]].x + width * layer.gridSize(), xMax),
								  .top = min<int>(gridInfo[coordinate[0]][coordinate[1]].y + height * layer.gridSize(), yMax),
								  .layerID = layer.layerID};
				if(newDummy.right - newDummy.left < layer.minWidth || newDummy.top - newDummy.bottom < layer.minWidth)
				{
					findNext(coordinate[0] + 1, coordinate[1]);
					findNext(coordinate[0], coordinate[1] + 1);
					continue;
				}

				dummyInfo.emplace_back(newDummy);
				for(int y = max<int>(coordinate[1] - 1, 0); y <= coordinate[1] + height && y < gridInfo[0].size(); y++)
				{
					for(int x = coordinate[0]; x <= coordinate[0] + width && x < gridInfo.size(); x++)
					{
						GRID & nowGrid = gridInfo[x][y];
						if(y >= coordinate[1] && y < coordinate[1] + height && x >= coordinate[0] && x < coordinate[0] + width)
						{
							if(nowGrid.type == GRIDTYPE::Empty)
								nowGrid.type = GRIDTYPE::Dummy;
							else if(nowGrid.type == GRIDTYPE::Critical)
								nowGrid.type = GRIDTYPE::Reserved;
							nowGrid.dummyID.emplace_back(newDummy.dummyID);
						}
						else
							nowGrid.type = GRIDTYPE::Spacing;
					}

					for(int x = coordinate[0] + width + 1; (insertOrder.empty() && x < gridInfo.size()) || (!insertOrder.empty() && x < insertOrder.top()[0]); x++)
					{
						if(gridInfo[x][y].type == GRIDTYPE::Empty || gridInfo[x][y].type == GRIDTYPE::Critical)
						{
							insertOrder.push(array<int, 2> {x, y});
							break;
						}
					}
				}

				findNext(coordinate[0], coordinate[1] + height + 1);
			}
		}
	}
}

void densityRefinement(const int & width, const int & height, const int & window, vector<DUMMY> & dummyInfo,
					   const int & xMin, const int & xMax, const int & yMin, const int & yMax,
					   vector<vector<GRID>> & gridInfo, LAYER & layer, const vector<CONDUCTOR> & conductorInfo)
{
	vector<vector<DENSITYGRID>> density (width + WINDOW_MOVING_STEP - 1, vector<DENSITYGRID> (height + WINDOW_MOVING_STEP - 1));
	int xDensity = 0, yDensity = 0;
	for(auto & column : gridInfo)
	{
		bool xIncrease = false;
		unsigned leftSmallWindowDensity = 0, rightSmallWindowDensity = 0;
		for(auto & nowGrid : column)
		{
			if(nowGrid.type == GRIDTYPE::Conductor)
			{
				if(nowGrid.conductorID.size() == 1)
				{
					const CONDUCTOR & nowConductor = conductorInfo[nowGrid.conductorID[0]];
					if(nowGrid.xDensitySeperate && nowGrid.yDensitySeperate)
					{
						nowGrid.density[0][0] = max<int>(min<int>(nowGrid.xSeperate, nowConductor.right) - max<int>(nowGrid.x, nowConductor.left), 0) *
												max<int>(min<int>(nowGrid.ySeperate, nowConductor.top) - max<int>(nowGrid.y, nowConductor.bottom), 0);
						nowGrid.density[0][1] = max<int>(min<int>(nowGrid.xSeperate, nowConductor.right) - max<int>(nowGrid.x, nowConductor.left), 0) *
												max<int>(min<int>(nowGrid.y + layer.gridSize(), nowConductor.top) - max<int>(nowGrid.ySeperate, nowConductor.bottom), 0);
						nowGrid.density[1][0] = max<int>(min<int>(nowGrid.x + layer.gridSize(), nowConductor.right) - max<int>(nowGrid.xSeperate, nowConductor.left), 0) *
												max<int>(min<int>(nowGrid.ySeperate, nowConductor.top) - max<int>(nowGrid.y, nowConductor.bottom), 0);
						nowGrid.density[1][1] = max<int>(min<int>(nowGrid.x + layer.gridSize(), nowConductor.right) - max<int>(nowGrid.xSeperate, nowConductor.left), 0) *
												max<int>(min<int>(nowGrid.y + layer.gridSize(), nowConductor.top) - max<int>(nowGrid.ySeperate, nowConductor.bottom), 0);
						
					}
					else if(nowGrid.xDensitySeperate)
					{
						nowGrid.density[0][0] = max<int>(min<int>(nowGrid.xSeperate, nowConductor.right) - max<int>(nowGrid.x, nowConductor.left), 0) *
												max<int>(min<int>(nowGrid.y + layer.gridSize(), nowConductor.top) - max<int>(nowGrid.y, nowConductor.bottom), 0);
						nowGrid.density[1][0] = max<int>(min<int>(nowGrid.x + layer.gridSize(), nowConductor.right) - max<int>(nowGrid.xSeperate, nowConductor.left), 0) *
												max<int>(min<int>(nowGrid.y + layer.gridSize(), nowConductor.top) - max<int>(nowGrid.y, nowConductor.bottom), 0);
					}
					else if(nowGrid.yDensitySeperate)
					{
						nowGrid.density[0][0] = max<int>(min<int>(nowGrid.x + layer.gridSize(), nowConductor.right) - max<int>(nowGrid.x, nowConductor.left), 0) *
												max<int>(min<int>(nowGrid.ySeperate, nowConductor.top) - max<int>(nowGrid.y, nowConductor.bottom), 0);
						nowGrid.density[0][1] = max<int>(min<int>(nowGrid.x + layer.gridSize(), nowConductor.right) - max<int>(nowGrid.x, nowConductor.left), 0) *
												max<int>(min<int>(nowGrid.y + layer.gridSize(), nowConductor.top) - max<int>(nowGrid.ySeperate, nowConductor.bottom), 0);
					}
					else
					{
						nowGrid.density[0][0] = max<int>(min<int>(nowGrid.x + layer.gridSize(), nowConductor.right) - max<int>(nowGrid.x, nowConductor.left), 0) *
												max<int>(min<int>(nowGrid.y + layer.gridSize(), nowConductor.top) - max<int>(nowGrid.y, nowConductor.bottom), 0);
					}
				}
				else
				{
					vector<vector<bool>> gridDensity (layer.gridSize(), vector<bool> (layer.gridSize(), false));
					for(const auto & id : nowGrid.conductorID)
					{

						const CONDUCTOR & nowConductor = conductorInfo[id];
						for(int x = max<int>(nowConductor.left - nowGrid.x, 0); x < min<int>(nowConductor.right - nowGrid.x, layer.gridSize()); x++)
						{
							for(int y = max<int>(nowConductor.bottom - nowGrid.y, 0); y < min<int>(nowConductor.top - nowGrid.y, layer.gridSize()); y++)
							{
								if(gridDensity[x][y] == false)
								{
									int xIdx = 0, yIdx = 0;
									if(nowGrid.xDensitySeperate && x + nowGrid.x >= nowGrid.xSeperate)
										xIdx = 1;
									if(nowGrid.yDensitySeperate && y + nowGrid.y >= nowGrid.ySeperate)
										yIdx = 1;
									nowGrid.density[xIdx][yIdx]++;

									gridDensity[x][y] = true;
								}
							}
						}
					}
				}
			}
			else if(nowGrid.type == GRIDTYPE::Dummy)
			{
				if(nowGrid.xDensitySeperate && nowGrid.yDensitySeperate)
				{
					nowGrid.density[0][0] = (nowGrid.xSeperate - nowGrid.x) * (nowGrid.ySeperate - nowGrid.y);
					nowGrid.density[0][1] = (nowGrid.xSeperate - nowGrid.x) * (min<int>(nowGrid.y + layer.gridSize(), yMax) - nowGrid.ySeperate);
					nowGrid.density[1][0] = (min<int>(nowGrid.x + layer.gridSize(), xMax) - nowGrid.xSeperate) * (nowGrid.ySeperate - nowGrid.y);
					nowGrid.density[1][1] = (min<int>(nowGrid.x + layer.gridSize(), xMax) - nowGrid.xSeperate) * (min<int>(nowGrid.y + layer.gridSize(), yMax) - nowGrid.ySeperate);
				}
				else if(nowGrid.xDensitySeperate)
				{
					nowGrid.density[0][0] = (nowGrid.xSeperate - nowGrid.x) * (min<int>(nowGrid.y + layer.gridSize(), yMax) - nowGrid.y);
					nowGrid.density[1][0] = (min<int>(nowGrid.x + layer.gridSize(), xMax) - nowGrid.xSeperate) * (min<int>(nowGrid.y + layer.gridSize(), yMax) - nowGrid.y);
				}
				else if(nowGrid.yDensitySeperate)
				{
					nowGrid.density[0][0] = (min<int>(nowGrid.x + layer.gridSize(), xMax) - nowGrid.x) * (nowGrid.ySeperate - nowGrid.y);
					nowGrid.density[0][1] = (min<int>(nowGrid.x + layer.gridSize(), xMax) - nowGrid.x) * (min<int>(nowGrid.y + layer.gridSize(), yMax) - nowGrid.ySeperate);
				}
				else
					nowGrid.density[0][0] = (min<int>(nowGrid.x + layer.gridSize(), xMax) - nowGrid.x) * (min<int>(nowGrid.y + layer.gridSize(), yMax) - nowGrid.y);
			}
			else if(nowGrid.type == GRIDTYPE::Reserved)
			{
				const int id = nowGrid.dummyID.front();
				density[xDensity][yDensity].criticalDummyID.emplace(id);
				if(nowGrid.xDensitySeperate && dummyInfo[id].right > nowGrid.xSeperate)
					density[min<int>(xDensity + 1, width + WINDOW_MOVING_STEP - 2)][yDensity].criticalDummyID.emplace(id);
				if(nowGrid.yDensitySeperate && dummyInfo[id].top > nowGrid.ySeperate)
					density[xDensity][min<int>(yDensity + 1, height + WINDOW_MOVING_STEP - 2)].criticalDummyID.emplace(id);
				if(nowGrid.xDensitySeperate && dummyInfo[id].right > nowGrid.xSeperate &&
				   nowGrid.yDensitySeperate && dummyInfo[id].top > nowGrid.ySeperate)
					density[min<int>(xDensity + 1, width + WINDOW_MOVING_STEP - 2)][min<int>(yDensity + 1, height + WINDOW_MOVING_STEP - 2)].criticalDummyID.emplace(id);
			}

			leftSmallWindowDensity += nowGrid.density[0][0];
			if(nowGrid.xDensitySeperate)
			{
				xIncrease = true;
				rightSmallWindowDensity += nowGrid.density[1][0];
			}
			if(nowGrid.yDensitySeperate)
			{
				density[xDensity][yDensity].original += leftSmallWindowDensity;
				if(nowGrid.xDensitySeperate && rightSmallWindowDensity > 0)
					density[xDensity + 1][yDensity].original += rightSmallWindowDensity;

				yDensity++;
				leftSmallWindowDensity = nowGrid.density[0][1];
				rightSmallWindowDensity = max<int>(nowGrid.density[1][1], 0);
			}
		}
		if(xIncrease)
			xDensity++;
		yDensity = 0;
	}

	for(int x = width + WINDOW_MOVING_STEP - 2; x >= 0; x--)
	{
		const int right = (x + WINDOW_MOVING_STEP < width + WINDOW_MOVING_STEP - 1) ? x + WINDOW_MOVING_STEP : -1;
		const bool nextR = (x + 1 < width + WINDOW_MOVING_STEP - 1);
		for(int y = height + WINDOW_MOVING_STEP - 2; y >= 0; y--)
		{
			const int top = (y + WINDOW_MOVING_STEP < height + WINDOW_MOVING_STEP - 1) ? y + WINDOW_MOVING_STEP : -1;
			const bool nextT = (y + 1 < height + WINDOW_MOVING_STEP - 1);

			density[x][y].window = density[x][y].original;
			if(nextR && nextT)
				density[x][y].window += (density[x + 1][y].window + density[x][y + 1].window - density[x + 1][y + 1].window);
			else if(nextR)
				density[x][y].window += density[x + 1][y].window;
			else if(nextT)
				density[x][y].window += density[x][y + 1].window;

			if(right != -1 && top != -1)
				density[x][y].window -= (density[right][y].original + density[x][top].original - density[right][top].original);
			else if(right != -1)
				density[x][y].window -= density[right][y].original;
			else if(top != -1)
				density[x][y].window -= density[x][top].original;
		}
	}

	unordered_map<int, int> criticalNeeded;
	vector<array<int, 2>> sortedCriticalNeeded;

	auto C2ID = [&](array<int, 2> xy)
	{
		if(layer.direction == DIRECTION::Horizontal)
			return xy[0] + xy[1] * (width + WINDOW_MOVING_STEP - 1);
		else if(layer.direction == DIRECTION::Vertical)
			return xy[0] * (height + WINDOW_MOVING_STEP - 1) + xy[1];
	};

	auto ID2C = [&](int id)
	{
		if(layer.direction == DIRECTION::Horizontal)
			return array<int, 2> {id % (width + WINDOW_MOVING_STEP - 1), id / (width + WINDOW_MOVING_STEP - 1)};
		else if(layer.direction == DIRECTION::Vertical)
			return array<int, 2> {id / (height + WINDOW_MOVING_STEP - 1), id % (height + WINDOW_MOVING_STEP - 1)};
	};

	auto XY2ID = [&](int x, int y)
	{
		if(layer.direction == DIRECTION::Horizontal)
			return x + y * (width + WINDOW_MOVING_STEP - 1);
		else if(layer.direction == DIRECTION::Vertical)
			return x * (height + WINDOW_MOVING_STEP - 1) + y;
	};

	auto sortingCriticalNeeded = [&](array<int, 2> a, array<int, 2> b) { return criticalNeeded[C2ID(a)] < criticalNeeded[C2ID(b)]; };

	for(int x = 0; x < width; x++)
	{
		for(int y = 0; y < height; y++)
		{
			if(density[x][y].window < layer.minDensity * window * window)
			{
				
				for(int xMove = 0; xMove < WINDOW_MOVING_STEP; xMove++)
				{
					for(int yMove = 0; yMove < WINDOW_MOVING_STEP; yMove++)
					{
						const int id = XY2ID(x + xMove, y + yMove);
						if(density[x + xMove][y + yMove].criticalDummyID.empty())
							continue;
						
						if(criticalNeeded.find(id) == criticalNeeded.end())
						{
							criticalNeeded[id] = 1;
							sortedCriticalNeeded.emplace_back(array<int, 2> {x + xMove, y + yMove});
						}
						else
							criticalNeeded[id]++;
					}
				}
			}
		}
	}
	if(criticalNeeded.empty())
		goto nonCritical;
	else
		sort(sortedCriticalNeeded.begin(), sortedCriticalNeeded.end(), sortingCriticalNeeded);

	while(!sortedCriticalNeeded.empty())
	{
		array<int, 2> nowDensity = sortedCriticalNeeded.back();
		if(criticalNeeded[C2ID(nowDensity)] == 0 || density[nowDensity[0]][nowDensity[1]].criticalDummyID.empty())
		{
			sortedCriticalNeeded.pop_back();
			continue;
		}
		int largestID = -1, largestArea = -1;
		for(const auto & id : density[nowDensity[0]][nowDensity[1]].criticalDummyID)
		{
			if(largestID == -1)
			{
				largestID = id;
				largestArea = dummyInfo[id].area();
			}
			else
			{
				if(largestArea < dummyInfo[id].area())
				{
					largestID = id;
					largestArea = dummyInfo[id].area();
				}
			}
		}

		DUMMY & nowDummy = dummyInfo[largestID];
		nowDummy.inserted = true;
		const int left = (nowDummy.left - xMin) / (window / WINDOW_MOVING_STEP);
		const int right = (nowDummy.right - 1 - xMin) / (window / WINDOW_MOVING_STEP);
		const int bottom = (nowDummy.bottom - yMin) / (window / WINDOW_MOVING_STEP);
		const int top = (nowDummy.top - 1 - yMin) / (window / WINDOW_MOVING_STEP);
		for(int X = left; X <= right; X++)
		{
			const int XMin = X * (window / WINDOW_MOVING_STEP) + xMin;
			const int XMax = XMin + (window / WINDOW_MOVING_STEP);
			for(int Y = bottom; Y <= top; Y++)
			{
				density[X][Y].criticalDummyID.erase(largestID);
				const int YMin = Y * (window / WINDOW_MOVING_STEP) + yMin;
				const int YMax = YMin + (window / WINDOW_MOVING_STEP);
				const int area = (min<int>(nowDummy.right, XMax) - max<int>(nowDummy.left, XMin)) * (min<int>(nowDummy.top, YMax) - max<int>(nowDummy.bottom, YMin));
				for(int x = max<int>(X - WINDOW_MOVING_STEP + 1, 0); x <= min<int>(X, width - 1); x++)
				{
					for(int y = max<int>(Y - WINDOW_MOVING_STEP + 1, 0); y <= min<int>(Y, height - 1); y++)
					{
						density[x][y].window += area;
						if(density[x][y].window >= layer.minDensity * window * window && (density[x][y].window - area) < layer.minDensity * window * window)
						{
							for(int xMove = 0; xMove < WINDOW_MOVING_STEP; xMove++)
							{
								for(int yMove = 0; yMove < WINDOW_MOVING_STEP; yMove++)
								{
									if(criticalNeeded.find(XY2ID(x + xMove, y + yMove)) != criticalNeeded.end())
									{
										criticalNeeded[XY2ID(x + xMove, y + yMove)]--;
									}
								}
							}
						}
					}
				}
			}
		}
		if(criticalNeeded[C2ID(nowDensity)] == 0)
			sortedCriticalNeeded.pop_back();
		sort(sortedCriticalNeeded.begin(), sortedCriticalNeeded.end(), sortingCriticalNeeded);
	}

nonCritical:

	set<int> dummyNeeded;
	for(int x = 0; x < width; x++)
	{
		for(int y = 0; y < height; y++)
		{
			if(density[x][y].window < layer.minDensity * window * window)
			{
				
				for(int xMove = 0; xMove < WINDOW_MOVING_STEP; xMove++)
				{
					for(int yMove = 0; yMove < WINDOW_MOVING_STEP; yMove++)
						dummyNeeded.emplace(XY2ID(x + xMove, y + yMove));
				}
			}
		}
	}
	vector<array<int, 4>> regions;
	while(!dummyNeeded.empty())
	{
		auto iter = dummyNeeded.begin();
		array<int, 4> region { ID2C(*iter)[0], ID2C(*iter)[1], ID2C(*iter)[0], ID2C(*iter)[1] };
		iter = dummyNeeded.erase(iter);
		bool cont = false;
		while(true && iter != dummyNeeded.end())
		{
			const array<int, 2> xy = ID2C(*iter);
			if(xy[1] >= region[1] && xy[1] <= region[3] && xy[0] >= region[0] && xy[0] <= region[2])
			{
				iter = dummyNeeded.erase(iter);
			}
			else if(xy[1] >= region[1] && xy[1] <= region[3])
			{
				if(xy[0] == region[0] - 1)
				{
					cont = true;
					iter = dummyNeeded.erase(iter);
					region[0] = xy[0];
				}
				else if(xy[0] == region[2] + 1)
				{
					cont = true;
					iter = dummyNeeded.erase(iter);
					region[2] = xy[0];
				}
				else
					iter++;
			}
			else if(xy[0] >= region[0] && xy[0] <= region[2])
			{
				if(xy[1] == region[1] - 1)
				{
					cont = true;
					iter = dummyNeeded.erase(iter);
					region[1] = xy[1];
				}
				else if(xy[1] == region[3] + 1)
				{
					cont = true;
					iter = dummyNeeded.erase(iter);
					region[3] = xy[1];
				}
				else
					iter++;
			}
			else
				iter++;
			if(iter == dummyNeeded.end())
			{
				if(cont)
				{
					iter = dummyNeeded.begin();
					cont = false;
				}
				else
				{
					break;
				}
			}
		}
		regions.emplace_back(region);
	}

	
	auto distance = [&](array<int, 2> a, array<int, 2> b)
	{
		if(layer.direction == DIRECTION::Horizontal)
		{
			if(a[1] == 0 && b[1] == 0)
				return conductorInfo[b[0]].bottom - conductorInfo[a[0]].top;
			else if(a[1] == 0 && b[1] == 1)
				return dummyInfo[b[0]].bottom - conductorInfo[a[0]].top;
			else if(a[1] == 1 && b[1] == 0)
				return conductorInfo[b[0]].bottom - dummyInfo[a[0]].top;
			else if(a[1] == 1 && b[1] == 1)
				return dummyInfo[b[0]].bottom - dummyInfo[a[0]].top;
		}
		else if(layer.direction == DIRECTION::Vertical)
		{
			if(a[1] == 0 && b[1] == 0)
				return conductorInfo[b[0]].left - conductorInfo[a[0]].right;
			else if(a[1] == 0 && b[1] == 1)
				return dummyInfo[b[0]].left - conductorInfo[a[0]].right;
			else if(a[1] == 1 && b[1] == 0)
				return conductorInfo[b[0]].left - dummyInfo[a[0]].right;
			else if(a[1] == 1 && b[1] == 1)
				return dummyInfo[b[0]].left - dummyInfo[a[0]].right;
		}
	};

	for(const auto & region : regions)
	{
		const int eLeft = (max<int>(region[0] - 1, 0) * (window / WINDOW_MOVING_STEP)) / layer.gridSize();
		const int eBottom = (max<int>(region[1] - 1, 0) * (window / WINDOW_MOVING_STEP)) / layer.gridSize();
		const int eRight = (min<int>(region[2] + 2, width + WINDOW_MOVING_STEP - 1) * (window / WINDOW_MOVING_STEP)) / layer.gridSize();
		const int eTop = (min<int>(region[3] + 2, height + WINDOW_MOVING_STEP - 1) * (window / WINDOW_MOVING_STEP)) / layer.gridSize();
		unordered_set<int> conductorID, dummyID;
		unordered_map<int, CONDUCTOR> modifiedConductorInfo;
		unordered_map<int, DUMMY> modifiedDummyInfo;
		auto sortingID = [&](array<int, 2> a, array<int, 2> b)
		{
			if(layer.direction == DIRECTION::Horizontal)
			{
				if(a[1] == 0 && b[1] == 0)
				{
					if(modifiedConductorInfo[a[0]].bottom == modifiedConductorInfo[b[0]].bottom)
						return modifiedConductorInfo[a[0]].left < modifiedConductorInfo[b[0]].left;
					else
						return modifiedConductorInfo[a[0]].bottom < modifiedConductorInfo[b[0]].bottom;
				}
				else if(a[1] == 0 && b[1] == 1)
				{
					if(modifiedConductorInfo[a[0]].bottom == modifiedDummyInfo[b[0]].bottom)
						return modifiedConductorInfo[a[0]].left < modifiedDummyInfo[b[0]].left;
					else
						return modifiedConductorInfo[a[0]].bottom < modifiedDummyInfo[b[0]].bottom;
				}
				else if(a[1] == 1 && b[1] == 0)
				{
					if(modifiedDummyInfo[a[0]].bottom == modifiedConductorInfo[b[0]].bottom)
						return modifiedDummyInfo[a[0]].left < modifiedConductorInfo[b[0]].left;
					else
						return modifiedDummyInfo[a[0]].bottom < modifiedConductorInfo[b[0]].bottom;
				}
				else if(a[1] == 1 && b[1] == 1)
				{
					if(modifiedDummyInfo[a[0]].bottom == modifiedDummyInfo[b[0]].bottom)
						return modifiedDummyInfo[a[0]].left < modifiedDummyInfo[b[0]].left;
					else
						return modifiedDummyInfo[a[0]].bottom < modifiedDummyInfo[b[0]].bottom;
				}
			}
			else if(layer.direction == DIRECTION::Vertical)
			{
				if(a[1] == 0 && b[1] == 0)
				{
					if(modifiedConductorInfo[a[0]].left == modifiedConductorInfo[b[0]].left)
						return modifiedConductorInfo[a[0]].bottom < modifiedConductorInfo[b[0]].bottom;
					else
						return modifiedConductorInfo[a[0]].left < modifiedConductorInfo[b[0]].left;
				}
				else if(a[1] == 0 && b[1] == 1)
				{
					if(modifiedConductorInfo[a[0]].left == modifiedDummyInfo[b[0]].left)
						return modifiedConductorInfo[a[0]].bottom < modifiedDummyInfo[b[0]].bottom;
					else
						return modifiedConductorInfo[a[0]].left < modifiedDummyInfo[b[0]].left;
				}
				else if(a[1] == 1 && b[1] == 0)
				{
					if(modifiedDummyInfo[a[0]].left == modifiedConductorInfo[b[0]].left)
						return modifiedDummyInfo[a[0]].bottom < modifiedConductorInfo[b[0]].bottom;
					else
						return modifiedDummyInfo[a[0]].left < modifiedConductorInfo[b[0]].left;
				}
				else if(a[1] == 1 && b[1] == 1)
				{
					if(modifiedDummyInfo[a[0]].left == modifiedDummyInfo[b[0]].left)
						return modifiedDummyInfo[a[0]].bottom < modifiedDummyInfo[b[0]].bottom;
					else
						return modifiedDummyInfo[a[0]].left < modifiedDummyInfo[b[0]].left;
				}
			}
		};
		for(int x = eLeft; x < eRight; x++)
		{
			for(int y = eBottom; y < eTop; y++)
			{
				for(const auto & id : gridInfo[x][y].conductorID)
				{
					if(modifiedConductorInfo.find(id) == modifiedConductorInfo.end())
					{
						conductorID.emplace(id);
						modifiedConductorInfo[id] = conductorInfo[id];
						modifiedConductorInfo[id].left = max<int>(conductorInfo[id].left, eLeft * layer.gridSize() + xMin);
						modifiedConductorInfo[id].bottom = max<int>(conductorInfo[id].bottom, eBottom * layer.gridSize() + yMin);
						modifiedConductorInfo[id].right = min<int>(conductorInfo[id].right, eRight * layer.gridSize() + xMin);
						modifiedConductorInfo[id].top = min<int>(conductorInfo[id].top, eTop * layer.gridSize() + yMin);
					}
				}
				for(const auto & id : gridInfo[x][y].dummyID)
				{
					if(modifiedDummyInfo.find(id) == modifiedDummyInfo.end() && dummyInfo[id].inserted)
					{
						dummyID.emplace(id);
						modifiedDummyInfo[id] = dummyInfo[id];
						modifiedDummyInfo[id].left = max<int>(dummyInfo[id].left, eLeft * layer.gridSize() + xMin);
						modifiedDummyInfo[id].bottom = max<int>(dummyInfo[id].bottom, eBottom * layer.gridSize() + yMin);
						modifiedDummyInfo[id].right = min<int>(dummyInfo[id].right, eRight * layer.gridSize() + xMin);
						modifiedDummyInfo[id].top = min<int>(dummyInfo[id].top, eTop * layer.gridSize() + yMin);
					}
				}
			}
		}
		vector<array<int, 2>> sortedID;
		sortedID.reserve(modifiedConductorInfo.size() + modifiedDummyInfo.size());
		for(const auto & id : conductorID)
			sortedID.emplace_back(array<int, 2> {id, 0});
		for(const auto & id : dummyID)
			sortedID.emplace_back(array<int, 2> {id, 1});
		sort(sortedID.begin(), sortedID.end(), sortingID);

		if(layer.direction == DIRECTION::Horizontal)
		{
			array<int, 2> nowCombine = sortedID.front();
			for(auto nowCheck = sortedID.begin() + 1; nowCheck != sortedID.end(); nowCheck++)
			{
				if(nowCombine[1] == (*nowCheck)[1])
				{
					if(nowCombine[1] == 0 &&
					   modifiedConductorInfo[nowCombine[0]].bottom == modifiedConductorInfo[(*nowCheck)[0]].bottom &&
					   modifiedConductorInfo[nowCombine[0]].top == modifiedConductorInfo[(*nowCheck)[0]].top &&
					   modifiedConductorInfo[(*nowCheck)[0]].left - modifiedConductorInfo[nowCombine[0]].right < 3 * layer.gridSize())
					{
						modifiedConductorInfo[nowCombine[0]].right = modifiedConductorInfo[(*nowCheck)[0]].right;
						(*nowCheck)[0] = -1;
					}
					else if(nowCombine[1] == 1 &&
					    	modifiedDummyInfo[nowCombine[0]].bottom == modifiedDummyInfo[(*nowCheck)[0]].bottom &&
					    	modifiedDummyInfo[nowCombine[0]].top == modifiedDummyInfo[(*nowCheck)[0]].top &&
					    	modifiedDummyInfo[(*nowCheck)[0]].left - modifiedDummyInfo[nowCombine[0]].right < 3 * layer.gridSize())
					{
						modifiedDummyInfo[nowCombine[0]].right = modifiedDummyInfo[(*nowCheck)[0]].right;
						(*nowCheck)[0] = -1;
					}
					else
						nowCombine = *nowCheck;
				}
				else
					nowCombine = *nowCheck;
			}
			for(auto iter1 = sortedID.begin(); iter1 != sortedID.end(); iter1++)
			{
				if((*iter1)[0] == -1)
					continue;
				int bias;
				vector<bool> boundary;
				int total = 0;
				if((*iter1)[1] == 0)
				{
					bias = modifiedConductorInfo[(*iter1)[0]].left + layer.gridSize();
					int len = modifiedConductorInfo[(*iter1)[0]].right - layer.gridSize() - bias;
					if(len < 0)
						continue;
					boundary.resize(len, false);
				}
				else
				{
					bias = modifiedDummyInfo[(*iter1)[0]].left + layer.gridSize();
					int len = modifiedDummyInfo[(*iter1)[0]].right - layer.gridSize() - bias;
					if(len < 0)
						continue;
					boundary.resize(len, false);
				}
				for(auto iter2 = iter1 + 1; iter2 != sortedID.end(); iter2++)
				{
					if((*iter2)[0] == -1)
						continue;
					int dist = distance(*iter1, *iter2), count = 0, xStart = -1;
					bool overlap = false;
					if(dist >= 0)
					{
						if((*iter2)[1] == 0)
						{
							for(int x = max<int>(modifiedConductorInfo[(*iter2)[0]].left - layer.gridSize() - bias, 0); x < min<int>(modifiedConductorInfo[(*iter2)[0]].right + layer.gridSize() - bias, boundary.size()); x++)
							{
								overlap = true;
								if(boundary[x] == false)
								{
									if(xStart < 0)
										xStart = x + bias;
									boundary[x] = true;
									count++;
								}
							}
						}
						else if((*iter2)[1] == 1)
						{
							for(int x = max<int>(modifiedDummyInfo[(*iter2)[0]].left - layer.gridSize() - bias, 0); x < min<int>(modifiedDummyInfo[(*iter2)[0]].right + layer.gridSize() - bias, boundary.size()); x++)
							{
								overlap = true;
								if(boundary[x] == false)
								{
									if(xStart < 0)
										xStart = x + bias;
									boundary[x] = true;
									count++;
								}
							}
						}
					}
					else
						continue;

					total += count;
					if(dist < 3 * layer.gridSize() || count < 3 * layer.gridSize() || !overlap)
					{
						if(boundary.size() - total < 3 * layer.gridSize())
							break;
						continue;
					}
					auto insert = [&](int yStart, int yEnd)
					{
						while(true)
						{
							int width = layer.maxWidth;
							if(width > count)
								width = count;
							count -= (width + layer.gridSize());

							DUMMY newDummy = {.inserted = true,
											.dummyID = int(dummyInfo.size()),
											.left = xStart,
											.bottom = yStart,
											.right = xStart + width,
											.top = yEnd,
											.layerID = layer.layerID};
							dummyInfo.emplace_back(newDummy);
							

							xStart += (width + layer.gridSize());

							if(count < layer.gridSize())
								break;
						}
					};

					if((*iter1)[1] == 0 && (*iter2)[1] == 0)
					{
						int yStart = modifiedConductorInfo[(*iter1)[0]].top + layer.gridSize();
						int yEnd = modifiedConductorInfo[(*iter2)[0]].bottom - layer.gridSize();
						insert(yStart, yEnd);
					}
					else if((*iter1)[1] == 0 && (*iter2)[1] == 1)
					{
						int yStart = modifiedConductorInfo[(*iter1)[0]].top + layer.gridSize();
						int yEnd = modifiedDummyInfo[(*iter2)[0]].bottom - layer.gridSize();
						insert(yStart, yEnd);
					}
					else if((*iter1)[1] == 1 && (*iter2)[1] == 0)
					{
						int yStart = modifiedDummyInfo[(*iter1)[0]].top + layer.gridSize();
						int yEnd = modifiedConductorInfo[(*iter2)[0]].bottom - layer.gridSize();
						insert(yStart, yEnd);
					}
					else if((*iter1)[1] == 1 && (*iter2)[1] == 1)
					{
						int yStart = modifiedDummyInfo[(*iter1)[0]].top + layer.gridSize();
						int yEnd = modifiedDummyInfo[(*iter2)[0]].bottom - layer.gridSize();
						insert(yStart, yEnd);
					}
				}
			}
		}
		else if(layer.direction == DIRECTION::Vertical)
		{
			array<int, 2> nowCombine = sortedID.front();
			for(auto nowCheck = sortedID.begin() + 1; nowCheck != sortedID.end(); nowCheck++)
			{
				if(nowCombine[1] == (*nowCheck)[1])
				{
					if(nowCombine[1] == 0 &&
					   modifiedConductorInfo[nowCombine[0]].left == modifiedConductorInfo[(*nowCheck)[0]].left &&
					   modifiedConductorInfo[nowCombine[0]].right == modifiedConductorInfo[(*nowCheck)[0]].right &&
					   modifiedConductorInfo[(*nowCheck)[0]].bottom - modifiedConductorInfo[nowCombine[0]].top < 3 * layer.gridSize())
					{
						modifiedConductorInfo[nowCombine[0]].top = modifiedConductorInfo[(*nowCheck)[0]].top;
						(*nowCheck)[0] = -1;
					}
					else if(nowCombine[1] == 1 &&
					    	modifiedDummyInfo[nowCombine[0]].left == modifiedDummyInfo[(*nowCheck)[0]].left &&
					    	modifiedDummyInfo[nowCombine[0]].right == modifiedDummyInfo[(*nowCheck)[0]].right &&
					    	modifiedDummyInfo[(*nowCheck)[0]].bottom - modifiedDummyInfo[nowCombine[0]].top < 3 * layer.gridSize())
					{
						modifiedDummyInfo[nowCombine[0]].top = modifiedDummyInfo[(*nowCheck)[0]].top;
						(*nowCheck)[0] = -1;
					}
					else
						nowCombine = *nowCheck;
				}
				else
					nowCombine = *nowCheck;
			}
			for(auto iter1 = sortedID.begin(); iter1 != sortedID.end(); iter1++)
			{
				if((*iter1)[0] == -1)
					continue;
				int bias;
				vector<bool> boundary;
				int total = 0;
				if((*iter1)[1] == 0)
				{
					bias = modifiedConductorInfo[(*iter1)[0]].bottom + layer.gridSize();
					int len = modifiedConductorInfo[(*iter1)[0]].top - layer.gridSize() - bias;
					if(len < 0)
						continue;
					boundary.resize(len, false);
				}
				else
				{
					bias = modifiedDummyInfo[(*iter1)[0]].bottom + layer.gridSize();
					int len = modifiedDummyInfo[(*iter1)[0]].top - layer.gridSize() - bias;
					if(len < 0)
						continue;
					boundary.resize(len, false);
				}
				for(auto iter2 = iter1 + 1; iter2 != sortedID.end(); iter2++)
				{
					if((*iter2)[0] == -1)
						continue;
					int dist = distance(*iter1, *iter2), count = 0, yStart = -1;
					bool overlap = false;
					if(dist >= 0)
					{
						if((*iter2)[1] == 0)
						{
							for(int y = max<int>(modifiedConductorInfo[(*iter2)[0]].bottom - layer.gridSize() - bias, 0); y < min<int>(modifiedConductorInfo[(*iter2)[0]].top + layer.gridSize() - bias, boundary.size()); y++)
							{
								overlap = true;
								if(boundary[y] == false)
								{
									if(yStart < 0)
										yStart = y + bias;
									boundary[y] = true;
									count++;
								}
							}
						}
						else if((*iter2)[1] == 1)
						{
							for(int y = max<int>(modifiedDummyInfo[(*iter2)[0]].bottom - layer.gridSize() - bias, 0); y < min<int>(modifiedDummyInfo[(*iter2)[0]].top + layer.gridSize() - bias, boundary.size()); y++)
							{
								overlap = true;
								if(boundary[y] == false)
								{
									if(yStart < 0)
										yStart = y + bias;
									boundary[y] = true;
									count++;
								}
							}
						}
					}
					else
						continue;
					
					total += count;
					if(dist < 3 * layer.gridSize() || count < 3 * layer.gridSize() || !overlap)
					{
						if(boundary.size() - total < 3 * layer.gridSize())
							break;
						continue;
					}

					auto insert = [&](int xStart, int xEnd)
					{
						while(true)
						{
							int height = layer.maxWidth;
							if(height > count)
								height = count;
							count -= (height + layer.gridSize());

							DUMMY newDummy = {.inserted = true,
											  .dummyID = int(dummyInfo.size()),
											  .left = xStart,
											  .bottom = yStart,
											  .right = xEnd,
											  .top = yStart + height,
											  .layerID = layer.layerID};
							dummyInfo.emplace_back(newDummy);
							yStart += (height + layer.gridSize());

							if(count < layer.gridSize())
								break;
						}
					};

					if((*iter1)[1] == 0 && (*iter2)[1] == 0)
					{
						int xStart = modifiedConductorInfo[(*iter1)[0]].right + layer.gridSize();
						int xEnd = modifiedConductorInfo[(*iter2)[0]].left - layer.gridSize();
						insert(xStart, xEnd);
					}
					else if((*iter1)[1] == 0 && (*iter2)[1] == 1)
					{
						int xStart = modifiedConductorInfo[(*iter1)[0]].right + layer.gridSize();
						int xEnd = modifiedDummyInfo[(*iter2)[0]].left - layer.gridSize();
						insert(xStart, xEnd);
					}
					else if((*iter1)[1] == 1 && (*iter2)[1] == 0)
					{
						int xStart = modifiedDummyInfo[(*iter1)[0]].right + layer.gridSize();
						int xEnd = modifiedConductorInfo[(*iter2)[0]].left - layer.gridSize();
						insert(xStart, xEnd);
					}
					else if((*iter1)[1] == 1 && (*iter2)[1] == 1)
					{
						int xStart = modifiedDummyInfo[(*iter1)[0]].right + layer.gridSize();
						int xEnd = modifiedDummyInfo[(*iter2)[0]].left - layer.gridSize();
						insert(xStart, xEnd);
					}
				}
			}
		}
	}
}

void writeFile(const char * file, const vector<vector<DUMMY>> & dummyInfo)
{
	fstream output;
	output.open(file, ios::out);

	for(const auto & dummys : dummyInfo)
	{
		for(const auto & i : dummys)
		{
			if(i.inserted)
				output << i.left << " " << i.bottom << " " << i.right << " " << i.top << " " << i.layerID << endl;
		}
	}

	output.close();
}

int main(int argc, char * argv[])
{
	auto inputStart = chrono::steady_clock::now();

	int xMin, xMax, yMin, yMax, window, numCirtical, numLayer, numConductor;
	CRITICALNET criticalNet;
	vector<LAYER> layerInfo;
	vector<CONDUCTOR> conductorInfo;
	readFile(argv[1],
			 xMin, xMax, yMin, yMax, window, numCirtical, numLayer, numConductor,
			 criticalNet, layerInfo, conductorInfo);

	auto inputEnd = chrono::steady_clock::now();

	vector<vector<DUMMY>> dummyInfo (numLayer + 1);
	for(int i = 1; i <= numLayer; i++)
	{
		cout << "\n[ Layer " << i << " ]" << endl; cout.flush();
		cout << "Grid Creation ..." << endl; cout.flush();
		vector<vector<GRID>> gridInfo = gridCreation(xMin, xMax, yMin, yMax, window,
													 criticalNet, layerInfo[i], conductorInfo);
		cout << "Dummy Fill Insertion ..." << endl; cout.flush();
		dummyInsertion(dummyInfo[i], xMax, yMax, gridInfo, layerInfo[i], conductorInfo);
		cout << "Density Refinement ..." << endl; cout.flush();
		densityRefinement((xMax - xMin) / (window / WINDOW_MOVING_STEP) - WINDOW_MOVING_STEP + 1,
						  (yMax - yMin) / (window / WINDOW_MOVING_STEP) - WINDOW_MOVING_STEP + 1,
						  window, dummyInfo[i], xMin, xMax, yMin, yMax, gridInfo, layerInfo[i], conductorInfo);
	}

	auto outputStart = chrono::steady_clock::now();

	writeFile(argv[2], dummyInfo);

	auto outputEnd = chrono::steady_clock::now();

	cout << "\n   -----   Timing Result   -----   \n"
		 << "  Input Time:\t\t" << chrono::duration<float>(inputEnd - inputStart).count() << "\tsec." << endl
		 << "+ Output Time:\t\t" << chrono::duration<float>(outputEnd - outputStart).count() << "\tsec." << endl
		 << "= Total Runtime:\t" << chrono::duration<float>(outputEnd - inputStart).count() << "\tsec." << endl << endl;
}