#pragma once
#include <iostream>
#include <string>
#include <random>
#include <chrono>
#include <thread>
#include <list>
#include <mutex>
#include <condition_variable>
#include "mpi.h"

#include "MsgStructure.h"
#include "DebaterRep.h"
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
		itemQ.push_back(std::list<DebaterRep>());
		itemQ.push_back(std::list<DebaterRep>());
		itemQ.push_back(std::list<DebaterRep>());
		for (int i = 0; i < this->sizeDebaters; i++)
		{
			roomsQ.push_back(DebaterRep(i, 0));
			for (auto & iQ : itemQ)
				iQ.push_back(DebaterRep(i, 0));
			otherDebaters.push_back(DebaterRep(i, 0));
		}


	}

private:
	int id;
	int sizeDebaters;
	int clock;
	int partner, position;
	int roomsAmount = 2, mAmount = 3, pAmount = 3, gAmount = 3;
	int choice, partnerChoice;
	MPI_Datatype MPI_msgStruct;
	MsgStructure packet;
	std::list<DebaterRep> otherDebaters;
	std::list<DebaterRep> friendsQ, roomsQ;
	std::vector<std::list<DebaterRep>> itemQ;
	int ackFriendCounter;
	enum class Status : int {FREE, WAITING, TAKEN};
	Status friendReady, inviteReady, roomTaken, chTaken;
	std::condition_variable checkpointF, checkpointR, checkpointCh;
	std::mutex mutexF, mutexR, mutexCh;

public:

	void run()
	{
		print("Start Running");
		std::thread t(&Debater::communicate, this);
		t.detach();

		while (1)
		{
			
			//wait
			wait(randomTime(1000, 5000));
			print("Start Searching");
			//zeruj liczbe ACK[F] i wszystkie inne zmienne
			safeChange(ackFriendCounter, 0, mutexF);
			safeChange(friendReady, Status::FREE, mutexF);
			//ackFriendCounter = 0;
			//friendReady = false;


			//umiesc sie w sekwencji Friends
			{
				std::lock_guard<std::mutex> lk(mutexF);
				addSorted(DebaterRep(id, clock), this->friendsQ);
			}
			print("Added himself to friends");

			//Wyslij jakos REQ[F]
			safeChange(this->friendReady, Status::WAITING, mutexF);
			broadcastMessage(Type::FRIENDS, SubType::REQ);
			print("Sent friend messages");
			//Czekamy az w¹tek komunikacyjny otrzyma odpowiednia iloœæ ackow
			{
				std::unique_lock<std::mutex> lk(mutexF);
				checkpointF.wait(lk, [this] {return this->friendReady == Status::TAKEN; });
			}
			print("Finished waiting for ack");



			//Usun wszystkie wpisy w Friendsach wyzsze od siebie i siebie
			//zwraca id poprzedniego i twoja pozycje 
			{
				std::lock_guard<std::mutex> lk(mutexF);
				std::pair<int, int> idpos = deleteUntil(friendsQ);
				this->partner = idpos.first;
				this->position = idpos.second;
			}

			std::cout << this->id << " - Partner id, your position " << partner << " " << position<<::std::endl;
			//Zaleznie od pozycji w Friends:
			//wyslij Invite
			//broadcast REQ[R], sprawdz swoja pozycje w kolejce Rqueue, jezeli sie nie dostajesz to czekaj za watkiem komunikacyjnym
			//nieparzysta
			if (position % 2 == 1)
			{
				print("Starts searching for room");
				sendMessage(partner, Type::FRIENDS, SubType::INVITE);


				broadcastMessage(Type::ROOMS, SubType::REQ);

				int pos = roomsAmount;
				{
					std::lock_guard<std::mutex> lk(mutexR);
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
				std::unique_lock<std::mutex> lk(mutexR);
				checkpointR.wait(lk, [this] {return this->inviteReady == Status::TAKEN; });
				//w watku komunikacyjnym partnera ustawic
				print("Gets invite from his friend");
			}

			//losuj jeden z trzech wyborów
			choice = rand() % 2;
			//broadcast REQ[M/P/G], sprawdz swoj¹ pozycje w kolejce, jezeli sie nie dostajesz to czekaj za w¹tkiem komunikacyjnym
			int pos = mAmount;

			{
				std::lock_guard<std::mutex> lk(mutexCh);
				pos = findPosition(itemQ[choice]);
			}
			broadcastMessage((Type)(choice+2), SubType::REQ);
			safeChange(chTaken, Status::WAITING, mutexCh);
			//wchodzisz
			if (pos < mAmount)
			{
				print("Gets item instantly");
				safeChange(chTaken, Status::TAKEN, mutexCh);
			}
			//nie wchodzisz i czekasz
			else
			{
				print("Waits for item");
				std::unique_lock<std::mutex> lk(mutexCh);
				checkpointCh.wait(lk, [this] {return this->chTaken == Status::TAKEN; });
				print("Gets item");
			}

			//wyslij RD, sprawdz czy dostales RD od partnera
			sendMessage(partner, Type::FRIENDS, (SubType) (choice + 3));
			print("Waits for RD");
			{
				std::unique_lock<std::mutex> lk(mutexCh);
				checkpointCh.wait(lk, [this] {return this->partnerChoice != -1; });
			}
			print("Game");
			wait(randomTime(10000, 50000));

			bool winner = false;
			//czekaj, porownaj swoje RD i RD partnera
			//ZMIENIC TO
			if (choice > partnerChoice)
			{
				winner = true;
			}


			//wyslij wiadomosci ACK[S] i ACK[M/P/G]
			broadcastMessage((Type)(choice + 2), SubType::ACK);
			safeChange(chTaken, Status::FREE, mutexCh);
			if (roomTaken == Status::TAKEN)
				broadcastMessage(Type::ROOMS, SubType::ACK);
			safeChange(roomTaken, Status::FREE, mutexR);

			//jezeli wygrales, czekaj
			if (winner)
				wait(randomTime(1000, 5000));

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
			DebaterRep sender(packet.id, packet.ts);
			int temp = std::max(packet.ts, this->clock);
			this->clock = temp + 1;
			switch ((Type)packet.type) 
			{
			case Type::FRIENDS:
				switch ((SubType)packet.subtype)
				{
				case SubType::REQ:
					{
						std::lock_guard<std::mutex> lk(mutexF);

						addSorted(sender, this->friendsQ);
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
					checkpointF.notify_all();
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

			case Type::ROOMS:
				switch ((SubType)packet.subtype)
				{
				case SubType::REQ:
					if (friendReady == Status::FREE)
					{
						std::lock_guard<std::mutex> lk(mutexR);
						this->addSorted(sender, roomsQ);
						sendMessage(sender.id, Type::ROOMS, SubType::ACK);
					}
					else
					{
						std::lock_guard<std::mutex> lk(mutexR);
						this->addSorted(sender, roomsQ);
					}
					break;
				case SubType::ACK:
					{
						std::lock_guard<std::mutex> lk(mutexR);
						this->addSorted(sender, roomsQ);
					}
					break;
				}
				
				{
					std::lock_guard<std::mutex> lk(mutexR);
					int pos = findPosition(roomsQ);
					if (pos < roomsAmount)
					{
						roomTaken = Status::TAKEN;
					}
				}
				checkpointR.notify_one();
				break;
			default:
				switch ((SubType)packet.subtype)
				{
				case SubType::REQ:
					if (this->chTaken == Status::FREE)
					{
						std::lock_guard<std::mutex> lk(mutexCh);
						this->addSorted(sender, itemQ[packet.type - 3]);
						sendMessage(sender.id, (Type)packet.type, SubType::ACK);
					}
					else
					{
						std::lock_guard<std::mutex> lk(mutexCh);
						this->addSorted(sender, itemQ[packet.type - 3]);
					}
					break;
				case SubType::ACK:
				{
					std::lock_guard<std::mutex> lk(mutexCh);
					this->addSorted(sender, itemQ[packet.type - 3]);
				}
				break;
				}
				// zmienic to potem
				{
					std::lock_guard<std::mutex> lk(mutexCh);
					int pos = findPosition(itemQ[packet.type - 3]);
					if (pos < mAmount)
					{
						chTaken = Status::TAKEN;
					}
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
		_sendMessage(destination, &msg);
		this->clock++;
	}

	//nie zwieksza clocka
	void _sendMessage(int destination, MsgStructure * msg)
	{
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
					break;
				}
			}
		}
	}

	//zwraca id, position
	std::pair<int, int> deleteUntil(std::list<DebaterRep> & list)
	{
		int beforeId = -1;
		int pos = -1;
		if (list.empty())
			return std::pair<int,int>(-1,-1);
		else
		{
			for ()
			auto value = list.back();
			list.pop_back();
			if (value.id == this->id)
				return std::pair<int,int>(beforeId,pos);
			pos++;
			beforeId = value.id;
		}
	}

	//zwraca position

	int findPosition(const std::list<DebaterRep> & list)
	{
		int pos = 0;
		for (auto iter = list.rbegin(); iter != list.rend(); iter++)
		{
			if (iter->id == this->id)
				return pos;
			pos++;
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
		std::cout << this->id << " - " << str << std::endl;
	}
};