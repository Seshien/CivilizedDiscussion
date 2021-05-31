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


#define RSIZE 3
#define MSIZE 6
#define PSIZE 6
#define GSIZE 6
#define WAITMIN 5000
#define WAITMAX 10000
#define MESSAGEWAIT 1000
#define DEBUG_MODE false

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
		for (int i=0; i<3;i++)
			itemQ.push_back(std::list<DebaterRep>());

		for (int i = 0; i < this->sizeDebaters; i++)
		{
			auto debRep = DebaterRep(i, 0);
			safeAdd(debRep, roomsQ);
			for (auto & iQ : itemQ)
				safeAdd(debRep, iQ);
			safeAdd(debRep, otherDebaters);
		}


	}

private:
	int id;
	int sizeDebaters;
	int clock;
	int partner, position;
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
	int ackFriendCounter;
	enum class Status : int {FREE, WAITING, TAKEN};
	Status friendTaken, inviteTaken, roomTaken, choiceTaken;
	std::condition_variable checkpointF, checkpointR, checkpointCh;
	std::mutex mutexF, mutexR, mutexCh, mutexPrint;

public:

	void run()
	{
		print("Start Running", true);
		std::thread t(&Debater::communicate, this);
		t.detach();

		while (1)
		{
			//wait
			wait(randomTime(WAITMIN, WAITMAX));
			print("Start searching for partner", true);
			//zeruj liczbe ACK[F] i wszystkie inne zmienne
			//umiesc sie w sekwencji Friends
			{
				std::lock_guard<std::mutex> lk(mutexF);
				ackFriendCounter = 0;
				this->friendTaken = Status::WAITING;
				safeAdd(DebaterRep(this->id, this->clock), this->friendsQ, true);
			}
			
			print("Added himself to friends");
			print("Before Friends size " + std::to_string(friendsQ.size()));
			//Wyslij jakos REQ[F]

			broadcastMessage(Type::FRIENDS, SubType::REQ);
			print("Sent friend messages");
			//Czekamy az w¹tek komunikacyjny otrzyma odpowiednia iloœæ ackow
			{
				std::unique_lock<std::mutex> lk(mutexF);
				checkpointF.wait(lk, [this] {return this->friendTaken == Status::TAKEN; });
			}
			print("Finished waiting for ack", true);

			print(string_format("After Friends size %d", friendsQ.size()));
			{
				std::lock_guard<std::mutex> lk(mutexF);
				print("Before DELETE");
				for (auto & debater : friendsQ)
					print(string_format("id - %d clock - %d", debater.id, debater.clock));
				print("\n");
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
					this->partner = deleteUntil(friendsQ);
					print("Send invite to his friend " + std::to_string(this->partner), true);
					sendMessage(partner, Type::FRIENDS, SubType::INVITE);
				}

				print("Start searching for room", true);


				int pos = roomsAmount;
				{
					std::lock_guard<std::mutex> lk(mutexR);
					safeAdd(DebaterRep(this->id, this->clock), this->roomsQ);
					pos = findPosition(this->roomsQ);
					roomTaken = Status::WAITING;
					broadcastMessage(Type::ROOMS, SubType::REQ);

				}
				//wchodzisz
				if (pos < roomsAmount)
				{
					print("Gets room instantly", true);
					std::lock_guard<std::mutex> lk(mutexR);
					roomTaken = Status::TAKEN;
					
					// 
				}
				//nie wchodzisz i czekasz
				else
				{
					print("Waits for room", true);
					std::unique_lock<std::mutex> lk(mutexR);
					checkpointR.wait(lk, [this] {return this->roomTaken == Status::TAKEN; });
					print("Gets room after waiting", true);
				}
			}

			//czekaj za watkiem komunikacyjnym,
			//parzysta
			else
			{
				print("Waits for his friend", true);
				if (this->inviteTaken != Status::TAKEN)
				{
					std::unique_lock<std::mutex> lk(mutexR);
					checkpointR.wait(lk, [this] {return this->inviteTaken == Status::TAKEN; });
				}

				//w watku komunikacyjnym partnera ustawic, usuwamy tez do partnera
				
				{
					std::lock_guard<std::mutex> lk(mutexF);
					print("Gets invite from his friend " + std::to_string(this->partner), true);
					int temp = deleteUntil(friendsQ, true);
					print("Deleted from friends queue to id " + std::to_string(temp));
				}
			}

			//losuj jeden z trzech wyborów
			//broadcast REQ[M/P/G], sprawdz swoj¹ pozycje w kolejce, jezeli sie nie dostajesz to czekaj za w¹tkiem komunikacyjnym
			int pos = -1;
			{
				std::lock_guard<std::mutex> lk(mutexCh);
				this->choice = rand() % 3;
				safeAdd(DebaterRep(this->id, this->clock), this->itemQ[choice]);
				pos = findPosition(this->itemQ[choice]);
				this->choiceTaken = Status::WAITING;
				broadcastMessage((Type)(choice + 2), SubType::REQ);
			}

			//wchodzisz
			if (pos < itemAmount[this->choice])
			{
				print("Gets item instantly " + getSubType(choice + 3), true);
				std::lock_guard<std::mutex> lk(mutexCh);
				this->choiceTaken = Status::TAKEN;
			}
			//nie wchodzisz i czekasz
			else
			{
				print("Waits for item " + getSubType(choice + 3), true);
				std::unique_lock<std::mutex> lk(mutexCh);
				checkpointCh.wait(lk, [this] {return this->choiceTaken == Status::TAKEN; });
				print("Gets item " + getSubType(choice + 3), true);
			}

			//wyslij RD, sprawdz czy dostales RD od partnera
			sendMessage(partner, Type::FRIENDS, (SubType) (choice + 3));
			if (partnerChoice == -1)
			{
				print("Waits for RD from " + std::to_string(this->partner), true);
				std::unique_lock<std::mutex> lk(mutexCh);
				checkpointCh.wait(lk, [this] {return this->partnerChoice != -1; });
			}
			else
				print("Gets RD instantly from " + std::to_string(this->partner), true);

			print("Starts discussion with " + std::to_string(this->partner), true);

			wait(randomTime(WAITMIN * 2, WAITMAX * 2));

			print("Ends discussion with " + std::to_string(this->partner), true);
			

			bool winner = false;
			//czekaj, porownaj swoje RD i RD partnera
			//ZMIENIC TO 
			// G >> P >> M >> G
			// 2 >> 1 >> 0 >> 2
			// 2 1
			// 1 0
			// 0 2

			// 1 2
			// 0 1
			// 2 0

			// W debacie slipki pokonuj¹ pinezki, pinezki pokonuj¹ miski, miski pokonuj¹ slipki. Po
			if (choice - partnerChoice == 1 || choice - partnerChoice == -2)
			{
				winner = true;
			}


			//wyslij wiadomosci ACK[S] i ACK[M/P/G]
			safeChange(partnerChoice, -1, mutexCh);

			broadcastMessage((Type)(choice + 2), SubType::ACK);

			safeChange(choiceTaken, Status::FREE, mutexCh);
			if (roomTaken == Status::TAKEN)
				broadcastMessage(Type::ROOMS, SubType::ACK);
			safeChange(roomTaken, Status::FREE, mutexR);

			safeChange(inviteTaken, Status::FREE, mutexR);

			safeChange(friendTaken, Status::FREE, mutexF);

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
					safeAdd(sender, this->friendsQ, true);
				}
				sendMessage(packet.id, Type::FRIENDS, SubType::ACK);
				break;

				case SubType::ACK:
				{
					std::lock_guard<std::mutex> lk(mutexF);
					ackFriendCounter++;
					if (ackFriendCounter >= sizeDebaters - 1)
					{
						friendTaken = Status::TAKEN;
					}
				}
				checkpointF.notify_all();
				break;

				case SubType::INVITE:
				{
					std::lock_guard<std::mutex> lk(mutexR);
					this->partner = packet.id;
					this->inviteTaken = Status::TAKEN;
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
			{
				std::lock_guard<std::mutex> lk(mutexR);
				safeAdd(sender, this->roomsQ);
				if ((SubType)packet.subtype == SubType::REQ && roomTaken == Status::FREE)
					sendMessage(sender.id, Type::ROOMS, SubType::ACK);
				if (this->roomTaken == Status::WAITING && findPosition(roomsQ) < roomsAmount)
					roomTaken = Status::TAKEN;
			}
			checkpointR.notify_one();
			break;
			//M, P, G
			default:
			{
				std::lock_guard<std::mutex> lk(mutexCh);
				std::list<DebaterRep> * queue = &this->itemQ[packet.type - 2];
				safeAdd(sender, *queue);
				if ((SubType)packet.subtype == SubType::REQ && this->choiceTaken == Status::FREE)
					sendMessage(sender.id, (Type) packet.type, SubType::ACK);
				if (this->choiceTaken == Status::WAITING && findPosition(*queue) < itemAmount[this->choice])
					choiceTaken = Status::TAKEN;
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

	void safeAdd(const DebaterRep & rep, std::list<DebaterRep> & list, bool duplicates = false)
	{
		if (list.empty())
			list.push_back(rep);
		else 
		{
			if (!duplicates)
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
				if (rep.clock < it->clock || (rep.clock == it->clock && rep.id < it->id))
				{
					list.insert(it, rep);
					return;
				}
			}
			list.insert(list.end(), rep);
			return;
		}
	}

	//zwraca before id, position
	int deleteUntil(std::list<DebaterRep> & list, bool partner = false)
	{
		int beforeId = -1;
		int toId = this->id;

		if (list.empty())
			return -1;

		else
		{
			auto it = list.begin();
			while (1)
			{
				if (it->id == toId)
				{
					//wyrzucamy tez partnera
					if (partner)
						it++;
					list.erase(list.begin(), ++it);
					return beforeId;
				}
				beforeId = it->id;
				it++;
			}
		}
		return -2;
	}

	int findPosition(const std::list<DebaterRep> & list)
	{
		int i = 0;
		auto iter = list.begin();
		while (iter != list.end())
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

	void print(const std::string & str, bool important=false)
	{
		if (important || DEBUG_MODE)
		{
			std::lock_guard<std::mutex> lk(mutexPrint);
			std::cout << this->id << " - " << str << std::endl;
		}

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