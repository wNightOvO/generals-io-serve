/*
	移动操作
*/

#pragma once
#include <include/type.hpp>

namespace gen {
	
struct MoveOperation {
	int playerID = 0;
	Point source;
	int d = 0; // 表示方向 方向定义见 include/generals/direction.h
	bool half; // 是否挪动一半兵力
	
	MoveOperation(int playerID, Point source, int d, bool half): playerID(playerID), source(source), d(d), half(half) {}
};
	
}
