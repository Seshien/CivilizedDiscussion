#pragma once
struct DebaterRep
{
	DebaterRep(int id, int clock)
	{
		this->id = id;
		this->clock = clock;
	}
	int id;
	int clock;
};

//if true, first go before second
bool compare(DebaterRep a, DebaterRep b)
{
	if (a.id == b.id)
		return false;
	if (a.clock == b.clock)
		return a.id < b.id;
	return a.clock < b.clock;
}