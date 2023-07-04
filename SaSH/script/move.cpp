﻿#include "stdafx.h"
#include "interpreter.h"
#include "util.h"
#include "injector.h"
#include "map/mapanalyzer.h"

//move
int Interpreter::setdir(int currentline, const TokenMap& TK)
{
	Injector& injector = Injector::getInstance();

	if (injector.server.isNull())
		return Parser::kError;

	checkBattleThenWait();

	QString dirStr = "";
	int dir = -1;
	checkInt(TK, 1, &dir);
	checkString(TK, 1, &dirStr);

	dirStr = dirStr.toUpper().simplified();

	if (dir != -1 && dirStr.isEmpty() && dir >= 1 && dir <= 8)
	{
		--dir;
		injector.server->setPlayerFaceDirection(dir);
	}
	else if (dir == -1 && !dirStr.isEmpty())
	{
		injector.server->setPlayerFaceDirection(dirStr);
	}
	else
		return Parser::kArgError;

	return Parser::kNoChange;
}

int Interpreter::move(int currentline, const TokenMap& TK)
{
	Injector& injector = Injector::getInstance();

	if (injector.server.isNull())
		return Parser::kError;

	checkBattleThenWait();

	QPoint p;
	checkInt(TK, 1, &p.rx());
	checkInt(TK, 2, &p.ry());

	int timeout = DEFAULT_FUNCTION_TIMEOUT;
	checkInt(TK, 3, &timeout);

	if (p.x() < 0 || p.x() >= 1500 || p.y() < 0 || p.y() >= 1500)
		return Parser::kArgError;

	injector.server->move(p);
	QThread::msleep(1);

	waitfor(timeout, [&injector, &p]()->bool
		{
			bool result = injector.server->getPoint() == p;
			if (result)
			{
				injector.server->move(p);
			}
			return result;
		});

	return Parser::kNoChange;
}

int Interpreter::fastmove(int currentline, const TokenMap& TK)
{
	Injector& injector = Injector::getInstance();

	if (injector.server.isNull())
		return Parser::kError;

	checkBattleThenWait();

	QPoint p;
	QString dirStr;
	if (checkInt(TK, 1, &p.rx()))
	{
		if (checkInt(TK, 2, &p.ry()))
		{
			if (p.x() < 0 || p.x() >= 1500 || p.y() < 0 || p.y() >= 1500)
				return Parser::kArgError;
		}
		else if (p.x() >= 1 && p.x() <= 8)
		{
			int dir = p.x() - 1;
			p = injector.server->getPoint() + util::fix_point.at(dir) * 10;
		}
		else
			return Parser::kArgError;
	}
	else if (checkString(TK, 1, &dirStr))
	{
		if (!dirMap.contains(dirStr.toUpper().simplified()))
			return Parser::kArgError;

		DirType dir = dirMap.value(dirStr.toUpper().simplified());
		//計算出往該方向10格的坐標
		p = injector.server->getPoint() + util::fix_point.at(dir) * 10;

	}

	injector.server->move(p);
	QThread::msleep(1);

	return Parser::kNoChange;
}

int Interpreter::packetmove(int currentline, const TokenMap& TK)
{
	Injector& injector = Injector::getInstance();

	if (injector.server.isNull())
		return Parser::kError;

	checkBattleThenWait();

	QPoint p;
	checkInt(TK, 1, &p.rx());
	checkInt(TK, 2, &p.ry());
	QString dirStr;
	checkString(TK, 3, &dirStr);

	if (p.x() < 0 || p.x() >= 1500 || p.y() < 0 || p.y() >= 1500)
		return Parser::kArgError;

	injector.server->move(p, dirStr);
	QThread::msleep(1);

	return Parser::kNoChange;
}

int Interpreter::findpath(int currentline, const TokenMap& TK)
{
	Injector& injector = Injector::getInstance();

	if (injector.server.isNull())
		return Parser::kError;

	checkBattleThenWait();
	int steplen = 3;
	QPoint p;
	QString name;
	if (!checkInt(TK, 1, &p.rx()))
	{
		if (!checkString(TK, 1, &name))
			return Parser::kArgError;
	}
	else
		checkInt(TK, 2, &p.ry());

	auto findNpcCallBack = [&injector](const QString& name, QPoint& dst, int* pdir)->bool
	{
		mapunit_s unit;
		if (!injector.server->findUnit(name, util::OBJ_NPC, &unit, ""))
		{
			if (!injector.server->findUnit(name, util::OBJ_HUMAN, &unit, ""))
				return 0;//沒找到
		}

		int dir = injector.server->mapAnalyzer->calcBestFollowPointByDstPoint(injector.server->nowFloor, injector.server->getPoint(), unit.p, &dst, true, unit.dir);
		if (pdir)
			*pdir = dir;
		return dir != -1;//找到了
	};

	int dir = -1;
	if (p.isNull() && !name.isEmpty())
	{
		QString key = QString::number(injector.server->nowFloor);
		util::Config config(util::getPointFileName());
		QList<util::MapData> datas = config.readMapData(key);
		if (datas.isEmpty())
			return Parser::kNoChange;

		for (const util::MapData& d : datas)
		{
			if (d.name.isEmpty())
				return Parser::kNoChange;

			if (d.name == name)
			{
				p = QPoint(d.x, d.y);
				checkInt(TK, 2, &steplen);
				break;
			}
			else if (name.startsWith(kFuzzyPrefix))
			{
				QString newName = name.mid(0);
				if (d.name.contains(newName))
				{
					p = QPoint(d.x, d.y);
					checkInt(TK, 2, &steplen);
					break;
				}
			}
		}

		if (p.isNull())
			return Parser::kNoChange;
	}
	else
		checkInt(TK, 3, &steplen);


	//findpath 不允許接受為0的xy座標
	if (p.x() < 0 || p.x() >= 1500 || p.y() < 0 || p.y() >= 1500)
		return Parser::kArgError;

	if (findPath(p, steplen))
	{
		if (!name.isEmpty() && (findNpcCallBack(name, p, &dir)) && dir != -1)
			injector.server->setPlayerFaceDirection(dir);
	}

	return Parser::kNoChange;
}

int Interpreter::movetonpc(int currentline, const TokenMap& TK)
{
	Injector& injector = Injector::getInstance();

	if (injector.server.isNull())
		return Parser::kError;

	checkBattleThenWait();

	QString cmpNpcName;
	checkString(TK, 1, &cmpNpcName);

	QString cmpFreeName;
	checkString(TK, 2, &cmpFreeName);

	QPoint p;
	checkInt(TK, 3, &p.rx());
	checkInt(TK, 4, &p.ry());

	int timeout = DEFAULT_FUNCTION_TIMEOUT * 36;
	checkInt(TK, 5, &timeout);

	//findpath 不允許接受為0的xy座標
	if (p.x() <= 0 || p.x() >= 1500 || p.y() <= 0 || p.y() >= 1500)
		return Parser::kArgError;

	mapunit_s unit;
	int dir = -1;
	auto findNpcCallBack = [&injector, &unit, cmpNpcName, cmpFreeName, &dir](QPoint& dst)->bool
	{
		if (!injector.server->findUnit(cmpNpcName, util::OBJ_NPC, &unit, cmpFreeName))
		{
			if (!injector.server->findUnit(cmpNpcName, util::OBJ_HUMAN, &unit, cmpFreeName))
				return 0;//沒找到
		}

		dir = injector.server->mapAnalyzer->calcBestFollowPointByDstPoint(injector.server->nowFloor, injector.server->getPoint(), unit.p, &dst, true, unit.dir);
		return dir != -1 ? 1 : 0;//找到了
	};

	bool bret = false;
	if (findPath(p, 1, 0, timeout, findNpcCallBack, true) && dir != -1)
	{
		injector.server->setPlayerFaceDirection(dir);
		bret = true;
	}

	return checkJump(TK, 6, bret, FailedJump);
}

int Interpreter::teleport(int currentline, const TokenMap& TK)
{
	Injector& injector = Injector::getInstance();

	if (injector.server.isNull())
		return Parser::kError;

	checkBattleThenWait();

	injector.server->warp();

	return Parser::kNoChange;
}

int Interpreter::warp(int currentline, const TokenMap& TK)
{
	Injector& injector = Injector::getInstance();

	if (injector.server.isNull())
		return Parser::kError;

	QPoint pfrom;
	checkInt(TK, 1, &pfrom.rx());
	checkInt(TK, 2, &pfrom.ry());
	if (pfrom.isNull() || pfrom.x() < 0 || pfrom.x() > 1500 || pfrom.y() < 0 || pfrom.y() > 1500)
		return Parser::kArgError;

	QPoint pto;
	checkInt(TK, 3, &pto.rx());
	checkInt(TK, 4, &pto.ry());
	if (pto.isNull() || pto.x() < 0 || pto.x() > 1500 || pto.y() < 0 || pto.y() > 1500)
		return Parser::kArgError;

	int floor = 0;
	QString floorName;
	if (!checkInt(TK, 5, &floor))
	{
		if (!checkString(TK, 5, &floorName))
			return Parser::kArgError;
	}

	if (floor == 0 && floorName.isEmpty())
		return Parser::kArgError;

	int timeout = DEFAULT_FUNCTION_TIMEOUT;
	checkInt(TK, 6, &timeout);

	bool bret = false;

	do
	{
		if (!findPath(pfrom, 1, 0, timeout))
			break;

		bret = waitfor(timeout, [&injector, pto, floor, floorName]()->bool
			{
				if (injector.server->getPoint() == pto)
				{
					if (floorName.isEmpty())
					{
						if (injector.server->nowFloor == floor)
							return true;
					}
					else
					{
						if (floorName.startsWith(kFuzzyPrefix))
						{
							QString newFloorName = floorName.mid(1);
							if (injector.server->nowFloorName.contains(newFloorName))
								return true;
						}
						else if (injector.server->nowFloorName == floorName)
							return true;
					}
				}
				return false;
			});

	} while (false);

	return checkJump(TK, 7, bret, FailedJump);
}