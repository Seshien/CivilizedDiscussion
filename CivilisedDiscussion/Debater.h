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
#include "Utility.h"
#include "mpi.h"

#include "MsgStructure.h"
#include "DebaterRep.h"


#define RSIZE 2
#define MSIZE 3
#define PSIZE 3
#define GSIZE 3
#define WAITMIN 5000
#define WAITMAX 10000
#define MESSAGEWAIT 1000


class Debater
{
public:
	Debater(int id, int sizeDebaters, MPI_Datatype msgStruct)
	{
		this->id = id;
		this->sizeDebaters = sizeDebaters;
		this->MPI_msgStruct = msgStruct;
		this->clock = 0;

		//stworz innych debaterow tam gdzie jest to konieczne

		itemQ.push_back(std::set<DebaterRep, cmp>());
		itemQ.push_back(std::set<DebaterRep, cmp>());
		itemQ.push_back(std::set<DebaterRep, cmp>());

		for (int i = 0; i < this->sizeDebaters; i++)
		{
			roomsQ.insert(DebaterRep(i, 0));
			for (auto & iQ : itemQ)
				iQ.insert(DebaterRep(i, 0));
			otherDebaters.insert(DebaterRep(i, 0));
		}


	}

private:
	int id;
	int sizeDebaters;
	int clock;
	int partner, position;
	int roomsAmount = RSIZE, mAmount = MSIZE, pAmount = PSIZE, gAmount = GSIZE;
	int itemLimit = 0;
	int choice, partnerChoice = -1;
	MPI_Datatype MPI_msgStruct;
	MsgStructure packet;
	//std::list<DebaterRep> otherDebaters;
	//std::set<int, decltype(&cmp)> s(&cmp);

	std::set<DebaterRep, cmp> otherDebaters;

	std::set<DebaterRep, cmp> friendsQ, roomsQ;
	std::vector<std::set<DebaterRep, cmp>> itemQ;
	int ackFriendCounter;
	enum class Status : int {FREE, WAITING, TAKEN};
	Status friendReady, inviteReady, roomTaken, chTaken;
	std::condition_variable checkpointF, checkpointR, checkpointCh;
	std::mutex mutexF, mutexR, mutexCh, mutexPrint;

public:

	void run()
	{
		print("Start Running");
		std::thread t(&Debater::communicate, this);
		t.detach();

		while (1)
		{
			//wait
			wait(randomTime(WAITMIN, WAITMAX));
			print("Starts searching for partner");
			//zeruj liczbe ACK[F] i wszystkie inne zmienne
			safeChange(ackFriendCounter, 0, mutexF);
			//ackFriendCounter = 0;
			//friendReady = false;

			safeChange(this->friendReady, Status::WAITING, mutexF);
			//umiesc sie w sekwencji Friends
			{
				std::lock_guard<std::mutex> lk(mutexF);
				this->friendsQ.insert(DebaterRep(this->id, this->clock));
				//addSorted(DebaterRep(id, clock), this->friendsQ);
			}
			
			print("Added himself to friends");
			std::cout << this->id << " - Friends size " << friendsQ.size() << std::endl;
			//Wyslij jakos REQ[F]

			broadcastMessage(Type::FRIENDS, SubType::REQ);
			print("Sent friend messages");
			//Czekamy az w¹tek komunikacyjny otrzyma odpowiednia iloœæ ackow
			{
				std::unique_lock<std::mutex> lk(mutexF);
				checkpointF.wait(lk, [this] {return this->friendReady == Status::TAKEN; });
			}
			print("Finished waiting for ack");

			print(string_format("Friends size %d", friendsQ.size()));
			{
			}
			{
				std::lock_guard<std::mutex> lk(mutexF);
				if (friendsQ.size() != sizeDebaters)
					for (auto & debater : friendsQ)
						print(string_format("id - %d clock - %d", debater.id, debater.clock));
				this->position = findPosition(friendsQ);
			}
			print(string_format("your position %d", position));
			//Zaleznie od pozycji w Friends:
			//wyslij Invite
			//broadcast REQ[R], sprawdz swoja pozycje w kolejce Rqueue, jezeli sie nie dostajesz to czekaj za watkiem komunikacyjnym
			//nieparzysta
			if (position % 2 == 1)
			{
				{
					std::lock_guard<std::mutex> lk(mutexF);
					//Usun wszystkie wpisy w Friendsach wyzsze od siebie i siebie
					//zwraca id poprzedniego i twoja pozycje 
					std::pair<int, int> idpos = deleteUntil(friendsQ);
					this->partner = idpos.first;
					print("Sends invite to his friend " + std::to_string(this->partner));
					sendMessage(partner, Type::FRIENDS, SubType::INVITE);
				}

				print("Starts searching for room");
				broadcastMessage(Type::ROOMS, SubType::REQ);

				int pos = roomsAmount;
				{
					std::lock_guard<std::mutex> lk(mutexR);
					this->friendsQ.insert(DebaterRep(this->id, this->clock));
					pos = findPosition(this->roomsQ);

				}
				safeChange(roomTaken, Status::WAITING, mutexR);
				//wchodzisz
				if (pos < roomsAmount)
				{
					print("Gets room instantly");
					safeChange(roomTaken,Status::TAKEN, mutexR);
					
					// 
				}
				//nie wchodzisz i czekasz
				else
				{
					print("Waits for room");
					std::unique_lock<std::mutex> lk(mutexR);
					checkpointR.wait(lk, [this] {return this->roomTaken == Status::TAKEN; });
					print("Gets room after waiting");
				}
			}

			//czekaj za watkiem komunikacyjnym,
			//parzysta
			else
			{
				print("Waits for his friend");
				{
					std::unique_lock<std::mutex> lk(mutexR);
					checkpointR.wait(lk, [this] {return this->inviteReady == Status::TAKEN; });
				}

				//w watku komunikacyjnym partnera ustawic, usuwamy tez do partnera
				
				{
					std::lock_guard<std::mutex> lk(mutexR);
					print("Gets invite from his friend" + std::to_string(this->partner));
					deleteUntil(friendsQ, this->partner);
				}
				print("Deleted from friends queue");
			}

			//losuj jeden z trzech wyborów
			this->choice = rand() % 3;
			//broadcast REQ[M/P/G], sprawdz swoj¹ pozycje w kolejce, jezeli sie nie dostajesz to czekaj za w¹tkiem komunikacyjnym

			if (choice == 0)
				itemLimit = mAmount;
			else if (choice == 1)
				itemLimit = pAmount;
			else if (choice == 2)
				itemLimit = gAmount;

			int pos = itemLimit;

			{
				std::lock_guard<std::mutex> lk(mutexCh);
				this->itemQ[choice].insert(DebaterRep(this->id, this->clock));
				pos = findPosition(itemQ[choice]);
			}

			broadcastMessage((Type)(choice+2), SubType::REQ);
			safeChange(chTaken, Status::WAITING, mutexCh);
			//wchodzisz
			if (pos < itemLimit)
			{
				print("Gets item instantly");
				safeChange(chTaken, Status::TAKEN, mutexCh);
			}
			//nie wchodzisz i czekasz
			else
			{
				print("Waits for item" + getSubType(choice + 3));
				std::unique_lock<std::mutex> lk(mutexCh);
				checkpointCh.wait(lk, [this] {return this->chTaken == Status::TAKEN; });
				print("Gets item" + getSubType(choice + 3));
			}

			//wyslij RD, sprawdz czy dostales RD od partnera
			sendMessage(partner, Type::FRIENDS, (SubType) (choice + 3));
			print("Waits for RD");
			{
				std::unique_lock<std::mutex> lk(mutexCh);
				checkpointCh.wait(lk, [this] {return this->partnerChoice != -1; });
			}


			print("Got RD - " + std::to_string(partnerChoice));

			print("Starts discussion");

			wait(randomTime(WAITMIN * 2, WAITMAX * 2));

			print("Ends discussion");
			

			bool winner = false;
			//czekaj, porownaj swoje RD i RD partnera
			//ZMIENIC TO
			if (choice > partnerChoice)
			{
				winner = true;

			}


			//wyslij wiadomosci ACK[S] i ACK[M/P/G]
			safeChange(partnerChoice, -1, mutexCh);

			broadcastMessage((Type)(choice + 2), SubType::ACK);

			safeChange(chTaken, Status::FREE, mutexCh);
			if (roomTaken == Status::TAKEN)
				broadcastMessage(Type::ROOMS, SubType::ACK);
			safeChange(roomTaken, Status::FREE, mutexR);

			safeChange(inviteReady, Status::FREE, mutexR);

			safeChange(friendReady, Status::FREE, mutexF);

			//jezeli wygrales, czekaj
			if (winner)
				wait(randomTime(WAITMIN, WAITMAX));

			//jezeli przegrales, nic

			// koniec

		}
		
	}

private:

	void communicate()
	{
		MPI_Status status;
		while (1)
		{
			MPI_Recv(&packet, 1, MPI_msgStruct, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
			print(string_format("Got message - %d %d %s %s", packet.id, packet.ts, getType(packet.type), getSubType(packet.subtype)));
			DebaterRep sender(packet.id, packet.ts);
			int temp = std::max(packet.ts, this->clock);
			this->clock = temp + 1;
			switch ((Type)packet.type) 
			{

			//FRIENDS
			case Type::FRIENDS:
				switch ((SubType)packet.subtype)
				{
				case SubType::REQ:
					{
						std::lock_guard<std::mutex> lk(mutexF);
						this->friendsQ.insert(sender);
					}
					sendMessage(packet.id, Type::FRIENDS, SubType::ACK);
					break;

				case SubType::ACK:

					{
						std::lock_guard<std::mutex> lk(mutexF);
						ackFriendCounter++;
						if (ackFriendCounter >= sizeDebaters - 1)
						{
							friendReady = Status::TAKEN;
						}
					}
					checkpointF.notify_all();
					break;

				case SubType::INVITE:
					{
						std::lock_guard<std::mutex> lk(mutexR);
						partner = packet.id;
						inviteReady = Status::TAKEN;
					}
					checkpointR.notify_all();
					break;
				default:
					{
						std::lock_guard<std::mutex> lk(mutexCh);
						partnerChoice = packet.subtype - 3;
					}
					checkpointCh.notify_all();
					break;
				}
				break;

			//ROOMS
			case Type::ROOMS:
				switch ((SubType)packet.subtype)
				{
				case SubType::REQ:
					{
						std::lock_guard<std::mutex> lk(mutexR);
						roomsQ.insert(sender);
						if (friendReady == Status::FREE)
							sendMessage(sender.id, Type::ROOMS, SubType::ACK);
					}
					break;
				case SubType::ACK:
					{
						std::lock_guard<std::mutex> lk(mutexR);
						roomsQ.insert(sender);
					}
					break;
				}
				{
					std::lock_guard<std::mutex> lk(mutexR);
					int pos = findPosition(roomsQ);
					if (pos < roomsAmount)
						roomTaken = Status::TAKEN;
				}
				checkpointR.notify_one();
				break;
			//M, P, G
			default:
				switch ((SubType)packet.subtype)
				{
				case SubType::REQ:
					{
						std::lock_guard<std::mutex> lk(mutexCh);
						itemQ[packet.type - 2].insert(sender);
						if (this->chTaken == Status::FREE || this->choice != packet.type - 2)
							sendMessage(sender.id, (Type)packet.type, SubType::ACK);
					}
					break;
				case SubType::ACK:
					{
						std::lock_guard<std::mutex> lk(mutexCh);
						itemQ[packet.type - 2].insert(sender);
					}
					break;
				}

				// zmienic to potem
				{
					std::lock_guard<std::mutex> lk(mutexCh);
					int pos = findPosition(itemQ[packet.type - 2]);
					if (pos < itemLimit)
						chTaken = Status::TAKEN;
				}
				checkpointCh.notify_one();
				break;
			}
		}
	}

	void safeChange(int & oldValue,const int & value, std::mutex & mutex)
	{
		std::lock_guard<std::mutex> lock(mutex);
		oldValue = value;
	}

	void safeChange(bool & oldValue, const bool & value, std::mutex & mutex)
	{
		std::lock_guard<std::mutex> lock(mutex);
		oldValue = value;
	}

	void safeChange(Status & oldValue, const Status & value, std::mutex & mutex)
	{
		std::lock_guard<std::mutex> lock(mutex);
		oldValue = value;
	}

	void broadcastMessage(Type type, SubType subtype)
	{
		MsgStructure msg(this->id, this->clock, type, subtype);
		print(string_format("Broadcasting message - %d %s %s ", msg.ts, getType(msg.type), getSubType(msg.subtype)));
		for (auto & debater : otherDebaters)
		{
			if (debater.id == this->id) 
				continue;
			_sendMessage(debater.id, &msg);
		}
		this->clock++;
	}

	void sendMessage(int destination, Type type, SubType subtype)
	{
		MsgStructure msg(this->id, this->clock, type, subtype);
		print(string_format("Sending message - %d %d %s %s", destination, msg.ts, getType(msg.type), getSubType(msg.subtype)));
		_sendMessage(destination, &msg);
		this->clock++;
	}

	//nie zwieksza clocka
	void _sendMessage(int destination, MsgStructure * msg)
	{
		wait(MESSAGEWAIT);
		MPI_Send(msg, 1, MPI_msgStruct, destination, 0, MPI_COMM_WORLD);
	}

	void addSorted(const DebaterRep & rep, std::list<DebaterRep> & list)
	{
		if (list.empty())
			list.push_back(rep);
		else 
		{
			for (auto it = list.begin(); it != list.end(); it++)
			{
				if (it->id == rep.id)
				{
					list.erase(it);
					break;
				}
			}

			for (auto it = list.begin(); it != list.end(); it++)
			{
				auto & value = *it;
				if (value.clock < rep.clock)
				{
					list.insert(it, rep);
					return;
				}
				if (value.clock == rep.clock)
					if (value.id < rep.id)
					{
						list.insert(it, rep);
						return;
					}
			}
			list.insert(list.end(), rep);
			return;
		}
	}

	//zwraca id, position
	std::pair<int, int> deleteUntil(std::set<DebaterRep, cmp> & list, int toId=-1)
	{
		int beforeId = -1;
		int pos = 0;
		if (toId == -1)
			toId = this->id;
		if (list.empty())
			return std::pair<int,int>(-1,-1);
		else
		{
			auto value = list.begin();
			while (1)
			{
				if (value->id == toId)
				{
					//wyrzucamy tez partnera
					list.erase(list.begin(), value);
					return std::pair<int, int>(beforeId, pos);
				}
				pos++;
				beforeId = value->id;
				value++;
			}

		}
	}

	//zwraca position
	/*
	int findPosition(const std::set<DebaterRep, cmp> & set)
	{
		auto iter = set.find(DebaterRep(this->id, this->clock));
		if (iter == set.end())
		{
			print("Find position failed");
			return -1;
		}

		auto distance = std::distance(set.begin(), iter);
		return distance;
	}
	*/
	int findPosition(const std::set<DebaterRep, cmp> & set)
	{
		int i = 0;
		auto iter = set.begin();
		while (iter != set.end())
		{
			if (iter->id == this->id)
				return i;
			iter++;
			i++;

		}
		return -1;
	}
	int randomTime(int start, int end)
	{
		return rand() % (end - start) + start;
	}

	void wait(int time)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(time));
	}

	void print(const std::string & str)
	{
		std::lock_guard<std::mutex> lk(mutexPrint);
		std::cout << this->id << " - " << str << std::endl;
	}

	std::string getType(int id)
	{
		std::string result[] = { "Friends", "Rooms", "M", "P", "G" };
		return result[id];
	}

	std::string getSubType(int id)
	{
		std::string result[] = { "Req", "Ack", "Invite", "chM", "chP", "chG" };
		return result[id];
	}
};