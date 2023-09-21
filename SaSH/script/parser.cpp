﻿/*
				GNU GENERAL PUBLIC LICENSE
				   Version 2, June 1991
COPYRIGHT (C) Bestkakkoii 2023 All Rights Reserved.
This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.
This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
GNU General Public License for more details.
You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.

*/

#include "stdafx.h"
#include "parser.h"

#include "signaldispatcher.h"
#include "injector.h"
#include "interpreter.h"


//"調用" 傳參數最小佔位
constexpr qint64 kCallPlaceHoldSize = 2;
//"格式化" 最小佔位
constexpr qint64 kFormatPlaceHoldSize = 3;

void makeTable(sol::state& lua, const char* name, qint64 i, qint64 j)
{
	if (!lua[name].valid())
		lua[name].get_or_create<sol::table>();
	else if (!lua[name].is<sol::table>())
		lua[name] = lua.create_table();

	qint64 k, l;

	for (k = 1; k <= i; ++k)
	{
		if (!lua[name][k].valid())
			lua[name][k].get_or_create<sol::table>();
		else
			lua[name][k] = lua.create_table();

		for (l = 1; l <= j; ++l)
		{
			if (!lua[name][k][l].valid())
				lua[name][k][l].get_or_create<sol::table>();
			else
				lua[name][k][l] = lua.create_table();
		}
	}
}

void makeTable(sol::state& lua, const char* name, qint64 i)
{
	if (!lua[name].valid())
		lua[name].get_or_create<sol::table>();
	else
		lua[name] = lua.create_table();

	qint64 k;
	for (k = 1; k <= i; ++k)
	{
		if (!lua[name][k].valid())
			lua[name][k].get_or_create<sol::table>();
		else
			lua[name][k] = lua.create_table();
	}
}

std::vector<std::string> Unique(const std::vector<std::string>& v)
{
	std::vector<std::string> result = v;
#if _MSVC_LANG > 201703L
	std::ranges::stable_sort(result, std::less<std::string>());
#else
	std::sort(result.begin(), result.end(), std::less<std::string>());
#endif
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

std::vector<qint64> Unique(const std::vector<qint64>& v)
{
	std::vector<qint64> result = v;
#if _MSVC_LANG > 201703L
	std::ranges::stable_sort(result, std::less<qint64>());
#else
	std::sort(result.begin(), result.end(), std::less<qint64>());
#endif
	result.erase(std::unique(result.begin(), result.end()), result.end());
	return result;
}

template<typename T>
std::vector<T> ShiftLeft(const std::vector<T>& v, qint64 i)
{
	std::vector<T> result = v;
#if _MSVC_LANG > 201703L
	std::ranges::shift_left(result, i);
#else
	std::rotate(result.begin(), result.begin() + i, result.end());
#endif
	return result;

}

template<typename T>
std::vector<T> ShiftRight(const std::vector<T>& v, qint64 i)
{
	std::vector<T> result = v;
#if _MSVC_LANG > 201703L
	std::ranges::shift_right(result, i);
#else
	std::rotate(result.begin(), result.end() - i, result.end());
#endif
	return result;
}

template<typename T>
std::vector<T> Shuffle(const std::vector<T>& v)
{
	std::vector<T> result = v;
	std::random_device rd;
	std::mt19937_64 gen(rd());
	//std::default_random_engine eng(rd());
#if _MSVC_LANG > 201703L
	std::ranges::shuffle(result, gen);
#else
	std::shuffle(result.begin(), result.end(), gen);
#endif
	return result;
}

template<typename T>
std::vector<T> Rotate(const std::vector<T>& v, qint64 len)//true = right, false = left
{
	std::vector<T> result = v;
	if (len >= 0)
#if _MSVC_LANG > 201703L
		std::ranges::rotate(result, result.begin() + len);
#else
		std::rotate(result.begin(), result.begin() + len, result.end());
#endif
	else
#if _MSVC_LANG > 201703L
		std::ranges::rotate(result, result.end() + len);
#else
		std::rotate(result.begin(), result.end() + len, result.end());
#endif
	return result;
}

Parser::Parser(qint64 index)
	: ThreadPlugin(index, nullptr)
	, lexer_(index)
	, clua_(index)
{
	qDebug() << "Parser is created!!";
	setIndex(index);
	SignalDispatcher& signalDispatcher = SignalDispatcher::getInstance(index);
	connect(&signalDispatcher, &SignalDispatcher::nodifyAllStop, this, &Parser::requestInterruption, Qt::UniqueConnection);

	clua_.openlibs();

	clua_.setHookForStop(true);

	sol::state& lua_ = clua_.getLua();

	lua_.set_function("input", [this, index](sol::object oargs, sol::this_state s)->sol::object
		{
			SignalDispatcher& signalDispatcher = SignalDispatcher::getInstance(index);

			std::string sargs = "";
			if (oargs.is<std::string>())
				sargs = oargs.as<std::string>();

			QString args = QString::fromUtf8(sargs.c_str());

			QStringList argList = args.split(util::rexOR, Qt::SkipEmptyParts);
			qint64 type = QInputDialog::InputMode::TextInput;
			QString msg;
			QVariant var;
			bool ok = false;
			if (argList.size() > 0)
			{
				type = argList.front().toLongLong(&ok) - 1ll;
				if (type < 0 || type > 2)
					type = QInputDialog::InputMode::TextInput;
			}

			if (argList.size() > 1)
			{
				msg = argList.at(1);
			}

			if (argList.size() > 2)
			{
				if (type == QInputDialog::IntInput)
					var = QVariant(argList.at(2).toLongLong(&ok));
				else if (type == QInputDialog::DoubleInput)
					var = QVariant(argList.at(2).toDouble(&ok));
				else
					var = QVariant(argList.at(2));
			}

			emit signalDispatcher.inputBoxShow(msg, type, &var);

			if (var.toLongLong() == 987654321ll)
			{
				requestInterruption();
				insertGlobalVar("vret", "nil");
				return sol::lua_nil;
			}

			if (type == QInputDialog::IntInput)
			{
				insertGlobalVar("vret", var.toLongLong());
				return sol::make_object(s, var.toLongLong());
			}
			else if (type == QInputDialog::DoubleInput)
			{
				insertGlobalVar("vret", var.toDouble());
				return sol::make_object(s, var.toDouble());
			}
			else
			{
				QString str = var.toString();
				if (str.toLower() == "true" || str.toLower() == "false" || str.toLower() == "真" || str.toLower() == "假")
					return sol::make_object(s, var.toBool());

				insertGlobalVar("vret", var.toString());
				return sol::make_object(s, var.toString().toUtf8().constData());
			}
		});

	lua_.set_function("format", [this](std::string sformat, sol::this_state s)->sol::object
		{
			QString formatStr = QString::fromUtf8(sformat.c_str());
			if (formatStr.isEmpty())
				return sol::lua_nil;

			static const QRegularExpression rexFormat(R"(\{([T|C])?\s*\:([\w\p{Han}]+(\['*"*[\w\p{Han}]+'*"*\])*)\})");
			if (!formatStr.contains(rexFormat))
				return sol::make_object(s, sformat.c_str());

			enum FormatType
			{
				kNothing,
				kTime,
				kConst,
			};

			constexpr qint64 MAX_FORMAT_DEPTH = 10;

			for (qint64 i = 0; i < MAX_FORMAT_DEPTH; ++i)
			{
				QRegularExpressionMatchIterator matchIter = rexFormat.globalMatch(formatStr);
				//Group 1: T or C or nothing
				//Group 2: var or var table

				QMap<QString, QPair<FormatType, QString>> formatVarList;

				for (;;)
				{
					if (!matchIter.hasNext())
						break;

					const QRegularExpressionMatch match = matchIter.next();
					if (!match.hasMatch())
						break;

					QString type = match.captured(1);
					QString var = match.captured(2);
					QVariant varValue;

					if (type == "T")
					{
						varValue = luaDoString(QString("return %1").arg(var));
						formatVarList.insert(var, qMakePair(FormatType::kTime, util::formatSeconds(varValue.toLongLong())));
					}
					else if (type == "C")
					{
						QString str;
						if (var.toLower() == "date")
						{
							const QDateTime dt = QDateTime::currentDateTime();
							str = dt.toString("yyyy-MM-dd");

						}
						else if (var.toLower() == "time")
						{
							const QDateTime dt = QDateTime::currentDateTime();
							str = dt.toString("hh:mm:ss:zzz");
						}

						formatVarList.insert(var, qMakePair(FormatType::kConst, str));
					}
					else
					{
						varValue = luaDoString(QString("return %1").arg(var));
						formatVarList.insert(var, qMakePair(FormatType::kNothing, varValue.toString()));
					}
				}

				for (auto it = formatVarList.constBegin(); it != formatVarList.constEnd(); ++it)
				{
					const QString var = it.key();
					const QPair<FormatType, QString> varValue = it.value();

					if (varValue.first == FormatType::kNothing)
					{
						formatStr.replace(QString("{:%1}").arg(var), varValue.second);
					}
					else if (varValue.first == FormatType::kTime)
					{
						formatStr.replace(QString("{T:%1}").arg(var), varValue.second);
					}
					else if (varValue.first == FormatType::kConst)
					{
						formatStr.replace(QString("{C:%1}").arg(var), varValue.second);
					}
				}
			}

			insertGlobalVar("vret", formatStr);
			return sol::make_object(s, formatStr.toUtf8().constData());

		});

	lua_.set_function("half", [this](std::string sstr, sol::this_state s)->std::string
		{
			const QString FullWidth = "０１２３４５６７８９"
				"ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ"
				"ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ"
				"、～！＠＃＄％︿＆＊（）＿－＝＋［］｛｝＼｜；：’＂，＜．＞／？【】《》　";
			const QString HalfWidth = "0123456789"
				"abcdefghijklmnopqrstuvwxyz"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"'~!@#$%^&*()_-+=[]{}\\|;:'\",<.>/? []<>";

			if (sstr.empty())
				return sstr;

			QString result = QString::fromUtf8(sstr.c_str());
			qint64 size = FullWidth.size();
			for (qint64 i = 0; i < size; ++i)
			{
				result.replace(FullWidth.at(i), HalfWidth.at(i));
			}

			insertGlobalVar("vret", result);
			return result.toUtf8().constData();
		});

	lua_.set_function("full", [this](std::string sstr, sol::this_state s)->std::string
		{
			const QString FullWidth = "０１２３４５６７８９"
				"ａｂｃｄｅｆｇｈｉｊｋｌｍｎｏｐｑｒｓｔｕｖｗｘｙｚ"
				"ＡＢＣＤＥＦＧＨＩＪＫＬＭＮＯＰＱＲＳＴＵＶＷＸＹＺ"
				"、～！＠＃＄％︿＆＊（）＿－＝＋［］｛｝＼｜；：’＂，＜．＞／？【】《》　";
			const QString HalfWidth = "0123456789"
				"abcdefghijklmnopqrstuvwxyz"
				"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
				"'~!@#$%^&*()_-+=[]{}\\|;:'\",<.>/?[]<> ";

			if (sstr.empty())
				return sstr;

			QString result = QString::fromUtf8(sstr.c_str());
			qint64 size = FullWidth.size();
			for (qint64 i = 0; i < size; ++i)
			{
				result.replace(HalfWidth.at(i), FullWidth.at(i));
			}

			insertGlobalVar("vret", result);
			return result.toUtf8().constData();
		});

	lua_.set_function("lower", [this](std::string sstr, sol::this_state s)->std::string
		{
			QString result = QString::fromUtf8(sstr.c_str()).toLower();
			insertGlobalVar("vret", result);
			return result.toUtf8().constData();
		});

	lua_.set_function("upper", [this](std::string sstr, sol::this_state s)->std::string
		{
			QString result = QString::fromUtf8(sstr.c_str()).toUpper();
			insertGlobalVar("vret", result);
			return result.toUtf8().constData();
		});

	lua_.set_function("trim", [this](std::string sstr, sol::object oisSimplified, sol::this_state s)->std::string
		{
			bool isSimplified = false;

			if (oisSimplified.is<bool>())
				isSimplified = oisSimplified.as<bool>();
			else if (oisSimplified.is<qint64>())
				isSimplified = oisSimplified.as<qint64>() > 0;
			else if (oisSimplified.is<double>())
				isSimplified = oisSimplified.as<double>() > 0.0;

			QString result = QString::fromUtf8(sstr.c_str()).trimmed();

			if (isSimplified)
				result = result.simplified();

			insertGlobalVar("vret", result);
			return result.toUtf8().constData();
		});

	lua_.set_function("todb", [this](sol::object ovalue, sol::this_state s)->double
		{
			double result = 0.0;
			if (ovalue.is<std::string>())
				result = QString::fromUtf8(ovalue.as<std::string>().c_str()).toDouble();
			else if (ovalue.is<qint64>())
				result = ovalue.as<qint64>();
			else if (ovalue.is<double>())
				result = ovalue.as<double>();

			insertGlobalVar("vret", result);
			return result;
		});

	lua_.set_function("tostr", [this](sol::object ovalue, sol::this_state s)->std::string
		{
			QString result = "";
			if (ovalue.is<std::string>())
				result = QString::fromUtf8(ovalue.as<std::string>().c_str());
			else if (ovalue.is<qint64>())
				result = QString::number(ovalue.as<qint64>());
			else if (ovalue.is<double>())
				result = QString::number(ovalue.as<double>(), 'f', 5);

			insertGlobalVar("vret", result);
			return result.toUtf8().constData();
		});

	lua_.set_function("toint", [this](sol::object ovalue, sol::this_state s)->qint64
		{
			qint64 result = 0.0;
			if (ovalue.is<std::string>())
				result = QString::fromUtf8(ovalue.as<std::string>().c_str()).toLongLong();
			else if (ovalue.is<qint64>())
				result = ovalue.as<qint64>();
			else if (ovalue.is<double>())
				result = static_cast<qint64>(qFloor(ovalue.as<double>()));

			insertGlobalVar("vret", result);
			return result;
		});


	lua_.set_function("replace", [this](std::string ssrc, std::string sfrom, std::string sto, sol::object oisRex, sol::this_state s)->std::string
		{
			QString src = QString::fromUtf8(ssrc.c_str());
			QString from = QString::fromUtf8(sfrom.c_str());
			QString to = QString::fromUtf8(sto.c_str());

			bool isRex = false;
			if (oisRex.is<bool>())
				isRex = oisRex.as<bool>();
			else if (oisRex.is<qint64>())
				isRex = oisRex.as<qint64>() > 0;
			else if (oisRex.is<double>())
				isRex = oisRex.as<double>() > 0.0;

			QString result;
			if (!isRex)
				result = src.replace(from, to);
			else
			{
				const QRegularExpression regex(from);
				result = src.replace(regex, to);
			}

			insertGlobalVar("vret", result);

			return result.toUtf8().constData();
		});

	lua_.set_function("find", [this](std::string ssrc, std::string sfrom, sol::object osto, sol::this_state s)->std::string
		{
			QString varValue = QString::fromUtf8(ssrc.c_str());
			QString text1 = QString::fromUtf8(sfrom.c_str());
			QString text2 = "";
			if (osto.is<std::string>())
				text2 = QString::fromUtf8(osto.as<std::string>().c_str());

			QString result = varValue;

			//查找 src 中 text1 到 text2 之间的文本 如果 text2 为空 则查找 text1 到行尾的文本

			qint64 pos1 = varValue.indexOf(text1);
			if (pos1 < 0)
				pos1 = 0;

			qint64 pos2 = -1;
			if (text2.isEmpty())
				pos2 = varValue.length();
			else
			{
				pos2 = static_cast<qint64>(varValue.indexOf(text2, pos1 + text1.length()));
				if (pos2 < 0)
					pos2 = varValue.length();
			}

			result = varValue.mid(pos1 + text1.length(), pos2 - pos1 - text1.length());

			insertGlobalVar("vret", result);
			return result.toUtf8().constData();
		});

	//参数1:字符串内容, 参数2:正则表达式, 参数3(选填):第几个匹配项, 参数4(选填):是否为全局匹配, 参数5(选填):第几组
	lua_.set_function("regex", [this](std::string ssrc, std::string rexstr, sol::object oidx, sol::object oisglobal, sol::object ogidx, sol::this_state s)->std::string
		{
			QString varValue = QString::fromUtf8(ssrc.c_str());

			QString result = varValue;

			QString text = QString::fromUtf8(rexstr.c_str());

			qint64 capture = 1;
			if (oidx.is<qint64>())
				capture = oidx.as<qint64>();

			bool isGlobal = false;
			if (oisglobal.is<qint64>())
				isGlobal = oisglobal.as<qint64>() > 0;
			else if (oisglobal.is<bool>())
				isGlobal = oisglobal.as<bool>();

			qint64 maxCapture = 0;
			if (ogidx.is<qint64>())
				maxCapture = ogidx.as<qint64>();

			const QRegularExpression regex(text);

			if (!isGlobal)
			{
				QRegularExpressionMatch match = regex.match(varValue);
				if (match.hasMatch())
				{
					if (capture < 0 || capture > match.lastCapturedIndex())
					{
						insertGlobalVar("vret", result);
						return result.toUtf8().constData();
					}

					result = match.captured(capture);
				}
			}
			else
			{
				QRegularExpressionMatchIterator matchs = regex.globalMatch(varValue);
				qint64 n = 0;
				while (matchs.hasNext())
				{
					QRegularExpressionMatch match = matchs.next();
					if (++n != maxCapture)
						continue;

					if (capture < 0 || capture > match.lastCapturedIndex())
						continue;

					result = match.captured(capture);
					break;
				}
			}

			insertGlobalVar("vret", result);
			return result.toUtf8().constData();
		});

	//rex 参数1:来源字符串, 参数2:正则表达式
	lua_.set_function("rex", [this](std::string ssrc, std::string rexstr, sol::this_state s)->sol::table
		{
			QString src = QString::fromUtf8(ssrc.c_str());
			sol::state_view lua(s);
			sol::table result = lua.create_table();

			QString expr = QString::fromUtf8(rexstr.c_str());

			const QRegularExpression regex(expr);

			QRegularExpressionMatch match = regex.match(src);

			qint64 n = 1;
			if (match.hasMatch())
			{
				for (qint64 i = 0; i <= match.lastCapturedIndex(); ++i)
				{
					result[n] = match.captured(i).toUtf8().constData();
				}
			}

			qint64 maxdepth = 50;
			insertGlobalVar("vret", getLuaTableString(result, maxdepth));
			return result;
		});

	lua_.set_function("rexg", [this](std::string ssrc, std::string rexstr, sol::this_state s)->sol::table
		{
			QString src = QString::fromUtf8(ssrc.c_str());
			sol::state_view lua(s);
			sol::table result = lua.create_table();

			QString expr = QString::fromUtf8(rexstr.c_str());

			const QRegularExpression regex(expr);

			QRegularExpressionMatchIterator matchs = regex.globalMatch(src);

			qint64 n = 1;
			while (matchs.hasNext())
			{
				QRegularExpressionMatch match = matchs.next();

				if (!match.hasMatch())
					continue;

				for (const auto& capture : match.capturedTexts())
				{
					result[n] = capture.toUtf8().constData();
					++n;
				}
			}

			qint64 maxdepth = 50;
			insertGlobalVar("vret", getLuaTableString(result, maxdepth));
			return result;
		});

	lua_.set_function("rnd", [this](sol::object omin, sol::object omax, sol::this_state s)->qint64
		{
			std::random_device rd;
			std::mt19937_64 gen(rd());
			qint64 result = 0;
			if (omin == sol::lua_nil && omax == sol::lua_nil)
			{
				result = gen();
				insertGlobalVar("vret", result);
				return result;
			}

			qint64 min = 0;
			if (omin.is<qint64>())
				min = omin.as<qint64>();


			qint64 max = 0;
			if (omax.is<qint64>())
				max = omax.as<qint64>();

			if ((min > 0 && max == 0) || (min == max))
			{
				std::uniform_int_distribution<qint64> distribution(0, min);
				result = distribution(gen);
			}
			else if (min > max)
			{
				std::uniform_int_distribution<qint64> distribution(max, min);
				result = distribution(gen);
			}
			else
			{
				std::uniform_int_distribution<qint64> distribution(min, max);
				result = distribution(gen);
			}

			insertGlobalVar("vret", result);
			return result;
		});

	lua_["mkpath"] = [](std::string filename, sol::object obj, sol::this_state s)->std::string
	{
		QString retstring = "\0";
		if (obj == sol::lua_nil)
		{
			retstring = util::findFileFromName(QString::fromUtf8(filename.c_str()) + ".txt");
		}
		else if (obj.is<std::string>())
		{
			retstring = util::findFileFromName(QString::fromUtf8(filename.c_str()) + ".txt", QString::fromUtf8(obj.as<std::string>().c_str()));
		}

		return retstring.toUtf8().constData();
	};

	lua_["mktable"] = [](qint64 a, sol::object ob, sol::this_state s)->sol::object
	{
		sol::state_view lua(s);

		sol::table t = lua.create_table();

		if (ob.is<qint64>() && a > ob.as<qint64>())
		{
			for (qint64 i = ob.as<qint64>(); i < (a + 1); ++i)
			{
				t.add(i);
			}
		}
		else if (ob.is<qint64>() && a < ob.as<qint64>())
		{
			for (qint64 i = a; i < (ob.as<qint64>() + 1); ++i)
			{
				t.add(i);
			}
		}
		else if (ob.is<qint64>() && a == ob.as<qint64>())
		{
			t.add(a);
		}
		else if (ob == sol::lua_nil && a >= 0)
		{
			for (qint64 i = 1; i < a + 1; ++i)
			{
				t.add(i);
			}
		}
		else if (ob == sol::lua_nil && a < 0)
		{
			for (qint64 i = a; i < 2; ++i)
			{
				t.add(i);
			}
		}
		else
			t.add(a);

		return t;
	};

	static const auto Is_1DTable = [](sol::table t)->bool
	{
		for (const std::pair<sol::object, sol::object>& i : t)
		{
			if (i.second.is<sol::table>())
			{
				return false;
			}
		}
		return true;
	};

	lua_["tshuffle"] = [](sol::object t, sol::this_state s)->sol::object
	{
		if (!t.is<sol::table>())
			return sol::lua_nil;
		//檢查是否為1維
		sol::table test = t.as<sol::table>();
		if (test.size() == 0)
			return sol::lua_nil;
		if (!Is_1DTable(test))
			return sol::lua_nil;

		sol::state_view lua(s);
		std::vector<sol::object> v = t.as<std::vector<sol::object>>();
		std::vector<sol::object> v2 = Shuffle(v);
		sol::table t2 = lua.create_table();
		for (const sol::object& i : v2) { t2.add(i); }
		auto copy = [&t](sol::table src)
		{
			//清空原表
			t.as<sol::table>().clear();
			//將篩選後的表複製到原表
			for (const std::pair<sol::object, sol::object>& i : src)
			{
				t.as<sol::table>().add(i.second);
			}
		};
		copy(t2);
		return t2;
	};

	lua_["trotate"] = [](sol::object t, sol::object oside, sol::this_state s)->sol::object
	{
		if (!t.is<sol::table>())
			return sol::lua_nil;

		qint64 len = 1;
		if (oside == sol::lua_nil)
			len = 1;
		else if (oside.is<qint64>())
			len = oside.as<qint64>();
		else
			return sol::lua_nil;

		sol::table test = t.as<sol::table>();
		if (test.size() == 0)
			return sol::lua_nil;
		if (!Is_1DTable(test))
			return sol::lua_nil;

		sol::state_view lua(s);
		std::vector<sol::object> v = t.as<std::vector<sol::object>>();
		std::vector<sol::object> v2 = Rotate(v, len);
		sol::table t2 = lua.create_table();
		for (const sol::object& i : v2) { t2.add(i); }
		auto copy = [&t](sol::table src)
		{
			//清空原表
			t.as<sol::table>().clear();
			//將篩選後的表複製到原表
			for (const std::pair<sol::object, sol::object>& i : src)
			{
				t.as<sol::table>().add(i.second);
			}
		};
		copy(t2);
		return t2;
	};

	lua_["tsleft"] = [](sol::object t, qint64 i, sol::this_state s)->sol::object
	{
		if (!t.is<sol::table>())
			return sol::lua_nil;
		if (i < 0)
			return sol::lua_nil;

		sol::table test = t.as<sol::table>();
		if (test.size() == 0)
			return sol::lua_nil;
		if (!Is_1DTable(test))
			return sol::lua_nil;

		sol::state_view lua(s);
		std::vector<sol::object> v = t.as<std::vector<sol::object>>();
		std::vector<sol::object> v2 = ShiftLeft(v, i);
		sol::table t2 = lua.create_table();
		for (const sol::object& it : v2) { t2.add(it); }
		auto copy = [&t](sol::table src)
		{
			//清空原表
			t.as<sol::table>().clear();
			//將篩選後的表複製到原表
			for (const std::pair<sol::object, sol::object>& i : src)
			{
				t.as<sol::table>().add(i.second);
			}
		};
		copy(t2);
		return t2;
	};

	lua_["tsright"] = [](sol::object t, qint64 i, sol::this_state s)->sol::object
	{
		if (!t.is<sol::table>())
			return sol::lua_nil;
		if (i < 0)
			return sol::lua_nil;

		sol::table test = t.as<sol::table>();
		if (test.size() == 0)
			return sol::lua_nil;
		if (!Is_1DTable(test))
			return sol::lua_nil;

		sol::state_view lua(s);
		std::vector<sol::object> v = t.as<std::vector<sol::object>>();
		std::vector<sol::object> v2 = ShiftRight(v, i);
		sol::table t2 = lua.create_table();
		for (const sol::object& o : v2) { t2.add(o); }
		auto copy = [&t](sol::table src)
		{
			//清空原表
			t.as<sol::table>().clear();
			//將篩選後的表複製到原表
			for (const std::pair<sol::object, sol::object>& o : src)
			{
				t.as<sol::table>().add(o.second);
			}
		};
		copy(t2);
		return t2;
	};

	lua_["tunique"] = [](sol::object t, sol::this_state s)->sol::object
	{
		if (!t.is<sol::table>())
			return sol::lua_nil;

		sol::table test = t.as<sol::table>();
		if (test.size() == 0)
			return sol::lua_nil;

		auto isIntTable = [&test]()->bool
		{
			for (const std::pair<sol::object, sol::object>& i : test)
			{
				if (!i.second.is<qint64>())
					return false;
			}
			return true;
		};

		auto isStringTable = [&test]()->bool
		{
			for (const std::pair<sol::object, sol::object>& i : test)
			{
				if (!i.second.is<std::string>())
					return false;
			}
			return true;
		};

		auto copy = [&t](sol::table src)
		{
			//清空原表
			t.as<sol::table>().clear();
			//將篩選後的表複製到原表
			for (const std::pair<sol::object, sol::object>& i : src)
			{
				t.as<sol::table>().add(i.second);
			}
		};

		sol::state_view lua(s);
		sol::table t2 = lua.create_table();
		if (isIntTable())
		{
			std::vector<qint64> v = t.as<std::vector<qint64>>();
			std::vector<qint64> v2 = Unique(v);
			for (const qint64& i : v2) { t2.add(i); }
			copy(t2);
			return t2;
		}
		else if (isStringTable())
		{
			std::vector<std::string> v = t.as<std::vector<std::string>>();
			std::vector<std::string> v2 = Unique(v);
			for (const std::string& i : v2) { t2.add(i); }
			copy(t2);
			return t2;
		}
		else
			return sol::lua_nil;
	};

	lua_["tsort"] = [](sol::object t, sol::this_state s)->sol::object
	{
		sol::state_view lua(s);
		if (!t.is<sol::table>())
			return sol::lua_nil;

		sol::protected_function sort = lua["table"]["sort"];
		if (sort.valid())
		{
			sort(t, [](sol::object a, sol::object b)->bool
				{
					if (a.is<qint64>() && b.is<qint64>())
					{
						return a.as<qint64>() < b.as<qint64>();
					}
					else if (a.is<double>() && b.is<double>())
					{
						return a.as<double>() < b.as<double>();
					}
					else if (a.is<std::string>() && b.is<std::string>())
					{
						return a.as<std::string>() < b.as<std::string>();
					}
					else
						return false;
				});
		}
		return t;
	};

	lua_["trsort"] = [](sol::object t, sol::this_state s)->sol::object
	{
		sol::state_view lua(s);
		if (!t.is<sol::table>())
			return sol::lua_nil;

		sol::protected_function sort = lua["table"]["sort"];
		if (sort.valid())
		{
			sort(t, [](sol::object a, sol::object b)->bool
				{
					if (a.is<qint64>() && b.is<qint64>())
					{
						return a.as<qint64>() > b.as<qint64>();
					}
					else if (a.is<double>() && b.is<double>())
					{
						return a.as<double>() > b.as<double>();
					}
					else if (a.is<std::string>() && b.is<std::string>())
					{
						return a.as<std::string>() > b.as<std::string>();
					}
					else
						return false;
				});
		}
		return t;
	};

#pragma region _copyfunstr
	std::string _copyfunstr = R"(
		function copy(object)
			local lookup_table = {};
			local function _copy(object)
				if (type(object) ~= ("table")) then
					
					return object;
				elseif (lookup_table[object]) then
					return lookup_table[object];
				end
				local newObject = {};
				lookup_table[object] = newObject;
				for key, value in pairs(object) do
					newObject[_copy(key)] = _copy(value);
				end
				return setmetatable(newObject, getmetatable(object));
			end

			local ret = _copy(object);
			return ret;
		end
	)";

	sol::load_result lr = lua_.load(_copyfunstr);
	if (lr.valid())
	{
		sol::protected_function target = lr.get<sol::protected_function>();
		sol::bytecode target_bc = target.dump();
		lua_.safe_script(target_bc.as_string_view(), sol::script_pass_on_error);
		lua_.collect_garbage();
	}
#pragma endregion

	//表合併
	lua_["tmerge"] = [](sol::object t1, sol::object t2, sol::this_state s)->sol::object
	{
		if (!t1.is<sol::table>() || !t2.is<sol::table>())
			return sol::lua_nil;

		sol::state_view lua(s);
		sol::table t3 = lua.create_table();
		sol::table lookup_table_1 = lua.create_table();
		sol::table lookup_table_2 = lua.create_table();
		//sol::protected_function _copy = lua["copy"];
		sol::table test1 = t1.as<sol::table>();//_copy(t1, lookup_table_1, lua);
		sol::table test2 = t2.as<sol::table>();//_copy(t2, lookup_table_2, lua);
		if (!test1.valid() || !test2.valid())
			return sol::lua_nil;
		for (const std::pair<sol::object, sol::object>& i : test1)
		{
			t3.add(i.second);
		}
		for (const std::pair<sol::object, sol::object>& i : test2)
		{
			t3.add(i.second);
		}
		auto copy = [&t1](sol::table src)
		{
			//清空原表
			t1.as<sol::table>().clear();
			//將篩選後的表複製到原表
			for (const std::pair<sol::object, sol::object>& i : src)
			{
				t1.as<sol::table>().add(i.second);
			}
		};
		copy(t3);
		return t3;
	};

	lua_.set_function("split", [](std::string src, std::string del, sol::this_state s)->sol::object
		{
			sol::state_view lua(s);
			sol::table t = lua.create_table();
			QString qsrc = QString::fromUtf8(src.c_str());
			QString qdel = QString::fromUtf8(del.c_str());
			const QRegularExpression re(qdel);
			QRegularExpression::NoMatchOption;
			QStringList v = qsrc.split(re);
			if (v.size() > 1)
			{
				for (const QString& i : v)
				{
					t.add(i.toUtf8().constData());
				}
				return t;
			}
			else
				return sol::lua_nil;
		});

	lua_["tjoin"] = [this](sol::table t, std::string del, sol::this_state s)->std::string
	{
		QStringList l = {};
		for (const std::pair<sol::object, sol::object>& i : t)
		{
			if (i.second.is<int>())
			{
				l.append(QString::number(i.second.as<int>()));
			}
			else if (i.second.is<double>())
			{
				l.append(QString::number(i.second.as<double>(), 'f', 5));
			}
			else if (i.second.is<bool>())
			{
				l.append(i.second.as<bool>() ? "true" : "false");
			}
			else if (i.second.is<std::string>())
			{
				l.append(QString::fromUtf8(i.second.as<std::string>().c_str()));
			}
		}
		QString ret = l.join(QString::fromUtf8(del.c_str()));
		insertGlobalVar("vret", ret);
		return  ret.toUtf8().constData();
	};

	//根據key交換表中的兩個元素
	lua_["tswap"] = [](sol::table t, sol::object key1, sol::object key2, sol::this_state s)->sol::object
	{
		if (!t.valid())
			return sol::lua_nil;
		if (!t[key1].valid() || !t[key2].valid())
			return sol::lua_nil;
		sol::object temp = t[key1];
		t[key1] = t[key2];
		t[key2] = temp;
		return t;
	};

	lua_["tadd"] = [](sol::table t, sol::object value, sol::this_state s)->sol::object
	{
		if (!t.valid())
			return sol::lua_nil;
		t.add(value);
		return t;
	};

	lua_["tpadd"] = [](sol::table t, sol::object value, sol::this_state s)->sol::object
	{
		sol::state_view lua(s);
		if (!t.valid())
			return sol::lua_nil;
		sol::function insert = lua["table"]["insert"];
		insert(t, 1, value);
		return t;
	};

	lua_["tpopback"] = [](sol::table t, sol::this_state s)->sol::object
	{
		sol::state_view lua(s);
		if (!t.valid())
			return sol::lua_nil;
		sol::function remove = lua["table"]["remove"];
		sol::object temp = t[t.size()];
		remove(t, t.size());
		return t;
	};

	lua_["tpopfront"] = [](sol::table t, sol::this_state s)->sol::object
	{
		sol::state_view lua(s);
		if (!t.valid())
			return sol::lua_nil;
		sol::function remove = lua["table"]["remove"];
		sol::object temp = t[1];
		remove(t, 1);
		return t;
	};

	lua_["tfront"] = [](sol::table t, sol::this_state s)->sol::object
	{
		if (!t.valid())
			return sol::lua_nil;
		return t[1];
	};

	lua_["tback"] = [](sol::table t, sol::this_state s)->sol::object
	{
		if (!t.valid())
			return sol::lua_nil;
		return t[t.size()];
	};
}

Parser::~Parser()
{
	qDebug() << "Parser is distory!!";
}

//讀取腳本文件並轉換成Tokens
bool Parser::loadFile(const QString& fileName, QString* pcontent)
{
	util::ScopedFile f(fileName, QIODevice::ReadOnly | QIODevice::Text);
	if (!f.isOpen())
		return false;

	const QFileInfo fi(fileName);
	const QString suffix = "." + fi.suffix();

	QString c;
	if (suffix == util::SCRIPT_DEFAULT_SUFFIX)
	{
		QTextStream in(&f);
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		in.setCodec(util::DEFAULT_CODEPAGE);
#else
		in.setEncoding(QStringConverter::Utf8);
#endif
		in.setGenerateByteOrderMark(true);
		c = in.readAll();
		c.replace("\r\n", "\n");
		isPrivate_ = false;
	}
#ifdef CRYPTO_H
	else if (suffix == util::SCRIPT_PRIVATE_SUFFIX_DEFAULT)
	{
		Crypto crypto;
		if (!crypto.decodeScript(fileName, c))
			return false;

		isPrivate_ = true;
	}
#endif
	else
	{
		return false;
	}

	if (pcontent != nullptr)
	{
		*pcontent = c;
	}

	bool bret = loadString(c);
	if (bret)
		scriptFileName_ = fileName;
	return bret;
}

//將腳本內容轉換成Tokens
bool Parser::loadString(const QString& content)
{
	bool bret = Lexer::tokenized(&lexer_, content);

	if (bret)
	{
		tokens_ = lexer_.getTokenMaps();
		labels_ = lexer_.getLabelList();
		functionNodeList_ = lexer_.getFunctionNodeList();
		forNodeList_ = lexer_.getForNodeList();
		luaNodeList_ = lexer_.getLuaNodeList();
	}

	return bret;
}

void Parser::insertUserCallBack(const QString& name, const QString& type)
{
	//確保一種類型只能被註冊一次
	for (auto it = userRegCallBack_.cbegin(); it != userRegCallBack_.cend(); ++it)
	{
		if (it.value() == type)
			return;
		if (it.key() == name)
			return;
	}

	userRegCallBack_.insert(name, type);
}

//根據token解釋腳本
void Parser::parse(qint64 line)
{
	setCurrentLine(line); //設置當前行號
	callStack_.clear(); //清空調用棧
	jmpStack_.clear();  //清空跳轉棧

	if (tokens_.isEmpty())
		return;

	bool hasLocalHash = false;
	if (variables_ == nullptr)
	{
		variables_ = new VariantSafeHash();
		if (variables_ == nullptr)
			return;
		hasLocalHash = true;
	}

	bool hasGlobalHash = false;
	if (globalVarLock_ == nullptr)
	{
		globalVarLock_ = new QReadWriteLock();
		if (globalVarLock_ == nullptr)
			return;
		hasGlobalHash = true;
	}

	processTokens();

	if (!isSubScript_ || hasLocalHash || hasGlobalHash)
	{
		if (variables_ != nullptr && hasLocalHash)
		{
			delete variables_;
			variables_ = nullptr;
		}

		if (globalVarLock_ != nullptr && hasGlobalHash)
		{
			delete globalVarLock_;
			globalVarLock_ = nullptr;
		}
	}
}

//處理錯誤
void Parser::handleError(qint64 err, const QString& addition)
{
	if (err == kNoChange)
		return;

	QString extMsg;
	if (!addition.isEmpty())
		extMsg = " " + addition;

	qint64 currentIndex = getIndex();
	SignalDispatcher& signalDispatcher = SignalDispatcher::getInstance(currentIndex);
	if (err == kError)
		emit signalDispatcher.addErrorMarker(getCurrentLine(), err);

	QString msg;
	switch (err)
	{
	case kNoChange:
		return;
	case kError:
		msg = QObject::tr("unknown error") + extMsg;
		break;
	case kServerNotReady:
	{
		msg = QObject::tr("server not ready") + extMsg;
		break;
	}
	case kLabelError:
		msg = QObject::tr("label incorrect or not exist") + extMsg;
		break;
	case kUnknownCommand:
	{
		QString cmd = currentLineTokens_.value(0).data.toString();
		msg = QObject::tr("unknown command: %1").arg(cmd) + extMsg;
		break;
	}
	case kLuaError:
	{
		QString cmd = currentLineTokens_.value(0).data.toString();
		msg = QObject::tr("[lua]:%1").arg(cmd) + extMsg;
		break;
	}
	default:
	{
		if (err >= kArgError && err <= kArgError + 20ll)
		{
			if (err == kArgError)
				msg = QObject::tr("argument error") + extMsg;
			else
				msg = QObject::tr("argument error") + QString(" idx:%1").arg(err - kArgError) + extMsg;
			break;
		}
		break;
	}
	}

	emit signalDispatcher.appendScriptLog(QObject::tr("[error]") + QObject::tr("@ %1 | detail:%2").arg(getCurrentLine() + 1).arg(msg), 6);
}

//比較兩個 QVariant 以 a 的類型為主
bool Parser::compare(const QVariant& a, const QVariant& b, RESERVE type) const
{
	QVariant::Type aType = a.type();

	if (aType == QVariant::Int || aType == QVariant::LongLong || aType == QVariant::Double)
	{
		qint64 numA = a.toDouble();
		qint64 numB = b.toDouble();

		// 根据 type 进行相应的比较操作
		switch (type)
		{
		case TK_EQ: // "=="
			return (numA == numB);

		case TK_NEQ: // "!="
			return (numA != numB);

		case TK_GT: // ">"
			return (numA > numB);

		case TK_LT: // "<"
			return (numA < numB);

		case TK_GEQ: // ">="
			return (numA >= numB);

		case TK_LEQ: // "<="
			return (numA <= numB);

		default:
			return false; // 不支持的比较类型，返回 false
		}
	}
	else
	{
		QString strA = a.toString().simplified();
		QString strB = b.toString().simplified();

		// 根据 type 进行相应的比较操作
		switch (type)
		{
		case TK_EQ: // "=="
			return (strA == strB);

		case TK_NEQ: // "!="
			return (strA != strB);

		case TK_GT: // ">"
			return (strA.compare(strB) > 0);

		case TK_LT: // "<"
			return (strA.compare(strB) < 0);

		case TK_GEQ: // ">="
			return (strA.compare(strB) >= 0);

		case TK_LEQ: // "<="
			return (strA.compare(strB) <= 0);

		default:
			return false; // 不支持的比较类型，返回 false
		}
	}

	return false; // 不支持的类型，返回 false
}

//三目運算表達式替換
void Parser::checkConditionalOp(QString& expr)
{
	static const QRegularExpression rexConditional(R"(([^\,]+)\s+\?\s+([^\,]+)\s+\:\s+([^\,]+))");
	if (expr.contains(rexConditional))
	{
		QRegularExpressionMatch match = rexConditional.match(expr);
		if (match.hasMatch())
		{
			//left ? mid : right
			QString left = match.captured(1);
			QString mid = match.captured(2);
			QString right = match.captured(3);

			//to lua style: (left and { mid } or { right })[1]

			QString luaExpr = QString("(((((%1) ~= nil) and ((%1) == true) or (type(%1) == 'number' and (%1) > 0)) and { %2 } or { %3 })[1])").arg(left).arg(mid).arg(right);
			expr = luaExpr;
		}
	}
}

//執行指定lua語句
QVariant Parser::luaDoString(QString expr)
{
	if (expr.isEmpty())
	{
		return "nil";
	}

	importVariablesToLua(expr);
	checkConditionalOp(expr);

	QString exprStr = QString("%1\n%2").arg(luaLocalVarStringList.join("\n")).arg(expr);

	sol::state& lua_ = clua_.getLua();

	const std::string exprStrUTF8 = exprStr.toUtf8().constData();
	sol::protected_function_result loaded_chunk = lua_.safe_script(exprStrUTF8.c_str(), sol::script_pass_on_error);
	lua_.collect_garbage();
	if (!loaded_chunk.valid())
	{
		qint64 currentLine = getCurrentLine();
		sol::error err = loaded_chunk;
		QString errStr = QString::fromUtf8(err.what());
		if (!isGlobalVarContains(QString("_LUAERR@%1").arg(currentLine)))
			insertGlobalVar(QString("_LUA_DO_STR_ERR@%1").arg(currentLine), exprStr);
		else
		{
			for (qint64 i = 0; i < 1000; ++i)
			{
				if (!isGlobalVarContains(QString("_LUA_DO_STR_ERR@%1%2").arg(currentLine).arg(i)))
				{
					insertGlobalVar(QString("_LUA_DO_STR_ERR@%1_%2").arg(currentLine).arg(i), exprStr);
					break;
				}
			}
		}
		handleError(kLuaError, errStr);
		return "nil";
	}

	sol::object retObject;
	try
	{
		retObject = loaded_chunk;
	}
	catch (...)
	{
		return "nil";
	}

	if (retObject.is<bool>())
		return retObject.as<bool>();
	else if (retObject.is<std::string>())
		return QString::fromUtf8(retObject.as<std::string>().c_str());
	else if (retObject.is<qint64>())
		return retObject.as<qint64>();
	else if (retObject.is<double>())
		return retObject.as<double>();
	else if (retObject.is<sol::table>())
	{
		qint64 deep = kMaxLuaTableDepth;
		return getLuaTableString(retObject.as<sol::table>(), deep);
	}

	return "nil";
}

//取表達式結果
template <typename T>
typename std::enable_if<
	std::is_same<T, QString>::value ||
	std::is_same<T, QVariant>::value ||
	std::is_same<T, bool>::value ||
	std::is_same<T, qint64>::value ||
	std::is_same<T, double>::value
	, bool>::type
	Parser::exprTo(QString expr, T* ret)
{
	if (nullptr == ret)
		return false;

	expr = expr.simplified();

	if (expr.isEmpty())
	{
		*ret = T();
		return true;
	}

	if (expr == "return" || expr == "back" || expr == "continue" || expr == "break"
		|| expr == QString(u8"返回") || expr == QString(u8"跳回") || expr == QString(u8"繼續") || expr == QString(u8"跳出")
		|| expr == QString(u8"继续"))
	{
		if constexpr (std::is_same<T, QVariant>::value || std::is_same<T, QString>::value)
			*ret = expr;
		return true;
	}

	QVariant var = luaDoString(QString("return %1").arg(expr));

	if constexpr (std::is_same<T, QString>::value)
	{
		*ret = var.toString();
		return true;
	}
	else if constexpr (std::is_same<T, QVariant>::value)
	{
		*ret = var;
		return true;
	}
	else if constexpr (std::is_same<T, bool>::value)
	{
		*ret = var.toBool();
		return true;
	}
	else if constexpr (std::is_same<T, qint64>::value)
	{
		*ret = var.toLongLong();
		return true;
	}
	else if constexpr (std::is_same<T, double>::value)
	{
		*ret = var.toDouble();
		return true;
	}

	return false;
}

//取表達式結果 += -= *= /= %= ^=
template <typename T>
typename std::enable_if<std::is_same<T, qint64>::value || std::is_same<T, double>::value || std::is_same<T, bool>::value, bool>::type
Parser::exprCAOSTo(const QString& varName, QString expr, T* ret)
{
	if (nullptr == ret)
		return false;

	expr = expr.simplified();

	if (expr.isEmpty())
	{
		*ret = T();
		return true;
	}

	QString exprStr = QString("%1 = %2 %3;\nreturn %4;").arg(varName).arg(varName).arg(expr).arg(varName);

	QVariant var = luaDoString(exprStr);

	if constexpr (std::is_same<T, bool>::value)
	{
		*ret = var.toBool();
		return true;
	}
	else if constexpr (std::is_same<T, qint64>::value)
	{
		*ret = var.toLongLong();
		return true;
	}
	else if constexpr (std::is_same<T, double>::value)
	{
		*ret = var.toDouble();
		return true;
	}

	return false;
}

void Parser::removeEscapeChar(QString* str) const
{
	if (nullptr == str)
		return;

	str->replace("\\\"", "@@@@@@@@@@");
	str->replace("\"", "");
	str->replace("@@@@@@@@@@", "\"");

	str->replace("'", "@@@@@@@@@@");
	str->replace("\\'", "");
	str->replace("@@@@@@@@@@", "'");

	str->replace("\\\"", "\"");
	str->replace("\\\'", "\'");
	str->replace("\\\\", "\\");
	str->replace("\\n", "\n");
	str->replace("\\r", "\r");
	str->replace("\\t", "\t");
	str->replace("\\b", "\b");
	str->replace("\\f", "\f");
	str->replace("\\v", "\v");
	str->replace("\\a", "\a");
	str->replace("\\?", "\?");
	str->replace("\\0", "\0");
}

//嘗試取指定位置的token轉為QVariant
bool Parser::toVariant(const TokenMap& TK, qint64 idx, QVariant* ret)
{
	if (!TK.contains(idx))
		return false;
	if (ret == nullptr)
		return false;

	RESERVE type = TK.value(idx).type;
	QVariant var = TK.value(idx).data;
	if (!var.isValid())
		return false;

	if (type == TK_STRING || type == TK_CMD)
	{
		QString s = var.toString();

		QString exprStrUTF8;
		qint64 value = 0;
		if (exprTo(s, &value))
			*ret = value;
		else if (exprTo(s, &exprStrUTF8))
			*ret = exprStrUTF8;
		else
			*ret = var;
	}
	else if (type == TK_BOOL)
	{
		bool ok = false;
		qint64 value = var.toLongLong(&ok);
		if (!ok)
			return false;
		*ret = value;
	}
	else if (type == TK_DOUBLE)
	{
		bool ok = false;
		double value = var.toDouble(&ok);
		if (!ok)
			return false;
		*ret = value;
	}
	else
	{
		*ret = var;
	}

	return true;
}

//嘗試取指定位置的token轉為字符串
bool Parser::checkString(const TokenMap& TK, qint64 idx, QString* ret)
{
	if (!TK.contains(idx))
		return false;
	if (ret == nullptr)
		return false;

	RESERVE type = TK.value(idx, Token{}).type;
	QVariant var = TK.value(idx, Token{}).data;

	if (!var.isValid())
		return false;

	if (type == TK_CSTRING)
	{
		*ret = var.toString();
		removeEscapeChar(ret);
	}
	else if (type == TK_STRING || type == TK_CMD)
	{
		*ret = var.toString();
		removeEscapeChar(ret);

		QString exprStrUTF8;
		if (!exprTo(*ret, &exprStrUTF8))
			return false;

		*ret = exprStrUTF8;

	}
	else
		return false;

	return true;
}

//嘗試取指定位置的token轉為整數
bool Parser::checkInteger(const TokenMap& TK, qint64 idx, qint64* ret)
{
	if (!TK.contains(idx))
		return false;
	if (ret == nullptr)
		return false;

	RESERVE type = TK.value(idx).type;
	QVariant var = TK.value(idx).data;

	if (!var.isValid())
		return false;
	if (type == TK_CSTRING)
		return false;

	if (type == TK_INT)
	{
		bool ok = false;
		qint64 value = var.toLongLong(&ok);
		if (!ok)
			return false;

		*ret = value;
		return true;
	}
	else if (type == TK_STRING || type == TK_CMD || type == TK_CAOS)
	{
		QString varValueStr = var.toString();

		QVariant value;
		if (!exprTo(varValueStr, &value))
			return false;

		if (value.type() == QVariant::LongLong || value.type() == QVariant::Int)
		{
			*ret = value.toLongLong();
			return true;
		}
		else if (value.type() == QVariant::Double)
		{
			*ret = value.toDouble();
			return true;
		}
		else if (value.type() == QVariant::Bool)
		{
			*ret = value.toBool() ? 1ll : 0ll;
			return true;
		}
		else
			return false;
	}

	if (type == TK_BOOL)
	{
		bool value = var.toBool();

		*ret = value ? 1ll : 0ll;
		return true;
	}
	else if (type == TK_DOUBLE)
	{
		bool ok = false;
		double value = var.toDouble(&ok);
		if (!ok)
			return false;

		*ret = value;
		return true;
	}
	else if (type == TK_STRING || type == TK_CMD || type == TK_CAOS)
	{
		QString varValueStr = var.toString();

		QVariant value;
		if (!exprTo(varValueStr, &value))
			return false;

		if (value.type() == QVariant::LongLong || value.type() == QVariant::Int)
		{
			*ret = value.toLongLong();
			return true;
		}
	}

	return false;
}

bool Parser::checkNumber(const TokenMap& TK, qint64 idx, double* ret)
{
	if (!TK.contains(idx))
		return false;
	if (ret == nullptr)
		return false;

	RESERVE type = TK.value(idx).type;
	QVariant var = TK.value(idx).data;

	if (!var.isValid())
		return false;
	if (type == TK_CSTRING)
		return false;

	if (type == TK_DOUBLE && var.type() == QVariant::Double)
	{
		bool ok = false;
		double value = var.toDouble(&ok);
		if (!ok)
			return false;

		*ret = value;
		return true;
	}
	else if (type == TK_STRING || type == TK_CMD || type == TK_CAOS)
	{
		QString varValueStr = var.toString();

		QVariant value;
		if (!exprTo(varValueStr, &value))
			return false;

		if (value.type() == QVariant::Double)
		{
			*ret = value.toDouble();
			return true;
		}
	}

	return false;
}

bool Parser::checkBoolean(const TokenMap& TK, qint64 idx, bool* ret)
{
	if (!TK.contains(idx))
		return false;
	if (ret == nullptr)
		return false;

	RESERVE type = TK.value(idx).type;
	QVariant var = TK.value(idx).data;

	if (!var.isValid())
		return false;
	if (type == TK_CSTRING)
		return false;

	if (type == TK_BOOL && var.type() == QVariant::Bool)
	{
		bool value = var.toBool();

		*ret = value;
		return true;
	}
	else if (type == TK_STRING || type == TK_CMD || type == TK_CAOS)
	{
		QString varValueStr = var.toString();

		QVariant value;
		if (!exprTo(varValueStr, &value))
			return false;

		if (value.type() == QVariant::Bool)
		{
			*ret = value.toBool();
			return true;
		}
	}

	return false;
}

//嘗試取指定位置的token轉為按照double -> qint64 -> string順序檢查
QVariant Parser::checkValue(const TokenMap TK, qint64 idx, QVariant::Type type)
{
	QVariant varValue;
	qint64 ivalue;
	QString text;
	bool bvalue;
	double dvalue;

	if (checkBoolean(currentLineTokens_, idx, &bvalue))
	{
		varValue = bvalue;
	}
	else if (checkNumber(currentLineTokens_, idx, &dvalue))
	{
		varValue = dvalue;
	}
	else if (checkInteger(currentLineTokens_, idx, &ivalue))
	{
		varValue = ivalue;
	}
	else if (checkString(currentLineTokens_, idx, &text))
	{
		varValue = text;
	}
	else
	{
		if (type == QVariant::String)
			varValue = "";
		else if (type == QVariant::Double)
			varValue = 0.0;
		else if (type == QVariant::Bool)
			varValue = false;
		else if (type == QVariant::LongLong)
			varValue = 0ll;
		else
			varValue = "nil";
	}

	return varValue;
}

//檢查跳轉是否滿足，和跳轉的方式
qint64 Parser::checkJump(const TokenMap& TK, qint64 idx, bool expr, JumpBehavior behavior)
{
	bool okJump = false;
	if (behavior == JumpBehavior::FailedJump)
		okJump = !expr;
	else
		okJump = expr;

	if (!okJump)
		return Parser::kNoChange;

	QString label;
	qint64 line = 0;
	if (TK.contains(idx))
	{
		QString preCheck = TK.value(idx).data.toString().simplified();

		if (preCheck == "return" || preCheck == "back" || preCheck == "continue" || preCheck == "break"
			|| preCheck == QString(u8"返回") || preCheck == QString(u8"跳回") || preCheck == QString(u8"繼續") || preCheck == QString(u8"跳出")
			|| preCheck == QString(u8"继续"))
		{

			label = preCheck;
		}
		else
		{
			QVariant var = checkValue(TK, idx);
			QVariant::Type type = var.type();
			if (type == QVariant::String && var.toString() != "nil")
			{
				label = var.toString();
				if (label.startsWith("'") || label.startsWith("\""))
					label = label.mid(1);
				if (label.endsWith("'") || label.endsWith("\""))
					label = label.left(label.length() - 1);
			}
			else if (type == QVariant::Int || type == QVariant::LongLong || type == QVariant::Double)
			{
				bool ok = false;
				qint64 value = 0;
				value = var.toLongLong(&ok);
				if (ok)
					line = value;
			}
			else
			{
				label = preCheck;
			}
		}
	}

	if (label.isEmpty() && line == 0)
		line = -1;

	if (!label.isEmpty())
	{
		jump(label, false);
	}
	else if (line != 0)
	{
		jump(line, false);
	}
	else
		return Parser::kArgError + idx;

	return Parser::kHasJump;
}

//檢查"調用"是否傳入參數
void Parser::checkCallArgs(qint64 line)
{
	//check rest of the tokens is exist push to stack 	QStack<QVariantList> callArgs_
	QVariantList list;
	qint64 currentLine = getCurrentLine();
	if (tokens_.contains(currentLine))
	{
		TokenMap TK = tokens_.value(currentLine);
		qint64 size = TK.size();
		for (qint64 i = kCallPlaceHoldSize; i < size; ++i)
		{
			Token token = TK.value(i);
			QVariant var = checkValue(TK, i);
			if (!var.isValid())
				var = "nil";

			if (token.type != TK_FUZZY)
				list.append(var);
			else
				list.append(0ll);
		}
	}

	//無論如何都要在調用call之後壓入新的參數堆棧
	callArgsStack_.push(list);
}

//檢查是否需要跳過整個function代碼塊
bool Parser::checkCallStack()
{
	const QString cmd = currentLineTokens_.value(0).data.toString();
	if (cmd != "function")
		return false;

	const QString funname = currentLineTokens_.value(1).data.toString();

	if (skipFunctionChunkDisable_)
	{
		skipFunctionChunkDisable_ = false;
		return false;
	}

	do
	{
		//棧為空時直接跳過整個代碼塊
		if (callStack_.isEmpty())
			break;

		FunctionNode node = callStack_.top();

		qint64 lastRow = node.callFromLine;

		//匹配棧頂紀錄的function名稱與當前執行名稱是否一致
		QString oldname = tokens_.value(lastRow).value(1).data.toString();
		if (oldname != funname)
		{
			if (!checkString(tokens_.value(lastRow), 1, &oldname))
				break;

			if (oldname != funname)
				break;
		}

		return false;
	} while (false);

	if (matchLineFromFunction(funname) == -1)
		return false;

	FunctionNode node = getFunctionNodeByName(funname);
	jump(std::abs(node.endLine - node.beginLine) + 1, true);
	return true;
}

#pragma region GlobalVar
VariantSafeHash* Parser::getGlobalVarPointer() const
{
	if (globalVarLock_ == nullptr)
		return nullptr;

	QReadLocker locker(globalVarLock_);
	return variables_;
}

void Parser::setVariablesPointer(VariantSafeHash* pvariables, QReadWriteLock* plock)
{
	if (!isSubScript())
		return;

	variables_ = pvariables;
	globalVarLock_ = plock;
}

bool Parser::isGlobalVarContains(const QString& name) const
{
	if (globalVarLock_ == nullptr)
		return false;

	QReadLocker locker(globalVarLock_);
	if (variables_ != nullptr)
		return variables_->contains(name);
	return false;
}

QVariant Parser::getGlobalVarValue(const QString& name) const
{
	if (globalVarLock_ == nullptr)
		return QVariant();

	QReadLocker locker(globalVarLock_);
	if (variables_ != nullptr)
		return variables_->value(name, 0);
	return QVariant();
}

//插入全局變量
void Parser::insertGlobalVar(const QString& name, const QVariant& value)
{
	if (globalVarLock_ == nullptr)
		return;

	QWriteLocker locker(globalVarLock_);
	if (variables_ == nullptr)
		return;

	if (!value.isValid())
		return;

	QVariant var = value;

	QVariant::Type type = value.type();
	if (type != QVariant::LongLong && type != QVariant::String
		&& type != QVariant::Double
		&& type != QVariant::Int && type != QVariant::Bool)
	{
		bool ok = false;
		qint64 val = value.toLongLong(&ok);
		if (ok)
			var = val;
		else
			var = false;
	}

	variables_->insert(name, var);
}

void Parser::insertVar(const QString& name, const QVariant& value)
{
	if (isLocalVarContains(name))
		insertLocalVar(name, value);
	else
		insertGlobalVar(name, value);
}

void Parser::removeGlobalVar(const QString& name)
{
	if (globalVarLock_ == nullptr)
		return;

	QWriteLocker locker(globalVarLock_);
	if (variables_ != nullptr)
	{
		sol::state& lua_ = clua_.getLua();
		variables_->remove(name);
		lua_[name.toUtf8().constData()] = sol::lua_nil;
	}
}

QVariantHash Parser::getGlobalVars()
{
	if (globalVarLock_ == nullptr)
		return QVariantHash();

	QReadLocker locker(globalVarLock_);
	if (variables_ != nullptr)
		return variables_->toHash();
	return QVariantHash();
}

void Parser::clearGlobalVar()
{
	if (globalVarLock_ == nullptr)
		return;

	QWriteLocker locker(globalVarLock_);
	if (variables_ != nullptr)
		variables_->clear();
}
#pragma endregion

#pragma region LocalVar
//插入局變量
void Parser::insertLocalVar(const QString& name, const QVariant& value)
{
	if (name.isEmpty())
		return;

	QVariantHash& hash = getLocalVarsRef();

	if (!value.isValid())
		return;

	QVariant var = value;

	QVariant::Type type = value.type();

	if (type != QVariant::LongLong && type != QVariant::Int
		&& type != QVariant::Double
		&& type != QVariant::Bool
		&& type != QVariant::String)
	{
		bool ok = false;
		qint64 val = value.toLongLong(&ok);
		if (ok)
			hash.insert(name, val);
		else
			hash.insert(name, false);
		return;
	}

	hash.insert(name, var);
}


bool Parser::isLocalVarContains(const QString& name)
{
	QVariantHash hash = getLocalVars();
	if (hash.contains(name))
		return true;

	return false;
}

QVariant Parser::getLocalVarValue(const QString& name)
{
	QVariantHash hash = getLocalVars();
	if (hash.contains(name))
		return hash.value(name);

	return QVariant();
}

void Parser::removeLocalVar(const QString& name)
{
	QVariantHash& hash = getLocalVarsRef();
	if (hash.contains(name))
		hash.remove(name);
}

QVariantHash Parser::getLocalVars() const
{
	if (!localVarStack_.isEmpty())
		return localVarStack_.top();

	return emptyLocalVars_;
}

QVariantHash& Parser::getLocalVarsRef()
{
	if (!localVarStack_.isEmpty())
		return localVarStack_.top();

	localVarStack_.push(emptyLocalVars_);
	return localVarStack_.top();
}
#pragma endregion

qint64 Parser::matchLineFromLabel(const QString& label) const
{
	if (!labels_.contains(label))
		return -1;

	return labels_.value(label, -1);
}

qint64 Parser::matchLineFromFunction(const QString& funcName) const
{
	for (const FunctionNode& node : functionNodeList_)
	{
		if (node.name == funcName)
			return node.beginLine;
	}

	return -1;
}

FunctionNode Parser::getFunctionNodeByName(const QString& funcName) const
{
	for (const FunctionNode& node : functionNodeList_)
	{
		if (node.name == funcName)
			return node;
	}

	return FunctionNode{};
}

ForNode Parser::getForNodeByLineIndex(qint64 line) const
{
	for (const ForNode& node : forNodeList_)
	{
		if (node.beginLine == line)
			return node;
	}

	return ForNode{};
}

QVariantList& Parser::getArgsRef()
{
	if (!callArgsStack_.isEmpty())
		return callArgsStack_.top();
	else
		return emptyArgs_;
}

//表達式替換內容
bool Parser::importVariablesToLua(const QString& expr)
{
	sol::state& lua_ = clua_.getLua();
	QVariantHash globalVars = getGlobalVars();
	for (auto it = globalVars.cbegin(); it != globalVars.cend(); ++it)
	{
		QString key = it.key();
		if (key.isEmpty())
			continue;
		QByteArray keyBytes = key.toUtf8();
		QString value = it.value().toString();
		if (it.value().type() == QVariant::String)
		{
			if (!value.startsWith("{") && !value.endsWith("}"))
			{
				bool ok = false;
				it.value().toLongLong(&ok);
				if (!ok || value.isEmpty())
					lua_.set(keyBytes.constData(), value.toUtf8().constData());
				else
					lua_.set(keyBytes.constData(), it.value().toLongLong());
			}
			else//table
			{
				QString content = QString("%1 = %2;").arg(key).arg(value);
				std::string contentBytes = content.toUtf8().constData();
				sol::protected_function_result loaded_chunk = lua_.safe_script(contentBytes, sol::script_pass_on_error);
				lua_.collect_garbage();
				if (!loaded_chunk.valid())
				{
					sol::error err = loaded_chunk;
					QString errStr = QString::fromUtf8(err.what());
					handleError(kLuaError, errStr);
					return false;
				}
			}
		}
		else if (it.value().type() == QVariant::Double)
			lua_.set(keyBytes.constData(), it.value().toDouble());
		else if (it.value().type() == QVariant::Bool)
			lua_.set(keyBytes.constData(), it.value().toBool());
		else
			lua_.set(keyBytes.constData(), it.value().toLongLong());
	}

	QVariantHash localVars = getLocalVars();
	luaLocalVarStringList.clear();
	for (auto it = localVars.cbegin(); it != localVars.cend(); ++it)
	{
		QString key = it.key();
		if (key.isEmpty())
			continue;
		QString value = it.value().toString();
		QVariant::Type type = it.value().type();
		if (type == QVariant::String && !value.startsWith("{") && !value.endsWith("}"))
		{
			bool ok = false;
			it.value().toLongLong(&ok);

			if (!ok || value.isEmpty())
				value = QString("'%1'").arg(value);
		}
		else if (type == QVariant::Double)
			value = QString("%1").arg(QString::number(it.value().toDouble(), 'f', 16));
		else if (type == QVariant::Bool)
			value = QString("%1").arg(it.value().toBool() ? "true" : "false");

		QString local = QString("local %1 = %2;").arg(key).arg(value);
		luaLocalVarStringList.append(local);
	}

	return updateSysConstKeyword(expr);
}

//根據表達式更新lua內的變量值
bool Parser::updateSysConstKeyword(const QString& expr)
{
	bool bret = false;
	sol::state& lua_ = clua_.getLua();
	if (expr.contains("_LINE_"))
	{
		lua_.set("_LINE_", getCurrentLine() + 1);
		return true;
	}

	if (expr.contains("_FILE_"))
	{
		lua_.set("_FILE_", getScriptFileName().toUtf8().constData());
		return true;
	}

	if (expr.contains("_FUNCTION_"))
	{
		if (!callStack_.isEmpty())
			lua_.set("_FUNCTION_", callStack_.top().name.toUtf8().constData());
		else
			lua_.set("_FUNCTION_", sol::lua_nil);
		return true;
	}

	qint64 currentIndex = getIndex();
	Injector& injector = Injector::getInstance(currentIndex);
	if (injector.server.isNull())
		return false;

	//char\.(\w+)
	if (expr.contains("char"))
	{
		bret = true;
		if (!lua_["char"].valid())
			lua_["char"] = lua_.create_table();
		else
		{
			if (!lua_["char"].is<sol::table>())
				lua_["char"] = lua_.create_table();
		}

		PC _pc = injector.server->getPC();

		lua_["char"]["name"] = _pc.name.toUtf8().constData();

		lua_["char"]["fname"] = _pc.freeName.toUtf8().constData();

		lua_["char"]["lv"] = _pc.level;

		lua_["char"]["hp"] = _pc.hp;

		lua_["char"]["maxhp"] = _pc.maxHp;

		lua_["char"]["hpp"] = _pc.hpPercent;

		lua_["char"]["mp"] = _pc.mp;

		lua_["char"]["maxmp"] = _pc.maxMp;

		lua_["char"]["mpp"] = _pc.mpPercent;

		lua_["char"]["exp"] = _pc.exp;

		lua_["char"]["maxexp"] = _pc.maxExp;

		lua_["char"]["stone"] = _pc.gold;

		lua_["char"]["vit"] = _pc.vit;

		lua_["char"]["str"] = _pc.str;

		lua_["char"]["tgh"] = _pc.tgh;

		lua_["char"]["dex"] = _pc.dex;

		lua_["char"]["atk"] = _pc.atk;

		lua_["char"]["def"] = _pc.def;

		lua_["char"]["chasma"] = _pc.chasma;

		lua_["char"]["turn"] = _pc.transmigration;

		lua_["char"]["earth"] = _pc.earth;

		lua_["char"]["water"] = _pc.water;

		lua_["char"]["fire"] = _pc.fire;

		lua_["char"]["wind"] = _pc.wind;

		lua_["char"]["modelid"] = _pc.modelid;

		lua_["char"]["faceid"] = _pc.faceid;

		lua_["char"]["family"] = _pc.family.toUtf8().constData();

		lua_["char"]["battlepet"] = _pc.battlePetNo + 1;

		lua_["char"]["ridepet"] = _pc.ridePetNo + 1;

		lua_["char"]["mailpet"] = _pc.mailPetNo + 1;

		lua_["char"]["luck"] = _pc.luck;

		lua_["char"]["point"] = _pc.point;
	}

	//pet\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\.(\w+)
	if (expr.contains("pet"))
	{
		bret = true;
		makeTable(lua_, "pet", MAX_PET);

		const QHash<QString, PetState> hash = {
			{ u8"battle", kBattle },
			{ u8"standby", kStandby },
			{ u8"mail", kMail },
			{ u8"rest", kRest },
			{ u8"ride", kRide },
		};

		for (qint64 i = 0; i < MAX_PET; ++i)
		{
			PET pet = injector.server->getPet(i);
			qint64 index = i + 1;

			lua_["pet"][index]["valid"] = pet.valid;

			lua_["pet"][index]["name"] = pet.name.toUtf8().constData();

			lua_["pet"][index]["fname"] = pet.freeName.toUtf8().constData();

			lua_["pet"][index]["lv"] = pet.level;

			lua_["pet"][index]["hp"] = pet.hp;

			lua_["pet"][index]["maxhp"] = pet.maxHp;

			lua_["pet"][index]["hpp"] = pet.hpPercent;

			lua_["pet"][index]["exp"] = pet.exp;

			lua_["pet"][index]["maxexp"] = pet.maxExp;

			lua_["pet"][index]["atk"] = pet.atk;

			lua_["pet"][index]["def"] = pet.def;

			lua_["pet"][index]["agi"] = pet.agi;

			lua_["pet"][index]["loyal"] = pet.loyal;

			lua_["pet"][index]["turn"] = pet.transmigration;

			lua_["pet"][index]["earth"] = pet.earth;

			lua_["pet"][index]["water"] = pet.water;

			lua_["pet"][index]["fire"] = pet.fire;

			lua_["pet"][index]["wind"] = pet.wind;

			lua_["pet"][index]["modelid"] = pet.modelid;

			lua_["pet"][index]["pos"] = pet.index;

			lua_["pet"][index]["index"] = index;

			PetState state = pet.state;
			QString str = hash.key(state, "");
			lua_["pet"][index]["state"] = str.toUtf8().constData();

			lua_["pet"][index]["power"] = (static_cast<double>(pet.atk + pet.def + pet.agi) + (static_cast<double>(pet.maxHp) / 4.0));
		}
	}

	//pet\.(\w+)
	if (expr.contains("pet"))
	{
		bret = true;
		if (!lua_["pet"].valid())
			lua_["pet"] = lua_.create_table();
		else
		{
			if (!lua_["pet"].is<sol::table>())
				lua_["pet"] = lua_.create_table();
		}

		lua_["pet"]["count"] = injector.server->getPetSize();
	}

	//item\[(\d+)\]\.(\w+)
	if (expr.contains("item"))
	{
		bret = true;
		makeTable(lua_, "item", MAX_ITEM);

		injector.server->updateItemByMemory();
		PC _pc = injector.server->getPC();

		for (qint64 i = 0; i < MAX_ITEM; ++i)
		{
			ITEM item = _pc.item[i];
			qint64 index = i + 1;
			if (i < CHAR_EQUIPPLACENUM)
				index += 100;
			else
				index = index - CHAR_EQUIPPLACENUM;

			if (!lua_["item"][index].valid())
				lua_["item"][index] = lua_.create_table();
			else
			{
				if (!lua_["item"][index].is<sol::table>())
					lua_["item"][index] = lua_.create_table();
			}

			lua_["item"][index]["valid"] = item.valid;

			lua_["item"][index]["index"] = index;

			lua_["item"][index]["name"] = item.name.toUtf8().constData();

			lua_["item"][index]["memo"] = item.memo.toUtf8().constData();

			if (item.name == "惡魔寶石" || item.name == "恶魔宝石")
			{
				static QRegularExpression rex("(\\d+)");
				QRegularExpressionMatch match = rex.match(item.memo);
				if (match.hasMatch())
				{
					QString str = match.captured(1);
					bool ok = false;
					qint64 dura = str.toLongLong(&ok);
					if (ok)
						lua_["item"][index]["count"] = dura;
				}
			}

			QString damage = item.damage.simplified();
			if (damage.contains("%"))
				damage.replace("%", "");
			if (damage.contains("％"))
				damage.replace("％", "");

			bool ok = false;
			qint64 dura = damage.toLongLong(&ok);
			if (!ok && !damage.isEmpty())
				lua_["item"][index]["dura"] = 100;
			else
				lua_["item"][index]["dura"] = dura;

			lua_["item"][index]["lv"] = item.level;

			if (item.valid && item.stack == 0)
				item.stack = 1;
			else if (!item.valid)
				item.stack = 0;

			lua_["item"][index]["stack"] = item.stack;

			lua_["item"][index]["lv"] = item.level;
			lua_["item"][index]["field"] = item.field;
			lua_["item"][index]["target"] = item.target;
			lua_["item"][index]["type"] = item.type;
			lua_["item"][index]["modelid"] = item.modelid;
			lua_["item"][index]["name2"] = item.name2.toUtf8().constData();
		}

		if (lua_["item"].is<sol::table>() && !lua_["item"]["sizeof"].valid())
		{
			sol::meta::unqualified_t<sol::table> item = lua_["item"];
			item.set_function("count", [this, currentIndex](sol::object oitemnames, sol::object oitemmemos, sol::this_state s)->qint64
				{
					qint64 count = 0;
					Injector& injector = Injector::getInstance(currentIndex);
					if (injector.server.isNull())
						return count;

					QString itemnames;
					if (oitemnames.is<std::string>())
						itemnames = QString::fromUtf8(oitemnames.as<std::string>().c_str());
					QString itemmemos;
					if (oitemmemos.is<std::string>())
						itemmemos = QString::fromUtf8(oitemmemos.as<std::string>().c_str());

					if (itemnames.isEmpty() && itemmemos.isEmpty())
					{
						insertGlobalVar("vret", 0);
						return count;
					}

					QVector<qint64> itemIndexs;
					if (!injector.server->getItemIndexsByName(itemnames, itemmemos, &itemIndexs))
					{
						insertGlobalVar("vret", 0);
						return count;
					}

					qint64 size = itemIndexs.size();
					PC pc = injector.server->getPC();
					for (qint64 i = 0; i < size; ++i)
					{
						qint64 itemIndex = itemIndexs.at(i);
						ITEM item = pc.item[i];
						if (item.valid)
							count += item.stack;
					}

					insertGlobalVar("vret", count);
					return count;
				});
		}
	}

	//item\.(\w+)
	if (expr.contains("item"))
	{
		bret = true;
		if (!lua_["item"].valid())
			lua_["item"] = lua_.create_table();
		else
		{
			if (!lua_["item"].is<sol::table>())
				lua_["item"] = lua_.create_table();
		}

		QVector<qint64> itemIndexs;
		injector.server->getItemEmptySpotIndexs(&itemIndexs);
		lua_["item"]["space"] = itemIndexs.size();
	}

	//team\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\.(\w+)
	if (expr.contains("team"))
	{
		bret = true;
		makeTable(lua_, "team", MAX_PARTY);

		for (qint64 i = 0; i < MAX_PARTY; ++i)
		{
			PARTY party = injector.server->getParty(i);
			qint64 index = i + 1;

			lua_["team"][index]["valid"] = party.valid;

			lua_["team"][index]["index"] = index;

			lua_["team"][index]["id"] = party.id;

			lua_["team"][index]["name"] = party.name.toUtf8().constData();

			lua_["team"][index]["lv"] = party.level;

			lua_["team"][index]["hp"] = party.hp;

			lua_["team"][index]["maxhp"] = party.maxHp;

			lua_["team"][index]["hpp"] = party.hpPercent;

			lua_["team"][index]["mp"] = party.mp;
		}
	}

	//team\.(\w+)
	if (expr.contains("team"))
	{
		bret = true;
		if (!lua_["team"].valid())
			lua_["team"] = lua_.create_table();
		else
		{
			if (!lua_["team"].is<sol::table>())
				lua_["team"] = lua_.create_table();
		}

		lua_["team"]["count"] = static_cast<qint64>(injector.server->getPartySize());
	}

	//map\.(\w+)
	if (expr.contains("map"))
	{
		bret = true;
		if (!lua_["map"].valid())
			lua_["map"] = lua_.create_table();
		else
		{
			if (!lua_["map"].is<sol::table>())
				lua_["map"] = lua_.create_table();
		}

		lua_["map"]["name"] = injector.server->nowFloorName.toUtf8().constData();

		lua_["map"]["floor"] = injector.server->nowFloor;

		lua_["map"]["x"] = injector.server->getPoint().x();

		lua_["map"]["y"] = injector.server->getPoint().y();
	}

	//card\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\.(\w+)
	if (expr.contains("card"))
	{
		bret = true;
		makeTable(lua_, "card", MAX_ADDRESS_BOOK);
		for (qint64 i = 0; i < MAX_ADDRESS_BOOK; ++i)
		{
			ADDRESS_BOOK addressBook = injector.server->getAddressBook(i);
			qint64 index = i + 1;

			lua_["card"][index]["valid"] = addressBook.valid;

			lua_["card"][index]["index"] = index;

			lua_["card"][index]["name"] = addressBook.name.toUtf8().constData();

			lua_["card"][index]["online"] = addressBook.onlineFlag ? 1 : 0;

			lua_["card"][index]["turn"] = addressBook.transmigration;

			lua_["card"][index]["dp"] = addressBook.dp;

			lua_["card"][index]["lv"] = addressBook.level;
		}
	}

	//chat\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]
	if (expr.contains("chat"))
	{
		bret = true;
		makeTable(lua_, "chat", MAX_CHAT_HISTORY);

		for (qint64 i = 0; i < MAX_CHAT_HISTORY; ++i)
		{
			QString text = injector.server->getChatHistory(i);
			qint64 index = i + 1;

			if (!lua_["chat"][index].valid())
				lua_["chat"][index] = lua_.create_table();
			else
			{
				if (!lua_["chat"][index].is<sol::table>())
					lua_["chat"][index] = lua_.create_table();
			}

			lua_["chat"][index] = text.toUtf8().constData();
		}
	}

	//unit\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\.(\w+)
	if (expr.contains("unit"))
	{
		bret = true;


		QList<mapunit_t> units = injector.server->mapUnitHash.values();

		qint64 size = units.size();
		makeTable(lua_, "unit", size);
		for (qint64 i = 0; i < size; ++i)
		{
			mapunit_t unit = units.at(i);
			if (!unit.isVisible)
				continue;

			qint64 index = i + 1;

			lua_["unit"][index]["valid"] = unit.isVisible;

			lua_["unit"][index]["index"] = index;

			lua_["unit"][index]["id"] = unit.id;

			lua_["unit"][index]["name"] = unit.name.toUtf8().constData();

			lua_["unit"][index]["fname"] = unit.freeName.toUtf8().constData();

			lua_["unit"][index]["family"] = unit.family.toUtf8().constData();

			lua_["unit"][index]["lv"] = unit.level;

			lua_["unit"][index]["dir"] = unit.dir;

			lua_["unit"][index]["x"] = unit.p.x();

			lua_["unit"][index]["y"] = unit.p.y();

			lua_["unit"][index]["gold"] = unit.gold;

			lua_["unit"][index]["modelid"] = unit.modelid;
		}

		lua_["unit"]["count"] = size;
	}

	//battle\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\.(\w+)
	if (expr.contains("battle"))
	{
		bret = true;
		battledata_t battle = injector.server->getBattleData();

		makeTable(lua_, "battle", MAX_ENEMY);

		qint64 size = battle.objects.size();
		for (qint64 i = 0; i < size; ++i)
		{
			battleobject_t obj = battle.objects.at(i);
			qint64 index = i + 1;

			lua_["battle"][index]["valid"] = obj.maxHp > 0 && obj.level > 0 && obj.modelid > 0;

			lua_["battle"][index]["index"] = static_cast<qint64>(obj.pos + 1);

			lua_["battle"][index]["name"] = obj.name.toUtf8().constData();

			lua_["battle"][index]["fname"] = obj.freeName.toUtf8().constData();

			lua_["battle"][index]["modelid"] = obj.modelid;

			lua_["battle"][index]["lv"] = obj.level;

			lua_["battle"][index]["hp"] = obj.hp;

			lua_["battle"][index]["maxhp"] = obj.maxHp;

			lua_["battle"][index]["hpp"] = obj.hpPercent;

			lua_["battle"][index]["status"] = injector.server->getBadStatusString(obj.status).toUtf8().constData();

			lua_["battle"][index]["ride"] = obj.rideFlag > 0 ? 1 : 0;

			lua_["battle"][index]["ridename"] = obj.rideName.toUtf8().constData();

			lua_["battle"][index]["ridelv"] = obj.rideLevel;

			lua_["battle"][index]["ridehp"] = obj.rideHp;

			lua_["battle"][index]["ridemaxhp"] = obj.rideMaxHp;

			lua_["battle"][index]["ridehpp"] = obj.rideHpPercent;
		}

		lua_["battle"]["playerpos"] = static_cast<qint64>(battle.player.pos + 1);
		lua_["battle"]["petpos"] = static_cast<qint64>(battle.pet.pos + 1);
	}

	//battle\.(\w+)
	if (expr.contains("battle"))
	{
		bret = true;
		if (!lua_["battle"].valid())
			lua_["battle"] = lua_.create_table();
		else
		{
			if (!lua_["battle"].is<sol::table>())
				lua_["battle"] = lua_.create_table();
		}

		battledata_t battle = injector.server->getBattleData();

		lua_["battle"]["round"] = injector.server->battleCurrentRound.load(std::memory_order_acquire) + 1;

		lua_["battle"]["field"] = injector.server->getFieldString(battle.fieldAttr).toUtf8().constData();

		lua_["battle"]["dura"] = static_cast<qint64>(injector.server->battleDurationTimer.elapsed() / 1000ll);

		lua_["battle"]["totaldura"] = static_cast<qint64>(injector.server->battle_total_time.load(std::memory_order_acquire) / 1000 / 60);

		lua_["battle"]["totalcombat"] = injector.server->battle_total.load(std::memory_order_acquire);
	}

	//dialog\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]
	if (expr.contains("dialog"))
	{
		bret = true;
		makeTable(lua_, "dialog", MAX_PET, MAX_PET_ITEM);

		QStringList dialog = injector.server->currentDialog.get().linedatas;
		qint64 size = dialog.size();
		bool visible = injector.server->isDialogVisible();
		for (qint64 i = 0; i < MAX_DIALOG_LINE; ++i)
		{
			QString text;
			if (i >= size || !visible)
				text = "";
			else
				text = dialog.at(i);

			qint64 index = i + 1;

			lua_["dialog"][index] = text.toUtf8().constData();
		}
	}

	//dialog\.(\w+)
	if (expr.contains("dialog"))
	{
		bret = true;
		if (!lua_["dialog"].valid())
			lua_["dialog"] = lua_.create_table();
		else
		{
			if (!lua_["dialog"].is<sol::table>())
				lua_["dialog"] = lua_.create_table();
		}

		dialog_t dialog = injector.server->currentDialog.get();

		lua_["dialog"]["id"] = dialog.dialogid;
		lua_["dialog"]["unitid"] = dialog.unitid;
		lua_["dialog"]["type"] = dialog.windowtype;
		lua_["dialog"]["buttontext"] = dialog.linebuttontext.join("|").toUtf8().constData();
		lua_["dialog"]["button"] = dialog.buttontype;
	}


	//magic\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\.(\w+)
	if (expr.contains("magic"))
	{
		bret = true;

		makeTable(lua_, "magic", MAX_MAGIC);

		for (qint64 i = 0; i < MAX_MAGIC; ++i)
		{
			qint64 index = i + 1;
			MAGIC magic = injector.server->getMagic(i);

			lua_["magic"][index]["valid"] = magic.valid;
			lua_["magic"][index]["index"] = index;
			lua_["magic"][index]["costmp"] = magic.costmp;
			lua_["magic"][index]["field"] = magic.field;
			lua_["magic"][index]["name"] = magic.name.toUtf8().constData();
			lua_["magic"][index]["memo"] = magic.memo.toUtf8().constData();
			lua_["magic"][index]["target"] = magic.target;
		}
	}

	//skill\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\.(\w+)
	if (expr.contains("skill"))
	{
		bret = true;

		makeTable(lua_, "skill", MAX_PROFESSION_SKILL);

		for (qint64 i = 0; i < MAX_PROFESSION_SKILL; ++i)
		{
			qint64 index = i + 1;
			PROFESSION_SKILL skill = injector.server->getSkill(i);

			lua_["skill"][index]["valid"] = skill.valid;
			lua_["skill"][index]["index"] = index;
			lua_["skill"][index]["costmp"] = skill.costmp;
			lua_["skill"][index]["modelid"] = skill.icon;
			lua_["skill"][index]["type"] = skill.kind;
			lua_["skill"][index]["lv"] = skill.skill_level;
			lua_["skill"][index]["id"] = skill.skillId;
			lua_["skill"][index]["name"] = skill.name.toUtf8().constData();
			lua_["skill"][index]["memo"] = skill.memo.toUtf8().constData();
			lua_["skill"][index]["target"] = skill.target;
		}
	}

	//petskill\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\.(\w+)
	if (expr.contains("petksill"))
	{
		bret = true;

		makeTable(lua_, "petksill", MAX_PET, MAX_SKILL);

		qint64 petIndex = -1;
		qint64 index = -1;
		qint64 i, j;
		for (i = 0; i < MAX_PET; ++i)
		{
			petIndex = i + 1;

			if (!lua_["petskill"][petIndex].valid())
				lua_["petskill"][petIndex] = lua_.create_table();
			else
			{
				if (!lua_["petskill"][petIndex].is<sol::table>())
					lua_["petskill"][petIndex] = lua_.create_table();
			}

			for (j = 0; j < MAX_SKILL; ++j)
			{
				index = j + 1;
				PET_SKILL skill = injector.server->getPetSkill(i, j);

				if (!lua_["petskill"][petIndex][index].valid())
					lua_["petskill"][petIndex][index] = lua_.create_table();
				else
				{
					if (!lua_["petskill"][petIndex][index].is<sol::table>())
						lua_["petskill"][petIndex][index] = lua_.create_table();
				}

				lua_["petskill"][petIndex][index]["valid"] = skill.valid;
				lua_["petskill"][petIndex][index]["index"] = index;
				lua_["petskill"][petIndex][index]["id"] = skill.skillId;
				lua_["petskill"][petIndex][index]["field"] = skill.field;
				lua_["petskill"][petIndex][index]["target"] = skill.target;
				lua_["petskill"][petIndex][index]["name"] = skill.name.toUtf8().constData();
				lua_["petskill"][petIndex][index]["memo"] = skill.memo.toUtf8().constData();

			}
		}
	}

	//petequip\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\[(?:'([^']*)'|"([^ "]*)"|(\d+))\]\.(\w+)
	if (expr.contains("petequip"))
	{
		bret = true;

		makeTable(lua_, "petequip", MAX_PET, MAX_PET_ITEM);

		qint64 petIndex = -1;
		qint64 index = -1;
		qint64 i, j;
		for (i = 0; i < MAX_PET; ++i)
		{
			petIndex = i + 1;


			for (j = 0; j < MAX_PET_ITEM; ++j)
			{
				index = j + 1;

				ITEM item = injector.server->getPetEquip(i, j);


				QString damage = item.damage;
				damage = damage.replace("%", "");
				damage = damage.replace("％", "");
				bool ok = false;
				qint64 damageValue = damage.toLongLong(&ok);
				if (!ok)
					damageValue = 100;

				lua_["petequip"][petIndex][index]["valid"] = item.valid;
				lua_["petequip"][petIndex][index]["index"] = index;
				lua_["petequip"][petIndex][index]["lv"] = item.level;
				lua_["petequip"][petIndex][index]["field"] = item.field;
				lua_["petequip"][petIndex][index]["target"] = item.target;
				lua_["petequip"][petIndex][index]["type"] = item.type;
				lua_["petequip"][petIndex][index]["modelid"] = item.modelid;
				lua_["petequip"][petIndex][index]["dura"] = damageValue;
				lua_["petequip"][petIndex][index]["name"] = item.name.toUtf8().constData();
				lua_["petequip"][petIndex][index]["name2"] = item.name2.toUtf8().constData();
				lua_["petequip"][petIndex][index]["memo"] = item.memo.toUtf8().constData();

			}
		}
	}

	if (expr.contains("_GAME_"))
	{
		bret = true;
		lua_.set("_GAME_", injector.server->getGameStatus());
	}

	if (expr.contains("_WORLD_"))
	{
		bret = true;
		lua_.set("_WORLD_", injector.server->getWorldStatus());
	}

	return bret;
}

//行跳轉
bool Parser::jump(qint64 line, bool noStack)
{
	qint64 currentLine = getCurrentLine();
	if (!noStack)
		jmpStack_.push(currentLine + 1);
	currentLine += line;
	setCurrentLine(currentLine);
	return true;
}

//指定行跳轉
void Parser::jumpto(qint64 line, bool noStack)
{
	qint64 currentLine = getCurrentLine();
	if (line - 1 < 0)
		line = 1;
	if (!noStack)
		jmpStack_.push(currentLine + 1);
	currentLine = line - 1;
	setCurrentLine(currentLine);
}

//標記跳轉
bool Parser::jump(const QString& name, bool noStack)
{
	QString newName = name.toLower();
	if (newName == "back" || newName == QString(u8"跳回"))
	{
		if (!jmpStack_.isEmpty())
		{
			qint64 returnIndex = jmpStack_.pop();//jump行號出棧
			qint64 jumpLineCount = returnIndex - getCurrentLine();

			return jump(jumpLineCount, true);
		}
		return false;
	}
	else if (newName == "return" || newName == QString(u8"返回"))
	{
		return processReturn(3);//從第三個參數開始取返回值
	}
	else if (newName == "continue" || newName == QString(u8"繼續") || newName == QString(u8"继续"))
	{
		return processContinue();
	}
	else if (newName == "break" || newName == QString(u8"跳出"))
	{
		return processBreak();
	}

	//從跳轉位調用函數
	qint64 jumpLine = matchLineFromFunction(name);
	if (jumpLine != -1)
	{
		FunctionNode node = getFunctionNodeByName(name);
		node.callFromLine = getCurrentLine();
		callStack_.push(node);
		jumpto(jumpLine, true);
		skipFunctionChunkDisable_ = true;
		return true;
	}

	//跳轉標記檢查
	jumpLine = matchLineFromLabel(name);
	if (jumpLine == -1)
	{
		handleError(kLabelError, QString("'%1'").arg(name));
		return false;
	}

	qint64 currentLine = getCurrentLine();
	if (!noStack)
		jmpStack_.push(currentLine + 1);

	qint64 jumpLineCount = jumpLine - currentLine;

	return jump(jumpLineCount, true);
}

//解析跳轉棧或調用棧相關訊息並發送信號
void Parser::generateStackInfo(qint64 type)
{
	if (mode_ != kSync)
		return;

	if (type != 0)
		return;
	qint64 currentIndex = getIndex();
	Injector& injector = Injector::getInstance(currentIndex);

	if (!injector.isScriptDebugModeEnable.load(std::memory_order_acquire))
		return;

	QList<FunctionNode> list = callStack_.toList();
	QVector<QPair<int, QString>> hash;
	if (!list.isEmpty())
	{
		for (const FunctionNode it : list)
		{

			QStringList l;
			QString cmd = QString("name: %1, form: %2, level:%3").arg(it.name).arg(it.callFromLine).arg(it.level);

			hash.append(qMakePair(it.beginLine, cmd));
		}
	}

	SignalDispatcher& signalDispatcher = SignalDispatcher::getInstance(currentIndex);
	if (type == kModeCallStack)
		emit signalDispatcher.callStackInfoChanged(QVariant::fromValue(hash));
	else
		emit signalDispatcher.jumpStackInfoChanged(QVariant::fromValue(hash));
}

QString Parser::getLuaTableString(const sol::table& t, qint64& depth)
{
	if (depth <= 0)
		return "";

	--depth;

	QString ret = "{\n";

	QStringList results;
	QStringList strKeyResults;

	QString nowIndent = "";
	for (qint64 i = 0; i <= 10 - depth + 1; ++i)
	{
		nowIndent += "    ";
	}

	for (const auto& pair : t)
	{
		qint64 nKey = 0;
		QString key = "";
		QString value = "";

		if (pair.first.is<qint64>())
		{
			nKey = pair.first.as<qint64>() - 1;
		}
		else
			key = QString::fromUtf8(pair.first.as<std::string>().c_str());

		if (pair.second.is<sol::table>())
			value = getLuaTableString(pair.second.as<sol::table>(), depth);
		else if (pair.second.is<std::string>())
			value = QString("'%1'").arg(QString::fromUtf8(pair.second.as<std::string>().c_str()));
		else if (pair.second.is<qint64>())
			value = QString::number(pair.second.as<qint64>());
		else if (pair.second.is<double>())
			value = QString::number(pair.second.as<double>(), 'f', 16);
		else if (pair.second.is<bool>())
			value = pair.second.as<bool>() ? "true" : "false";
		else
			value = "nil";

		if (key.isEmpty())
		{
			if (nKey >= results.size())
			{
				for (qint64 i = results.size(); i <= nKey; ++i)
					results.append("nil");
			}

			results[nKey] = nowIndent + value;
		}
		else
			strKeyResults.append(nowIndent + QString("%1 = %2").arg(key).arg(value));
	}

	std::sort(strKeyResults.begin(), strKeyResults.end(), [](const QString& a, const QString& b)
		{
			static const QLocale locale;
			static const QCollator collator(locale);
			return collator.compare(a, b) < 0;
		});

	results.append(strKeyResults);

	ret += results.join(",\n");
	ret += "\n}";

	return ret;
}

//根據關鍵字取值保存到變量
bool Parser::processGetSystemVarValue(const QString& varName, QString& valueStr, QVariant& varValue)
{
	qint64 currentIndex = getIndex();
	Injector& injector = Injector::getInstance(currentIndex);
	if (injector.server.isNull())
		return false;

	QString trimmedStr = valueStr.simplified().toLower();

	enum SystemVarName
	{
		kPlayerInfo,
		kMagicInfo,
		kSkillInfo,
		kPetInfo,
		kPetSkillInfo,
		kMapInfo,
		kItemInfo,
		kEquipInfo,
		kPetEquipInfo,
		kTeamInfo,
		kChatInfo,
		kDialogInfo,
		kPointInfo,
		kBattleInfo,
	};

	const QHash<QString, SystemVarName> systemVarNameHash = {
		{ "player", kPlayerInfo },
		{ "magic", kMagicInfo },
		{ "skill", kSkillInfo },
		{ "pet", kPetInfo },
		{ "petskill", kPetSkillInfo },
		{ "map", kMapInfo },
		{ "item", kItemInfo },
		{ "equip", kEquipInfo },
		{ "petequip", kPetEquipInfo },
		{ "team", kTeamInfo },
		{ "chat", kChatInfo },
		{ "dialog", kDialogInfo },
		{ "point", kPointInfo },
		{ "battle", kBattleInfo },
	};

	if (!systemVarNameHash.contains(trimmedStr))
		return false;

	SystemVarName index = systemVarNameHash.value(trimmedStr);
	bool bret = true;
	varValue = "";
	switch (index)
	{
	case kPlayerInfo:
	{
		QString typeStr;
		checkString(currentLineTokens_, 3, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
		{
			break;
		}

		PC _pc = injector.server->getPC();
		QHash<QString, QVariant> hash = {
			{ "dir", _pc.dir },
			{ "hp", _pc.hp }, { "maxhp", _pc.maxHp }, { "hpp", _pc.hpPercent },
			{ "mp", _pc.mp }, { "maxmp", _pc.maxMp }, { "mpp", _pc.mpPercent },
			{ "vit", _pc.vit },
			{ "str", _pc.str }, { "tgh", _pc.tgh }, { "dex", _pc.dex },
			{ "exp", _pc.exp }, { "maxexp", _pc.maxExp },
			{ "lv", _pc.level },
			{ "atk", _pc.atk }, { "def", _pc.def },
			{ "agi", _pc.agi }, { "chasma", _pc.chasma }, { "luck", _pc.luck },
			{ "earth", _pc.earth }, { "water", _pc.water }, { "fire", _pc.fire }, { "wind", _pc.wind },
			{ "stone", _pc.gold },

			{ "title", _pc.titleNo },
			{ "dp", _pc.dp },
			{ "name", _pc.name },
			{ "fname", _pc.freeName },
			{ "namecolor", _pc.nameColor },
			{ "battlepet", _pc.battlePetNo + 1  },
			{ "mailpet", _pc.mailPetNo + 1 },
			//{ "", _pc.standbyPet },
			//{ "", _pc.battleNo },
			//{ "", _pc.sideNo },
			//{ "", _pc.helpMode },
			//{ "", _pc._pcNameColor },
			{ "turn", _pc.transmigration },
			//{ "", _pc.chusheng },
			{ "family", _pc.family },
			//{ "", _pc.familyleader },
			{ "ridename", _pc.ridePetName },
			{ "ridelv", _pc.ridePetLevel },
			{ "earnstone", injector.server->recorder[0].goldearn },
			{ "earnrep", injector.server->recorder[0].repearn > 0 ? (injector.server->recorder[0].repearn / (injector.server->repTimer.elapsed() / 1000)) * 3600 : 0 },
			//{ "", _pc.familySprite },
			//{ "", _pc.basemodelid },
		};

		if (!hash.contains(typeStr))
			break;

		varValue = hash.value(typeStr);
		bret = varValue.isValid();
		break;
	}
	case kMagicInfo:
	{
		qint64 magicIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &magicIndex))
			break;
		--magicIndex;

		if (magicIndex < 0 || magicIndex > MAX_MAGIC)
			break;

		QString typeStr;
		checkString(currentLineTokens_, 4, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		MAGIC m = injector.server->getMagic(magicIndex);

		const QVariantHash hash = {
			{ "valid", m.valid ? 1 : 0 },
			{ "cost", m.costmp },
			{ "field", m.field },
			{ "target", m.target },
			{ "deadTargetFlag", m.deadTargetFlag },
			{ "name", m.name },
			{ "memo", m.memo },
		};

		if (!hash.contains(typeStr))
			break;

		varValue = hash.value(typeStr);
		break;
	}
	case kSkillInfo:
	{
		qint64 skillIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &skillIndex))
			break;
		--skillIndex;
		if (skillIndex < 0 || skillIndex > MAX_SKILL)
			break;

		QString typeStr;
		checkString(currentLineTokens_, 4, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		PROFESSION_SKILL s = injector.server->getSkill(skillIndex);

		const QHash<QString, QVariant> hash = {
			{ "valid", s.valid ? 1 : 0 },
			{ "cost", s.costmp },
			//{ "field", s. },
			{ "target", s.target },
			//{ "", s.deadTargetFlag },
			{ "name", s.name },
			{ "memo", s.memo },
		};

		if (!hash.contains(typeStr))
			break;

		varValue = hash.value(typeStr);
		break;
	}
	case kPetInfo:
	{
		qint64 petIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &petIndex))
		{
			QString typeStr;
			if (!checkString(currentLineTokens_, 3, &typeStr))
				break;

			if (typeStr == "count")
			{
				varValue = injector.server->getPetSize();
				bret = true;
			}

			break;
		}
		--petIndex;

		if (petIndex < 0 || petIndex >= MAX_PET)
			break;

		QString typeStr;
		checkString(currentLineTokens_, 4, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		const QHash<PetState, QString> petStateHash = {
			{ kBattle, u8"battle" },
			{ kStandby , u8"standby" },
			{ kMail, u8"mail" },
			{ kRest, u8"rest" },
			{ kRide, u8"ride" },
		};

		PET _pet = injector.server->getPet(petIndex);

		QVariantHash hash = {
			{ "index", _pet.index + 1 },						//位置
			{ "modelid", _pet.modelid },						//圖號
			{ "hp", _pet.hp }, { "maxhp", _pet.maxHp }, { "hpp", _pet.hpPercent },					//血量
			{ "mp", _pet.mp }, { "maxmp", _pet.maxMp }, { "mpp", _pet.mpPercent },					//魔力
			{ "exp", _pet.exp }, { "maxexp", _pet.maxExp },				//經驗值
			{ "lv", _pet.level },						//等級
			{ "atk", _pet.atk },						//攻擊力
			{ "def", _pet.def },						//防禦力
			{ "agi", _pet.agi },						//速度
			{ "loyal", _pet.loyal },							//AI
			{ "earth", _pet.earth }, { "water", _pet.water }, { "fire", _pet.fire }, { "wind", _pet.wind },
			{ "maxskill", _pet.maxSkill },
			{ "turn", _pet.transmigration },						// 寵物轉生數
			{ "name", _pet.name },
			{ "fname", _pet.freeName },
			{ "valid", _pet.valid ? 1ll : 0ll },
			{ "turn", _pet.transmigration },
			{ "state", petStateHash.value(_pet.state) },
			{ "power", static_cast<qint64>(qFloor(_pet.power)) }
		};

		if (!hash.contains(typeStr))
			break;

		varValue = hash.value(typeStr);
		break;
	}
	case kPetSkillInfo:
	{
		qint64 petIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &petIndex))
			break;

		if (petIndex < 0 || petIndex >= MAX_PET)
			break;

		qint64 skillIndex = -1;
		if (!checkInteger(currentLineTokens_, 4, &skillIndex))
			break;

		if (skillIndex < 0 || skillIndex >= MAX_SKILL)
			break;

		QString typeStr;
		checkString(currentLineTokens_, 5, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		PET_SKILL _petskill = injector.server->getPetSkill(petIndex, skillIndex);

		QHash<QString, QVariant> hash = {
			{ "valid", _petskill.valid ? 1 : 0 },
			//{ "", _petskill.field },
			{ "target", _petskill.target },
			//{ "", _petskill.deadTargetFlag },
			{ "name", _petskill.name },
			{ "memo", _petskill.memo },
		};

		if (!hash.contains(typeStr))
			break;

		varValue = hash.value(typeStr);
		break;
	}
	case kMapInfo:
	{
		QString typeStr;
		checkString(currentLineTokens_, 3, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		injector.server->reloadHashVar("map");
		VariantSafeHash hashmap = injector.server->hashmap;
		if (!hashmap.contains(typeStr))
			break;

		varValue = hashmap.value(typeStr);
		break;
	}
	case kItemInfo:
	{
		qint64 itemIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &itemIndex))
		{
			QString typeStr;
			if (checkString(currentLineTokens_, 3, &typeStr))
			{
				typeStr = typeStr.simplified().toLower();
				if (typeStr == "space")
				{
					QVector<qint64> itemIndexs;
					qint64 size = 0;
					if (injector.server->getItemEmptySpotIndexs(&itemIndexs))
						size = itemIndexs.size();

					varValue = size;
					break;
				}
				else if (typeStr == "count")
				{
					QString itemName;
					checkString(currentLineTokens_, 4, &itemName);


					QString memo;
					checkString(currentLineTokens_, 5, &memo);

					if (itemName.isEmpty() && memo.isEmpty())
						break;

					QVector<qint64> v;
					qint64 count = 0;
					if (injector.server->getItemIndexsByName(itemName, memo, &v))
					{
						for (const qint64 it : v)
							count += injector.server->getPC().item[it].stack;
					}

					varValue = count;
				}
				else
				{
					QString cmpStr = typeStr;
					if (cmpStr.isEmpty())
						break;

					QString memo;
					checkString(currentLineTokens_, 4, &memo);

					QVector<qint64> itemIndexs;
					if (injector.server->getItemIndexsByName(cmpStr, memo, &itemIndexs))
					{
						qint64 index = itemIndexs.front();
						if (itemIndexs.front() >= CHAR_EQUIPPLACENUM)
							varValue = index + 1 - CHAR_EQUIPPLACENUM;
						else
							varValue = index + 1 + 100;
					}
				}
			}
		}

		qint64 index = itemIndex + CHAR_EQUIPPLACENUM - 1;
		if (index < CHAR_EQUIPPLACENUM || index >= MAX_ITEM)
			break;

		QString typeStr;
		checkString(currentLineTokens_, 4, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		PC pc = injector.server->getPC();
		ITEM item = pc.item[index];

		QString damage = item.damage.simplified();
		qint64 damageValue = 0;
		if (damage.contains("%"))
			damage.replace("%", "");
		if (damage.contains("％"))
			damage.replace("％", "");

		bool ok = false;
		qint64 dura = damage.toLongLong(&ok);
		if (!ok && !damage.isEmpty())
			damageValue = 100;
		else
			damageValue = dura;

		qint64 countdura = 0;
		if (item.name == "惡魔寶石" || item.name == "恶魔宝石")
		{
			static QRegularExpression rex("(\\d+)");
			QRegularExpressionMatch match = rex.match(item.memo);
			if (match.hasMatch())
			{
				QString str = match.captured(1);
				bool ok = false;
				dura = str.toLongLong(&ok);
				if (ok)
					countdura = dura;
			}
		}

		QHash<QString, QVariant> hash = {
			//{ "", item.color },
			{ "modelid", item.modelid },
			{ "lv", item.level },
			{ "stack", item.stack },
			{ "valid", item.valid ? 1 : 0 },
			{ "field", item.field },
			{ "target", item.target },
			//{ "", item.deadTargetFlag },
			//{ "", item.sendFlag },
			{ "name", item.name },
			{ "name2", item.name2 },
			{ "memo", item.memo },
			{ "dura", damageValue },
			{ "count", countdura }
		};

		varValue = hash.value(typeStr);
		break;
	}
	case kEquipInfo:
	{
		qint64 itemIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &itemIndex))
			break;

		qint64 index = itemIndex - 1;
		if (index < 0 || index >= CHAR_EQUIPPLACENUM)
			break;

		QString typeStr;
		checkString(currentLineTokens_, 4, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		PC pc = injector.server->getPC();
		ITEM item = pc.item[index];

		QString damage = item.damage.simplified();
		qint64 damageValue = 0;
		if (damage.contains("%"))
			damage.replace("%", "");
		if (damage.contains("％"))
			damage.replace("％", "");

		bool ok = false;
		qint64 dura = damage.toLongLong(&ok);
		if (!ok && !damage.isEmpty())
			damageValue = 100;
		else
			damageValue = dura;

		QHash<QString, QVariant> hash = {
			//{ "", item.color },
			{ "modelid", item.modelid },
			{ "lv", item.level },
			{ "stack", item.stack },
			{ "valid", item.valid ? 1 : 0 },
			{ "field", item.field },
			{ "target", item.target },
			//{ "", item.deadTargetFlag },
			//{ "", item.sendFlag },
			{ "name", item.name },
			{ "name2", item.name2 },
			{ "memo", item.memo },
			{ "dura", damageValue },
		};

		varValue = hash.value(typeStr);
		break;
	}
	case kPetEquipInfo:
	{
		qint64 petIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &petIndex))
			break;
		--petIndex;

		if (petIndex < 0 || petIndex >= MAX_PET)
			break;

		qint64 itemIndex = -1;
		if (!checkInteger(currentLineTokens_, 4, &itemIndex))
			break;
		--itemIndex;

		if (itemIndex < 0 || itemIndex >= MAX_PET_ITEM)
			break;

		QString typeStr;
		checkString(currentLineTokens_, 5, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		ITEM item = injector.server->getPetEquip(petIndex, itemIndex);

		QString damage = item.damage.simplified();
		qint64 damageValue = 0;
		if (damage.contains("%"))
			damage.replace("%", "");
		if (damage.contains("％"))
			damage.replace("％", "");

		bool ok = false;
		qint64 dura = damage.toLongLong(&ok);
		if (!ok && !damage.isEmpty())
			damageValue = 100;
		else
			damageValue = dura;

		QHash<QString, QVariant> hash = {
			//{ "", item.color },
			{ "modelid", item.modelid },
			{ "lv", item.level },
			{ "stack", item.stack },
			{ "valid", item.valid ? 1 : 0 },
			{ "field", item.field },
			{ "target", item.target },
			//{ "", item.deadTargetFlag },
			//{ "", item.sendFlag },
			{ "name", item.name },
			{ "name2", item.name2 },
			{ "memo", item.memo },
			{ "dura", damageValue },
		};

		if (!hash.contains(typeStr))
			break;

		varValue = hash.value(typeStr);
		break;
	}
	case kTeamInfo:
	{
		qint64 partyIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &partyIndex))
			break;
		--partyIndex;

		if (partyIndex < 0 || partyIndex >= MAX_PARTY)
			break;

		QString typeStr;
		checkString(currentLineTokens_, 4, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		PARTY _party = injector.server->getParty(partyIndex);
		QHash<QString, QVariant> hash = {
			{ "valid", _party.valid ? 1 : 0 },
			{ "id", _party.id },
			{ "lv", _party.level },
			{ "maxhp", _party.maxHp },
			{ "hp", _party.hp },
			{ "hpp", _party.hpPercent },
			{ "mp", _party.mp },
			{ "name", _party.name },
		};

		varValue = hash.value(typeStr);
		break;
	}
	case kChatInfo:
	{
		qint64 chatIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &chatIndex))
			break;

		if (chatIndex < 1 || chatIndex > 20)
			break;

		varValue = injector.server->getChatHistory(chatIndex - 1);
		break;
	}
	case kDialogInfo:
	{
		bool valid = injector.server->isDialogVisible();

		qint64 dialogIndex = -1;
		if (!checkInteger(currentLineTokens_, 3, &dialogIndex))
		{
			QString typeStr;
			if (!checkString(currentLineTokens_, 3, &typeStr))
				break;

			if (typeStr == "id")
			{
				if (!valid)
				{
					varValue = 0;
					break;
				}

				dialog_t dialog = injector.server->currentDialog;
				varValue = dialog.dialogid;
			}
			else if (typeStr == "unitid")
			{
				if (!valid)
				{
					varValue = 0;
					break;
				}

				qint64 unitid = injector.server->currentDialog.get().unitid;
				varValue = unitid;
			}
			else if (typeStr == "type")
			{
				if (!valid)
				{
					varValue = 0;
					break;
				}

				qint64 type = injector.server->currentDialog.get().windowtype;
				varValue = type;
			}
			else if (typeStr == "button")
			{
				if (!valid)
				{
					varValue = "";
					break;
				}

				QStringList list = injector.server->currentDialog.get().linebuttontext;
				varValue = list.join("|");
			}
			break;
		}

		if (!valid)
		{
			varValue = "";
			break;
		}

		QStringList dialogStrList = injector.server->currentDialog.get().linedatas;

		if (dialogIndex == -1)
		{
			QStringList texts;
			for (qint64 i = 0; i < MAX_DIALOG_LINE; ++i)
			{
				if (i >= dialogStrList.size())
					break;

				texts.append(dialogStrList.at(i).simplified());
			}

			varValue = texts.join("\n");
		}
		else
		{
			qint64 index = dialogIndex - 1;
			if (index < 0 || index >= dialogStrList.size())
				break;

			varValue = dialogStrList.at(index);
		}
		break;
	}
	case kPointInfo:
	{
		QString typeStr;
		checkString(currentLineTokens_, 3, &typeStr);
		typeStr = typeStr.simplified().toLower();
		if (typeStr.isEmpty())
			break;

		injector.server->IS_WAITFOR_EXTRA_DIALOG_INFO_FLAG = true;
		injector.server->shopOk(2);

		QElapsedTimer timer; timer.start();
		for (;;)
		{
			if (isInterruptionRequested())
				return false;

			if (timer.hasExpired(5000))
				break;

			if (!injector.server->IS_WAITFOR_EXTRA_DIALOG_INFO_FLAG)
				break;

			QThread::msleep(100);
		}

		//qint64 rep = 0;   // 聲望
		//qint64 ene = 0;   // 氣勢
		//qint64 shl = 0;   // 貝殼
		//qint64 vit = 0;   // 活力
		//qint64 pts = 0;   // 積分
		//qint64 vip = 0;   // 會員點
		currencydata_t currency = injector.server->currencyData;
		if (typeStr == "exp")
		{
			varValue = currency.expbufftime;
		}
		else if (typeStr == "rep")
		{
			varValue = currency.prestige;
		}
		else if (typeStr == "ene")
		{
			varValue = currency.energy;
		}
		else if (typeStr == "shl")
		{
			varValue = currency.shell;
		}
		else if (typeStr == "vit")
		{
			varValue = currency.vitality;
		}
		else if (typeStr == "pts")
		{
			varValue = currency.points;
		}
		else if (typeStr == "vip")
		{
			varValue = currency.VIPPoints;
		}
		else
			break;

		injector.server->press(BUTTON_CANCEL);

		break;
	}
	case kBattleInfo:
	{
		qint64 index = -1;
		if (!checkInteger(currentLineTokens_, 3, &index))
		{
			QString typeStr;
			checkString(currentLineTokens_, 3, &typeStr);
			if (typeStr.isEmpty())
				break;

			if (typeStr == "round")
			{
				varValue = injector.server->battleCurrentRound.load(std::memory_order_acquire);
			}
			else if (typeStr == "field")
			{
				varValue = injector.server->hashbattlefield.get();
			}
			break;
		}

		QString typeStr;
		checkString(currentLineTokens_, 4, &typeStr);

		injector.server->reloadHashVar("battle");
		QHash<qint64, QVariantHash> hashbattle = injector.server->hashbattle.toHash();
		if (!hashbattle.contains(index))
			break;

		QVariantHash hash = hashbattle.value(index);
		if (!hash.contains(typeStr))
			break;

		varValue = hash.value(typeStr);
		break;
	}
	default:
	{
		break;
	}
	}

	return bret;
}

//處理"功能"，檢查是否有聲明局變量 這裡要注意只能執行一次否則會重複壓棧
void Parser::processFunction()
{
	QString functionName = currentLineTokens_.value(1).data.toString();

	//避免因錯誤跳轉到這裡後又重複定義
	if (!callStack_.isEmpty() && callStack_.top().name != functionName)
	{
		return;
	}

	//檢查是否有強制類型
	QVariantList args = getArgsRef();
	QVariantHash labelVars;
	const QStringList typeList = { "int", "double", "bool", "string", "table" };

	for (qint64 i = kCallPlaceHoldSize; i < currentLineTokens_.size(); ++i)
	{
		Token token = currentLineTokens_.value(i);
		if (token.type == TK_FUNCTIONARG)
		{
			QString labelName = token.data.toString().simplified();
			if (labelName.isEmpty())
				continue;

			if (i - kCallPlaceHoldSize >= args.size())
				break;

			QVariant currnetVar = args.at(i - kCallPlaceHoldSize);
			QVariant::Type type = currnetVar.type();

			if (!args.isEmpty() && (args.size() > (i - kCallPlaceHoldSize)) && (currnetVar.isValid()))
			{
				if (labelName.contains("int ", Qt::CaseInsensitive)
					&& (type == QVariant::Type::Int || type == QVariant::Type::LongLong))
				{
					labelVars.insert(labelName.replace("int ", "", Qt::CaseInsensitive), currnetVar.toLongLong());
				}
				else if (labelName.contains("double ", Qt::CaseInsensitive) && (type == QVariant::Type::Double))
				{
					labelVars.insert(labelName.replace("double ", "", Qt::CaseInsensitive), currnetVar.toDouble());
				}
				else if (labelName.contains("string ", Qt::CaseInsensitive) && type == QVariant::Type::String)
				{
					labelVars.insert(labelName.replace("string ", "", Qt::CaseInsensitive), currnetVar.toString());
				}
				else if (labelName.contains("table ", Qt::CaseInsensitive)
					&& type == QVariant::Type::String
					&& currnetVar.toString().startsWith("{")
					&& currnetVar.toString().endsWith("}"))
				{
					labelVars.insert(labelName.replace("table ", "", Qt::CaseInsensitive), currnetVar.toString());
				}
				else if (labelName.contains("bool ", Qt::CaseInsensitive)
					&& (type == QVariant::Type::Int || type == QVariant::Type::LongLong || type == QVariant::Type::Bool))
				{
					labelVars.insert(labelName.replace("bool ", "", Qt::CaseInsensitive), currnetVar.toBool());
				}
				else
				{
					QString expectedType = labelName;
					for (const QString& typeStr : typeList)
					{
						if (!expectedType.startsWith(typeStr))
							continue;

						expectedType = typeStr;
						labelVars.insert(labelName, "nil");
						handleError(kArgError + i - kCallPlaceHoldSize + 1, QString(QObject::tr("@ %1 | Invalid local variable type expacted '%2' but got '%3'")).arg(getCurrentLine()).arg(expectedType).arg(currnetVar.typeName()));
						break;
					}

					if (expectedType == labelName)
						labelVars.insert(labelName, currnetVar);
				}
			}
		}
	}

	currentField.push(TK_FUNCTION);//作用域標誌入棧
	localVarStack_.push(labelVars);//聲明局變量入棧
}

//處理"標記"
void Parser::processLabel()
{

}

//處理"結束"
void Parser::processClean()
{
	callStack_.clear();
	jmpStack_.clear();
	callArgsStack_.clear();
	localVarStack_.clear();
}

//處理所有核心命令之外的所有命令
qint64 Parser::processCommand()
{
	TokenMap tokens = getCurrentTokens();
	Token commandToken = tokens.value(0);
	QVariant varValue = commandToken.data;
	if (!varValue.isValid())
	{
		return kError;
	}

	QString commandName = commandToken.data.toString();
	qint64 status = kNoChange;

	if (commandRegistry_.contains(commandName))
	{
		CommandRegistry function = commandRegistry_.value(commandName, nullptr);
		if (function == nullptr)
		{
			return kError;
		}

		qint64 currentIndex = getIndex();
		status = function(currentIndex, getCurrentLine(), tokens);
	}
	else
	{
		return kError;
	}

	return status;
}

//處理"變量"
void Parser::processVariable(RESERVE type)
{
	switch (type)
	{
	case TK_VARDECL:
	{
		//取第一個參數(變量名)
		QString varName = getToken<QString>(1);
		if (varName.isEmpty())
			break;

		//取第二個參數(類別字符串)
		QString typeStr;
		if (!checkString(currentLineTokens_, 2, &typeStr))
			break;

		if (typeStr.isEmpty())
			break;

		QVariant varValue;
		if (!processGetSystemVarValue(varName, typeStr, varValue))
			break;

		//插入全局變量表
		if (varName.contains("[") && varName.contains("]"))
			processTableSet(varName, varValue);
		else
			insertVar(varName, varValue);
		break;
	}
	case TK_VARFREE:
	{
		QString varName = getToken<QString>(1);
		if (varName.isEmpty())
			break;

		if (isGlobalVarContains(varName))
			removeGlobalVar(varName);
		else
			removeLocalVar(varName);
		break;
	}
	case TK_VARCLR:
	{
		clearGlobalVar();
		break;
	}
	default:
		break;
	}
}

//處理單個或多個局變量聲明
void Parser::processLocalVariable()
{
	QString varNameStr = getToken<QString>(0);
	if (varNameStr.isEmpty())
		return;

	QStringList varNames = varNameStr.split(util::rexComma, Qt::SkipEmptyParts);
	if (varNames.isEmpty())
		return;

	qint64 varCount = varNames.count();
	if (varCount == 0)
		return;

	//取區域變量表
	QVariantHash args = getLocalVars();

	QVariant firstValue;
	QString preExpr = getToken<QString>(1);
	if (preExpr.contains("input("))
	{
		exprTo(preExpr, &firstValue);
		if (firstValue.toLongLong() == 987654321ll)
		{
			requestInterruption();
			return;
		}

		if (!firstValue.isValid())
			firstValue = 0;
	}
	else
		firstValue = checkValue(currentLineTokens_, 1);

	if (varCount == 1)
	{
		insertLocalVar(varNames.front(), firstValue);
		return;
	}

	for (qint64 i = 0; i < varCount; ++i)
	{
		QString varName = varNames.at(i);
		if (varName.isEmpty())
		{
			continue;
		}

		if (i + 1 >= currentLineTokens_.size())
		{
			insertLocalVar(varName, firstValue);
			continue;
		}

		QVariant varValue = checkValue(currentLineTokens_, i + 1);
		if (i > 0 && !varValue.isValid())
		{
			varValue = firstValue;
		}

		if (varValue.type() == QVariant::String && currentLineTokens_.value(i + 1).type != TK_CSTRING)
		{
			QVariant varValue2 = varValue;
			QString expr = varValue.toString();
			if (exprTo(expr, &varValue2) && varValue2.isValid())
				varValue = varValue2.toString();
		}

		insertLocalVar(varName, varValue);
		firstValue = varValue;
	}
}

//處理單個或多個全局變量聲明，或單個局變量重新賦值
void Parser::processMultiVariable()
{
	QString varNameStr = getToken<QString>(0);
	QStringList varNames = varNameStr.split(util::rexComma, Qt::SkipEmptyParts);
	qint64 varCount = varNames.count();
	qint64 value = 0;

	QVariant firstValue;

	QString preExpr = getToken<QString>(1);
	static const QRegularExpression rexCallFun(R"([\w\p{Han}]+\((.+)\))");
	if (preExpr.contains("input("))
	{
		exprTo(preExpr, &firstValue);
		if (firstValue.toLongLong() == 987654321ll)
		{
			requestInterruption();
			return;
		}

		if (!firstValue.isValid())
			firstValue = 0;
	}
	else if (preExpr.contains(rexCallFun))
	{
		QString expr = preExpr;
		exprTo(expr, &firstValue);
		if (!firstValue.isValid())
			firstValue = 0;
	}
	else
		firstValue = checkValue(currentLineTokens_, 1);

	if (varCount == 1)
	{
		//只有一個有可能是局變量改值。使用綜合的insert
		insertVar(varNames.front(), firstValue);
		return;
	}

	//下面是多個變量聲明和初始化必定是全局
	for (qint64 i = 0; i < varCount; ++i)
	{
		QString varName = varNames.at(i);
		if (varName.isEmpty())
		{
			continue;
		}

		if (i + 1 >= currentLineTokens_.size())
		{
			insertGlobalVar(varName, firstValue);
			continue;
		}

		QVariant varValue;
		preExpr = getToken<QString>(i + 1);
		if (preExpr.contains("input("))
		{
			exprTo(preExpr, &varValue);
			if (firstValue.toLongLong() == 987654321ll)
			{
				requestInterruption();
				return;
			}

			if (!varValue.isValid())
				varValue = 0;
		}
		else
			varValue = checkValue(currentLineTokens_, i + 1);

		if (i > 0 && !varValue.isValid())
		{
			varValue = firstValue;
		}

		if (varValue.type() == QVariant::String && currentLineTokens_.value(i + 1).type != TK_CSTRING)
		{
			QVariant varValue2 = varValue;
			QString expr = varValue.toString();

			if (exprTo(expr, &varValue2) && varValue2.isValid())
				varValue = varValue2.toLongLong();
		}

		insertGlobalVar(varName, varValue);
		firstValue = varValue;
	}
}

//處理數組(表)
void Parser::processTable()
{
	QString varName = getToken<QString>(0);
	QString expr = getToken<QString>(1);
	RESERVE type = getTokenType(1);
	if (type == TK_LOCALTABLE)
		insertLocalVar(varName, expr);
	else
		insertGlobalVar(varName, expr);
}

//處理表元素設值
void Parser::processTableSet(const QString& preVarName, const QVariant& value)
{
	static const QRegularExpression rexTableSet(R"(([\w\p{Han}]+)(?:\['*"*(\w+\p{Han}*)'*"*\])?)");
	QString varName = preVarName;
	QString expr;
	sol::state& lua_ = clua_.getLua();

	if (varName.isEmpty())
		varName = getToken<QString>(0);
	else
	{
		QRegularExpressionMatch match = rexTableSet.match(varName);
		if (match.hasMatch())
		{
			varName = match.captured(1).simplified();
			if (!value.isValid())
				return;

			expr = QString("%1[%2] = %3").arg(varName).arg(match.captured(2)).arg(value.toString());
		}
		else
			return;
	}

	if (expr.isEmpty())
		expr = getToken<QString>(1);
	if (expr.isEmpty())
		return;

	importVariablesToLua(expr);
	checkConditionalOp(expr);

	RESERVE type = getTokenType(0);

	if (isLocalVarContains(varName))
	{
		QString localVar = getLocalVarValue(varName).toString();
		if (!localVar.startsWith("{") || !localVar.endsWith("}"))
			return;

		QString exprStr = QString("%1\n%2;\nreturn %3;").arg(luaLocalVarStringList.join("\n")).arg(expr).arg(varName);
		insertGlobalVar("_LUAEXPR", exprStr);

		const std::string exprStrUTF8 = exprStr.toUtf8().constData();
		sol::protected_function_result loaded_chunk = lua_.safe_script(exprStrUTF8.c_str(), sol::script_pass_on_error);
		lua_.collect_garbage();
		if (!loaded_chunk.valid())
		{
			sol::error err = loaded_chunk;
			QString errStr = QString::fromUtf8(err.what());
			insertGlobalVar(QString("_LUAERR@%1").arg(getCurrentLine()), exprStr);
			handleError(kLuaError, errStr);
			return;
		}

		sol::object retObject;
		try
		{
			retObject = loaded_chunk;
		}
		catch (...)
		{
			return;
		}

		if (!retObject.is<sol::table>())
			return;

		qint64 depth = kMaxLuaTableDepth;
		QString resultStr = getLuaTableString(retObject.as<sol::table>(), depth);
		if (resultStr.isEmpty())
			return;

		insertLocalVar(varName, resultStr);
	}
	else if (isGlobalVarContains(varName))
	{
		QString globalVar = getGlobalVarValue(varName).toString();
		if (!globalVar.startsWith("{") || !globalVar.endsWith("}"))
			return;

		QString exprStr = QString("%1\n%2 = %3;\n%4;\nreturn %5;").arg(luaLocalVarStringList.join("\n")).arg(varName).arg(globalVar).arg(expr).arg(varName);
		insertGlobalVar("_LUAEXPR", exprStr);

		const std::string exprStrUTF8 = exprStr.toUtf8().constData();
		sol::protected_function_result loaded_chunk = lua_.safe_script(exprStrUTF8.c_str(), sol::script_pass_on_error);
		lua_.collect_garbage();
		if (!loaded_chunk.valid())
		{
			sol::error err = loaded_chunk;
			QString errStr = QString::fromUtf8(err.what());
			insertGlobalVar(QString("_LUAERR@%1").arg(getCurrentLine()), exprStr);
			handleError(kLuaError, errStr);
			return;
		}

		sol::object retObject;
		try
		{
			retObject = loaded_chunk;
		}
		catch (...)
		{
			return;
		}

		if (!retObject.is<sol::table>())
			return;

		qint64 depth = kMaxLuaTableDepth;
		QString resultStr = getLuaTableString(retObject.as<sol::table>(), depth);
		if (resultStr.isEmpty())
			return;

		insertGlobalVar(varName, resultStr);
	}
}

//處理變量自增自減
void Parser::processVariableIncDec()
{
	QString varName = getToken<QString>(0);
	if (varName.isEmpty())
		return;

	RESERVE op = getTokenType(1);

	QVariantHash args = getLocalVars();

	qint64 value = 0;
	if (args.contains(varName))
		value = args.value(varName, 0ll).toLongLong();
	else
		value = getVar<qint64>(varName);

	if (op == TK_DEC)
		--value;
	else if (op == TK_INC)
		++value;
	else
		return;

	insertVar(varName, value);
}

//處理變量計算
void Parser::processVariableCAOs()
{
	QString varName = getToken<QString>(0);
	if (varName.isEmpty())

		return;

	QVariant varFirst = checkValue(currentLineTokens_, 0);
	QVariant::Type typeFirst = varFirst.type();

	RESERVE op = getTokenType(1);

	QVariant varSecond = checkValue(currentLineTokens_, 2);
	QVariant::Type typeSecond = varSecond.type();

	sol::state& lua_ = clua_.getLua();

	if (typeFirst == QVariant::String && varFirst.toString() != "nil" && varSecond.toString() != "nil")
	{
		if (typeSecond == QVariant::String && op == TK_ADD)
		{
			QString strA = varFirst.toString();
			QString strB = varSecond.toString();
			if (strB.startsWith("'") || strB.startsWith("\""))
				strB = strB.mid(1);
			if (strB.endsWith("'") || strB.endsWith("\""))
				strB = strB.left(strB.length() - 1);

			QString exprStr = QString("return '%1' .. '%2';").arg(strA).arg(strB);
			insertGlobalVar("_LUAEXPR", exprStr);

			importVariablesToLua(exprStr);
			checkConditionalOp(exprStr);

			const std::string exprStrUTF8 = exprStr.toUtf8().constData();
			sol::protected_function_result loaded_chunk = lua_.safe_script(exprStrUTF8.c_str(), sol::script_pass_on_error);
			lua_.collect_garbage();
			if (!loaded_chunk.valid())
			{
				sol::error err = loaded_chunk;
				QString errStr = QString::fromUtf8(err.what());
				insertGlobalVar(QString("_LUAERR@%1").arg(getCurrentLine()), exprStr);
				handleError(kLuaError, errStr);
				return;
			}

			sol::object retObject;
			try
			{
				retObject = loaded_chunk;
			}
			catch (...)
			{
				return;
			}

			if (retObject.is<std::string>())
			{
				insertVar(varName, QString::fromUtf8(retObject.as<std::string>().c_str()));
			}
		}
		return;
	}

	if (typeFirst != QVariant::Int && typeFirst != QVariant::LongLong && typeFirst != QVariant::Double)
		return;

	QString opStr = getToken<QString>(1).simplified();
	opStr.replace("=", "");

	qint64 value = 0;
	QString expr = "";
	if (typeFirst == QVariant::Int || typeFirst == QVariant::LongLong)
		expr = QString("%1 %2").arg(opStr).arg(varSecond.toLongLong());
	else if (typeFirst == QVariant::Double)
		expr = QString("%1 %2").arg(opStr).arg(varSecond.toDouble());
	else if (typeFirst == QVariant::Bool)
		expr = QString("%1 %2").arg(opStr).arg(varSecond.toBool() ? 1 : 0);
	else
		return;

	if (!exprCAOSTo(varName, expr, &value))
		return;
	insertVar(varName, value);
}

//處理變量表達式
void Parser::processVariableExpr()
{
	QString varName = getToken<QString>(0);
	if (varName.isEmpty())
		return;

	QVariant varValue = checkValue(currentLineTokens_, 0);

	QString expr;
	if (!checkString(currentLineTokens_, 1, &expr))
		return;

	QVariant::Type type = varValue.type();

	QVariant result;
	if (type == QVariant::Int || type == QVariant::LongLong)
	{
		qint64 value = 0;
		if (!exprTo(expr, &value))
			return;
		result = value;
	}
	else if (type == QVariant::Double)
	{
		double value = 0;
		if (!exprTo(expr, &value))
			return;
		result = value;
	}
	else if (type == QVariant::Bool)
	{
		bool value = false;
		if (!exprTo(expr, &value))
			return;
		result = value;
	}
	else
	{
		result = expr;
	}

	insertVar(varName, result);
}

//處理if
bool Parser::processIfCompare()
{
	/*
	if (expr) then
		return true;
	else
		return false;
	end
	*/
	bool result = false;
	QString exprStr = QString("if (%1) then return true; else return false; end").arg(getToken<QString>(1).simplified());
	insertGlobalVar("_IFEXPR", exprStr);

	QVariant value = luaDoString(exprStr);

	insertGlobalVar("_IFRESULT", value.toString());

	return checkJump(currentLineTokens_, 2, value.toBool(), SuccessJump) == kHasJump;
}

//處理"格式化"
void Parser::processFormation()
{
	do
	{
		QString varName = getToken<QString>(1);
		if (varName.isEmpty())
			break;

		QString formatStr = checkValue(currentLineTokens_, 2).toString();

		QVariant var = luaDoString(QString("return format(tostring('%1'));").arg(formatStr));

		QString formatedStr = var.toString();

		static const QRegularExpression rexOut(R"(\[(\d+)\])");
		if ((varName.startsWith("out", Qt::CaseInsensitive) && varName.contains(rexOut)) || varName.toLower() == "out")
		{
			QRegularExpressionMatch match = rexOut.match(varName);
			qint64 color = QRandomGenerator::global()->bounded(0, 10);
			if (match.hasMatch())
			{
				QString str = match.captured(1);
				qint64 nColor = str.toLongLong();
				if (nColor >= 0 && nColor <= 10)
					color = nColor;
			}

			qint64 currentIndex = getIndex();
			Injector& injector = Injector::getInstance(currentIndex);
			if (!injector.server.isNull())
				injector.server->announce(formatedStr, color);

			const QDateTime time(QDateTime::currentDateTime());
			const QString timeStr(time.toString("hh:mm:ss:zzz"));
			QString src = "\0";

			QString msg = (QString("[%1 | @%2]: %3\0") \
				.arg(timeStr)
				.arg(getCurrentLine() + 1, 3, 10, QLatin1Char(' ')).arg(formatedStr));

			SignalDispatcher& signalDispatcher = SignalDispatcher::getInstance(currentIndex);
			emit signalDispatcher.appendScriptLog(msg, color);
		}
		else if ((varName.startsWith("say", Qt::CaseInsensitive) && varName.contains(rexOut)) || varName.toLower() == "say")
		{
			QRegularExpressionMatch match = rexOut.match(varName);
			qint64 color = QRandomGenerator::global()->bounded(0, 10);
			if (match.hasMatch())
			{
				QString str = match.captured(1);
				qint64 nColor = str.toLongLong();
				if (nColor >= 0 && nColor <= 10)
					color = nColor;
			}

			qint64 currentIndex = getIndex();
			Injector& injector = Injector::getInstance(currentIndex);
			if (!injector.server.isNull())
				injector.server->talk(formatedStr, color);
		}
		else
		{
			if (varName.contains("[") && varName.contains("]"))
				processTableSet(varName, QVariant::fromValue(formatedStr));
			else
				insertVar(varName, QVariant::fromValue(formatedStr));
		}
	} while (false);
}

//處理"調用"
bool Parser::processCall(RESERVE reserve)
{
	RESERVE type = getTokenType(1);
	do
	{
		QString functionName;
		if (reserve == TK_CALL)//call xxxx
			checkString(currentLineTokens_, 1, &functionName);
		else //xxxx()
			functionName = getToken<QString>(1);

		if (functionName.isEmpty())
			break;

		qint64 jumpLine = matchLineFromFunction(functionName);
		if (jumpLine == -1)
			break;

		FunctionNode node = getFunctionNodeByName(functionName);
		++node.callCount;
		node.callFromLine = getCurrentLine();
		checkCallArgs(jumpLine);
		jumpto(jumpLine + 1, true);
		callStack_.push(node);

		return true;

	} while (false);
	return false;
}

//處理"跳轉"
bool Parser::processGoto()
{
	RESERVE type = getTokenType(1);
	do
	{
		QVariant var = checkValue(currentLineTokens_, 1);
		QVariant::Type type = var.type();
		if (type == QVariant::Int || type == QVariant::LongLong || type == QVariant::Double || type == QVariant::Bool)
		{
			qint64 jumpLineCount = var.toLongLong();
			if (jumpLineCount == 0)
				break;

			jump(jumpLineCount, false);
			return true;
		}
		else if (type == QVariant::String && var.toString() != "nil")
		{
			QString labelName = var.toString();
			if (labelName.isEmpty())
				break;

			jump(labelName, false);
			return true;
		}

	} while (false);
	return false;
}

//處理"指定行跳轉"
bool Parser::processJump()
{
	QVariant var = checkValue(currentLineTokens_, 1);
	if (var.toString() == "nil")
		return false;

	qint64 line = var.toLongLong();
	if (line <= 0)
		return false;

	jumpto(line, false);
	return true;
}

//處理"返回"
bool Parser::processReturn(qint64 takeReturnFrom)
{
	QVariantList list;
	for (qint64 i = takeReturnFrom; i < currentLineTokens_.size(); ++i)
		list.append(checkValue(currentLineTokens_, i));

	qint64 size = list.size();

	lastReturnValue_ = list;
	if (size == 1)
		insertGlobalVar("vret", list.at(0));
	else if (size > 1)
	{
		QString str = "{";
		QStringList v;

		for (const QVariant& var : list)
		{
			QString s = var.toString();
			if (s.isEmpty())
				continue;

			if (var.type() == QVariant::String && s != "nil")
				s = "'" + s + "'";

			v.append(s);
		}

		str += v.join(", ");
		str += "}";

		insertGlobalVar("vret", str);
	}
	else
		insertGlobalVar("vret", "nil");

	if (!callArgsStack_.isEmpty())
		callArgsStack_.pop();//callArgNames出棧

	if (!localVarStack_.isEmpty())
		localVarStack_.pop();//callALocalArgs出棧

	if (!currentField.isEmpty())
	{
		//清空所有forNode
		for (;;)
		{
			if (currentField.isEmpty())
				break;

			auto field = currentField.pop();
			if (field == TK_FOR && !forStack_.isEmpty())
				forStack_.pop();

			//只以fucntino作為主要作用域
			else if (field == TK_FUNCTION)
				break;
		}
	}

	if (callStack_.isEmpty())
	{
		return false;
	}

	FunctionNode node = callStack_.pop();
	qint64 returnIndex = node.callFromLine + 1;
	qint64 jumpLineCount = returnIndex - getCurrentLine();
	jump(jumpLineCount, true);
	return true;
}

//處理"返回跳轉"
void Parser::processBack()
{
	if (jmpStack_.isEmpty())
	{
		setCurrentLine(0);
		return;
	}

	qint64 returnIndex = jmpStack_.pop();//jump行號出棧
	qint64 jumpLineCount = returnIndex - getCurrentLine();
	jump(jumpLineCount, true);
}

//這裡是防止人為設置過長的延時導致腳本無法停止
void Parser::processDelay()
{
	qint64 currentIndex = getIndex();
	Injector& injector = Injector::getInstance(currentIndex);
	qint64 extraDelay = injector.getValueHash(util::kScriptSpeedValue);
	if (extraDelay > 1000ll)
	{
		//將超過1秒的延時分段
		qint64 i = 0ll;
		qint64 size = extraDelay / 1000ll;
		for (i = 0; i < size; ++i)
		{
			if (isInterruptionRequested())
				return;
			QThread::msleep(1000L);
		}
		if (extraDelay % 1000ll > 0ll)
			QThread::msleep(extraDelay % 1000ll);
	}
	else if (extraDelay > 0ll)
	{
		QThread::msleep(extraDelay);
	}
}

//處理"遍歷"
bool Parser::processFor()
{
	//init var value
	QVariant initValue = checkValue(currentLineTokens_, 2);
	if (initValue.type() != QVariant::LongLong)
	{
		initValue = QVariant();
	}

	qint64 nBeginValue = initValue.toLongLong();

	//break condition
	QVariant breakValue = checkValue(currentLineTokens_, 3);
	if (breakValue.type() != QVariant::LongLong)
	{
		breakValue = QVariant();
	}

	qint64 nEndValue = breakValue.toLongLong();

	//step var value
	QVariant stepValue = checkValue(currentLineTokens_, 4);
	if (stepValue.type() != QVariant::LongLong || stepValue.toLongLong() == 0)
		stepValue = 1ll;

	qint64 nStepValue = stepValue.toLongLong();

	qint64 currentLine = getCurrentLine();
	if (forStack_.isEmpty() || forStack_.top().beginLine != currentLine)
	{
		ForNode node = getForNodeByLineIndex(currentLine);
		node.beginValue = nBeginValue;
		node.endValue = nEndValue;
		node.stepValue = stepValue;

		++node.callCount;
		if (!node.varName.isEmpty())
			insertLocalVar(node.varName, nBeginValue);
		forStack_.push(node);
		currentField.push(TK_FOR);
	}
	else
	{
		ForNode node = forStack_.top();

		if (node.varName.isEmpty())
			return false;

		QVariant localValue = getLocalVarValue(node.varName);

		if (localValue.type() != QVariant::LongLong)
			return false;

		if (!initValue.isValid() && !breakValue.isValid())
			return false;

		qint64 nNowLocal = localValue.toLongLong();

		if ((nNowLocal >= nEndValue && nStepValue >= 0)
			|| (nNowLocal <= nEndValue && nStepValue < 0))
		{
			return processBreak();
		}
		else
		{
			nNowLocal += nStepValue;
			insertLocalVar(node.varName, nNowLocal);
		}
	}
	return false;
}

bool Parser::processEnd()
{
	if (currentField.isEmpty())
	{
		if (!forStack_.isEmpty())
			return processEndFor();
		else
			return processReturn();
	}
	else if (currentField.top() == TK_FUNCTION)
	{
		return processReturn();
	}
	else if (currentField.top() == TK_FOR)
	{
		return processEndFor();
	}

	return false;
}

//處理"遍歷結束"
bool Parser::processEndFor()
{
	if (forStack_.isEmpty())
		return false;

	ForNode node = forStack_.top();//跳回 for
	jumpto(node.beginLine + 1, true);
	return true;
}

//處理"繼續"
bool Parser::processContinue()
{
	if (currentField.isEmpty())
		return false;

	RESERVE reserve = currentField.top();
	switch (reserve)
	{
	case TK_FOR:
	{
		if (forStack_.isEmpty())
			break;

		ForNode node = forStack_.top();//跳到 end
		jumpto(node.endLine + 1, true);
		return true;
		break;
	}
	case TK_WHILE:
	case TK_REPEAT:
	default:
		break;
	}

	return false;
}

//處理"跳出"
bool Parser::processBreak()
{
	if (currentField.isEmpty())
		return false;

	RESERVE reserve = currentField.pop();

	switch (reserve)
	{
	case TK_FOR:
	{
		if (forStack_.isEmpty())
			break;

		//for行號出棧
		ForNode node = forStack_.pop();
		qint64 endline = node.endLine + 1;

		jumpto(endline + 1, true);
		return true;
	}
	case TK_WHILE:
	case TK_REPEAT:
	default:
		break;
	}

	return false;
}

bool Parser::processLuaCode()
{
	const qint64 currentLine = getCurrentLine();
	QString luaCode;
	for (const LuaNode& it : luaNodeList_)
	{
		if (currentLine != it.beginLine)
			continue;

		luaCode = it.content;
		break;
	}

	if (luaCode.isEmpty())
		return false;

	const QString luaFunction = QString(R"(
local chunk <const> = function()
	%1
end

return chunk();
	)").arg(luaCode);

	QVariant result = luaDoString(luaFunction);
	bool isValid = result.isValid();
	if (isValid)
		insertGlobalVar("vret", result);
	else
		insertGlobalVar("vret", "nil");
	return isValid;
}

//處理所有的token
void Parser::processTokens()
{
	qint64 currentIndex = getIndex();
	SignalDispatcher& signalDispatcher = SignalDispatcher::getInstance(currentIndex);
	Injector& injector = Injector::getInstance(currentIndex);
	sol::state& lua_ = clua_.getLua();
	clua_.setMax(size());

	//同步模式時清空所有marker並重置UI顯示的堆棧訊息
	if (mode_ == kSync)
	{
		emit signalDispatcher.addErrorMarker(-1, false);
		emit signalDispatcher.addForwardMarker(-1, false);
		emit signalDispatcher.addStepMarker(-1, false);
		generateStackInfo(kModeCallStack);
		generateStackInfo(kModeJumpStack);
	}

	bool skip = false;
	RESERVE currentType = TK_UNK;
	QString name;

	QElapsedTimer timer; timer.start();

	for (;;)
	{
		if (isInterruptionRequested())
		{
			break;
		}

		if (empty())
		{
			break;
		}

		//導出變量訊息
		if (mode_ == kSync && !skip)
			exportVarInfo();

		qint64 currentLine = getCurrentLine();
		currentLineTokens_ = tokens_.value(currentLine);

		currentType = getCurrentFirstTokenType();

		skip = currentType == RESERVE::TK_WHITESPACE || currentType == RESERVE::TK_COMMENT || currentType == RESERVE::TK_UNK;

		if (currentType == TK_FUNCTION)
		{
			if (checkCallStack())
				continue;
		}

		if (!skip)
		{
			processDelay();
			if (!lua_["_DEBUG"].is<bool>() || lua_["_DEBUG"].get<bool>() || injector.isScriptDebugModeEnable.load(std::memory_order_acquire))
			{
				lua_["_DEBUG"] = true;
				QThread::msleep(1);
			}
			else
				lua_["_DEBUG"] = false;

			if (callBack_ != nullptr)
			{
				qint64 status = callBack_(currentIndex, currentLine, currentLineTokens_);
				if (status == kStop)
					break;
			}
		}

		switch (currentType)
		{
		case TK_COMMENT:
		case TK_WHITESPACE:
		{
			break;
		}
		case TK_EXIT:
		{
			lastCriticalError_ = kNoError;
			name.clear();
			requestInterruption();
			break;
		}
		case TK_LUABEGIN:
		{
			if (!processLuaCode())
			{
				name.clear();
				requestInterruption();
			}
			break;
		}
		case TK_LUAEND:
		{
			break;
		}
		case TK_IF:
		{
			if (processIfCompare())
				continue;

			break;
		}
		case TK_CMD:
		{
			qint64 ret = processCommand();
			switch (ret)
			{
			case kHasJump:
			{
				generateStackInfo(kModeJumpStack);
				continue;
			}
			case kError:
			case kArgError:
			case kUnknownCommand:
			default:
			{
				handleError(ret);
				name.clear();
				break;
			}
			break;
			}

			break;
		}
		case TK_LOCAL:
		{
			processLocalVariable();
			break;
		}
		case TK_VARDECL:
		case TK_VARFREE:
		case TK_VARCLR:
		{
			processVariable(currentType);
			break;
		}
		case TK_TABLESET:
		{
			processTableSet();
			break;
		}
		case TK_TABLE:
		case TK_LOCALTABLE:
		{
			processTable();
			break;
		}
		case TK_MULTIVAR:
		{
			processMultiVariable();
			break;
		}
		case TK_INCDEC:
		{
			processVariableIncDec();
			break;
		}
		case TK_CAOS:
		{
			processVariableCAOs();
			break;
		}
		case TK_EXPR:
		{
			processVariableExpr();
			break;
		}
		case TK_FORMAT:
		{
			processFormation();
			break;
		}
		case TK_GOTO:
		{
			if (processGoto())
				continue;

			break;
		}
		case TK_JMP:
		{
			if (processJump())
				continue;

			break;
		}
		case TK_CALLWITHNAME:
		case TK_CALL:
		{
			if (processCall(currentType))
			{
				generateStackInfo(kModeCallStack);
				continue;
			}

			generateStackInfo(kModeCallStack);

			break;
		}
		case TK_FUNCTION:
		{
			processFunction();
			break;
		}
		case TK_LABEL:
		{
			processLabel();
			break;
		}
		case TK_FOR:
		{
			if (processFor())
				continue;

			break;
		}
		case TK_BREAK:
		{
			if (processBreak())
				continue;

			break;
		}
		case TK_CONTINUE:
		{
			if (processContinue())
				continue;

			break;
		}
		case TK_END:
		{
			if (processEnd())
				continue;

			break;
		}
		case TK_RETURN:
		{
			bool bret = processReturn();
			generateStackInfo(kModeCallStack);
			if (bret)
				continue;

			break;
		}
		case TK_BAK:
		{
			processBack();
			generateStackInfo(kModeJumpStack);
			continue;
		}
		default:
		{
			qDebug() << "Unexpected token type:" << currentType;
			break;
		}
		}

		//導出變量訊息
		if (mode_ == kSync && !skip)
			exportVarInfo();

		//移動至下一行
		next();
	}

	processClean();
	lua_.collect_garbage();

	/*==========全部重建 : 成功 2 個，失敗 0 個，略過 0 個==========
	========== 重建 開始於 1:24 PM 並使用了 01:04.591 分鐘 ==========*/
	emit signalDispatcher.appendScriptLog(QObject::tr(" ========== script result : %1，cost %2 ==========")
		.arg(isSubScript() ? QObject::tr("sub-ok") : QObject::tr("main-ok")).arg(util::formatMilliseconds(timer.elapsed())));

}

//導出變量訊息
void Parser::exportVarInfo()
{
	qint64 currentIndex = getIndex();
	Injector& injector = Injector::getInstance(currentIndex);
	if (!injector.isScriptDebugModeEnable.load(std::memory_order_acquire))
		return;


	QVariantHash varhash;
	VariantSafeHash* pglobalHash = getGlobalVarPointer();
	QVariantHash localHash;
	if (!localVarStack_.isEmpty())
		localHash = localVarStack_.top();

	QString key;
	for (auto it = pglobalHash->cbegin(); it != pglobalHash->cend(); ++it)
	{
		key = QString("global|%1").arg(it.key());
		varhash.insert(key, it.value());
	}

	for (auto it = localHash.cbegin(); it != localHash.cend(); ++it)
	{
		key = QString("local|%1").arg(it.key());
		varhash.insert(key, it.value());
	}

	SignalDispatcher& signalDispatcher = SignalDispatcher::getInstance(currentIndex);
	emit signalDispatcher.varInfoImported(this, varhash);
}