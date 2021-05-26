#pragma once
enum class Type : int { FRIENDS, ROOMS, M, P, G };

enum class SubType : int { REQ, ACK, INVITE, chM, chP, chG };
struct MsgStructure
{
	MsgStructure()
	{
	}
	MsgStructure(int id, int ts, Type type, SubType subtype)
	{
		this->id = id;
		this->ts = ts;
		this->type = (int) type;
		this->subtype = (int) subtype;
	}
	int id;
	int ts;
	int type;
	int subtype;
};

