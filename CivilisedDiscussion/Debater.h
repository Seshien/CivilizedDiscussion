#pragma once
#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include <set>

#include "mpi.h"

#include "Utility.h"
#include "MsgStructure.h"
#include "DebaterRep.h"

//defaultowe wartosci, ale mozna je zmienic
#define RSIZE 3
#define MSIZE 6
#define PSIZE 6
#define GSIZE 6
#define WAITMIN 5000
#define WAITMAX 10000
#define DISCUSSIONWAIT 7500
#define MESSAGEWAIT 1000
#define DEBUG_MODE false

class Debater
{
public:
	Debater(int id, int sizeDebaters, MPI_Datatype msgStruct);

private:
	int id;
	int sizeDebaters;
	int clock;
	int partner = -1, position = - 1;
	int roomsAmount = RSIZE;
	int itemAmount[3] = { MSIZE, PSIZE, GSIZE };
	int choice, partnerChoice = -1;
	MPI_Datatype MPI_msgStruct;
	MsgStructure packet;
	//std::list<DebaterRep> otherDebaters;
	//std::set<int, decltype(&cmp)> s(&cmp);

	std::list<DebaterRep> otherDebaters;

	std::list<DebaterRep> friendsQ, roomsQ;
	std::vector<std::list<DebaterRep>> itemQ;
	int ackFriendCounter = 0;
	enum class Status : int {FREE, WAITING, TAKEN};
	Status friendTaken = Status::FREE, inviteTaken = Status::FREE, roomTaken = Status::FREE, itemTaken = Status::FREE;
	std::condition_variable checkpointF, checkpointR, checkpointCh;
	std::mutex mutexF, mutexR, mutexCh, mutexPrint, mutexCl;

public:
	void interpretArgs(int argc, char** argv);
	void run();

private:

	void communicate();

	void searchForPartner();

	void searchRoom();

	void waitForPartner();

	void searchItem();

	void waitForRD();

	bool getResult();

	void broadcastMessage(Type type, SubType subtype);

	void sendMessage(int destination, Type type, SubType subtype);

	//nie zwieksza clocka
	void _sendMessage(int destination, MsgStructure * msg);

	void safeAdd(const DebaterRep & rep, std::list<DebaterRep> & list, bool duplicates = false);

	//zwraca beforeid
	int deleteUntil(std::list<DebaterRep> & list, bool partner = false);

	int findPosition(const std::list<DebaterRep> & list);

	int randomTime(int start, int end);

	void wait(int time);

	void printColour(const std::string & str, bool important = false);

	std::string getType(int id);

	std::string getSubType(int id);
};