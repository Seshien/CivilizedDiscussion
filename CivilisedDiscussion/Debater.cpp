#include "Debater.h"

Debater::Debater(int id, int sizeDebaters, MPI_Datatype msgStruct)
{
	this->id = id;
	this->sizeDebaters = sizeDebaters;
	this->MPI_msgStruct = msgStruct;
	this->clock = 0;

	//stworz innych debaterow tam gdzie jest to konieczne
	for (int i = 0; i<3; i++)
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

void Debater::run()
{
	printColour("Start Running", true);
	std::thread t(&Debater::communicate, this);
	t.detach();

	while (1)
	{
		//wait
		wait(randomTime(WAITMIN, WAITMAX));
		searchForPartner();

		//Zaleznie od pozycji w Friends:
		//wyslij Invite
		//broadcast REQ[R], sprawdz swoja pozycje w kolejce Rqueue, jezeli sie nie dostajesz to czekaj za watkiem komunikacyjnym
		//nieparzysta
		if (this->position % 2 == 1)
		{
			searchRoom();
		}

		//czekaj za watkiem komunikacyjnym,
		//parzysta
		else
		{
			waitForRoom();
		}


		searchItem();




		printColour("Starts discussion with " + std::to_string(this->partner), true);

		wait(DISCUSSIONWAIT);

		printColour("Ends discussion with " + std::to_string(this->partner), true);


		
		bool winner = getResult();

		//wyslij wiadomosci ACK[S] i ACK[M/P/G]
		{
			std::lock_guard<std::mutex> lk(mutexCh);
			partnerChoice = -1;
			itemTaken = Status::FREE;
		}

		broadcastMessage((Type)(this->choice + 2), SubType::ACK);


		if (roomTaken == Status::TAKEN)
			broadcastMessage(Type::ROOMS, SubType::ACK);

		{
			std::lock_guard<std::mutex> lk(mutexR);
			roomTaken = Status::FREE;
			inviteTaken = Status::FREE;
		}

		{
			std::lock_guard<std::mutex> lk(mutexF);
			friendTaken = Status::FREE;
		}


		//jezeli wygrales, czekaj dodatkowy czas
		if (winner)
			wait(randomTime(WAITMIN, WAITMAX));

		//jezeli przegrales, nic

		// koniec

	}

}

void Debater::communicate()
{
	MPI_Status status;
	while (1)
	{
		MPI_Recv(&packet, 1, MPI_msgStruct, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		printColour(string_format("Got message - %d %d %s %s", packet.id, packet.ts, getType(packet.type), getSubType(packet.subtype)));
		DebaterRep sender(packet.id, packet.ts);
		{
			std::lock_guard<std::mutex> lk(mutexCl);
			int temp = this->clock;
			if (packet.ts > this->clock)
				temp = packet.ts;
			this->clock = temp + 1;
			safeAdd(sender, otherDebaters);
		}

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
			if ((SubType)packet.subtype == SubType::REQ && this->itemTaken == Status::FREE)
				sendMessage(sender.id, (Type)packet.type, SubType::ACK);
			if (this->itemTaken == Status::WAITING && findPosition(*queue) < itemAmount[this->choice])
				itemTaken = Status::TAKEN;
		}
		checkpointCh.notify_one();
		break;
		}
	}
}

void Debater::searchForPartner()
{
	printColour("Start searching for partner", true);
	//zeruj liczbe ACK[F] i wszystkie inne zmienne
	//umiesc sie w sekwencji Friends
	{
		std::lock_guard<std::mutex> lk(mutexF);
		ackFriendCounter = 0;
		this->friendTaken = Status::WAITING;
		safeAdd(DebaterRep(this->id, this->clock), this->friendsQ, true);
	}

	printColour("Added himself to friends");
	printColour("Before Friends size " + std::to_string(friendsQ.size()));
	//Wyslij jakos REQ[F]

	broadcastMessage(Type::FRIENDS, SubType::REQ);
	printColour("Sent friend messages");
	//Czekamy az w¹tek komunikacyjny otrzyma odpowiednia iloœæ ackow
	{
		std::unique_lock<std::mutex> lk(mutexF);
		checkpointF.wait(lk, [this] {return this->friendTaken == Status::TAKEN; });
	}
	printColour("Finished waiting for ack", true);

	printColour(string_format("After Friends size %d", friendsQ.size()));
	{
		std::lock_guard<std::mutex> lk(mutexF);
		printColour("Before DELETE");
		for (auto & debater : friendsQ)
			printColour(string_format("id - %d clock - %d", debater.id, debater.clock));
		printColour("\n");
		this->position = findPosition(friendsQ);
	}
	printColour(string_format("your position %d", position));
}

void Debater::searchRoom()
{
	{
		std::lock_guard<std::mutex> lk(mutexF);
		//Usun wszystkie wpisy w Friendsach wyzsze od siebie i siebie
		//zwraca id poprzedniego i twoja pozycje 
		this->partner = deleteUntil(friendsQ);
		printColour("Send invite to his partner " + std::to_string(this->partner), true);
		sendMessage(partner, Type::FRIENDS, SubType::INVITE);
	}

	printColour("Start searching for room", true);


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
		printColour("Gets room instantly", true);
		std::lock_guard<std::mutex> lk(mutexR);
		roomTaken = Status::TAKEN;

		// 
	}
	//nie wchodzisz i czekasz
	else
	{
		printColour("Waits for room", true);
		std::unique_lock<std::mutex> lk(mutexR);
		checkpointR.wait(lk, [this] {return this->roomTaken == Status::TAKEN; });
		printColour("Gets room after waiting", true);
	}
}

void Debater::waitForRoom()
{
	printColour("Waits for invite from partner", true);
	if (this->inviteTaken != Status::TAKEN)
	{
		std::unique_lock<std::mutex> lk(mutexR);
		checkpointR.wait(lk, [this] {return this->inviteTaken == Status::TAKEN; });
	}

	//w watku komunikacyjnym partnera ustawic, usuwamy tez do partnera

	{
		std::lock_guard<std::mutex> lk(mutexF);
		printColour("Gets invite from his partner " + std::to_string(this->partner), true);
		int temp = deleteUntil(friendsQ, true);
		printColour("Deleted from friends queue to id " + std::to_string(temp));
	}
}

void Debater::searchItem()
{
	//losuj jeden z trzech wyborów
//broadcast REQ[M/P/G], sprawdz swoj¹ pozycje w kolejce, jezeli sie nie dostajesz to czekaj za w¹tkiem komunikacyjnym
	int pos = -1;
	{
		std::lock_guard<std::mutex> lk(mutexCh);
		this->choice = rand() % 3;
		safeAdd(DebaterRep(this->id, this->clock), this->itemQ[choice]);
		pos = findPosition(this->itemQ[choice]);
		this->itemTaken = Status::WAITING;
		broadcastMessage((Type)(choice + 2), SubType::REQ);
	}

	//wchodzisz
	if (pos < itemAmount[this->choice])
	{
		printColour("Gets item instantly " + getSubType(choice + 3), true);
		std::lock_guard<std::mutex> lk(mutexCh);
		this->itemTaken = Status::TAKEN;
	}
	//nie wchodzisz i czekasz
	else
	{
		printColour("Waits for item " + getSubType(choice + 3), true);
		std::unique_lock<std::mutex> lk(mutexCh);
		checkpointCh.wait(lk, [this] {return this->itemTaken == Status::TAKEN; });
		printColour("Gets item " + getSubType(choice + 3), true);
	}
}

void Debater::waitForRD()
{
	//wyslij RD, sprawdz czy dostales RD od partnera
	sendMessage(partner, Type::FRIENDS, (SubType)(choice + 3));
	if (partnerChoice == -1)
	{
		printColour("Waits for RD from " + std::to_string(this->partner), true);
		std::unique_lock<std::mutex> lk(mutexCh);
		checkpointCh.wait(lk, [this] {return this->partnerChoice != -1; });
	}
	else
		printColour("Gets RD instantly from " + std::to_string(this->partner), true);
}

bool Debater::getResult()
{
	//czekaj, porownaj swoje RD i RD partnera
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
		return true;
	else
		return false;

}

void Debater::broadcastMessage(Type type, SubType subtype)
{
	MsgStructure msg(this->id, this->clock, type, subtype);
	printColour(string_format("Broadcasting message - %d %s %s ", msg.ts, getType(msg.type), getSubType(msg.subtype)));

	{
		std::lock_guard<std::mutex> lk(mutexCl);
		this->clock++;
		for (auto & debater : otherDebaters)
		{
			if (debater.id == this->id)
				continue;
			_sendMessage(debater.id, &msg);
		}

	}

}

void Debater::sendMessage(int destination, Type type, SubType subtype)
{
	MsgStructure msg(this->id, this->clock, type, subtype);
	printColour(string_format("Sending message - %d %d %s %s", destination, msg.ts, getType(msg.type), getSubType(msg.subtype)));
	{
		std::lock_guard<std::mutex> lk(mutexCl);
		this->clock++;
		_sendMessage(destination, &msg);

	}
}

//nie zwieksza clocka

void Debater::_sendMessage(int destination, MsgStructure * msg)
{
	//wait(MESSAGEWAIT);
	MPI_Send(msg, 1, MPI_msgStruct, destination, 0, MPI_COMM_WORLD);
}

void Debater::safeAdd(const DebaterRep & rep, std::list<DebaterRep>& list, bool duplicates)
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

//zwraca beforeid

int Debater::deleteUntil(std::list<DebaterRep>& list, bool partner)
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

int Debater::findPosition(const std::list<DebaterRep>& list)
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

int Debater::randomTime(int start, int end)
{
	return rand() % (end - start) + start;
}

void Debater::wait(int time)
{
	std::this_thread::sleep_for(std::chrono::milliseconds(time));
}

void Debater::printColour(const std::string & str, bool important)
{
	if (important || DEBUG_MODE)
	{
		std::lock_guard<std::mutex> lk(mutexPrint);

		std::cout << "\033[" << 31 + this->id % 7 << "m" << this->id << " - " << this->clock << " - " << str << " \x1b[0m" << std::endl;
	}

}

std::string Debater::getType(int id)
{
	std::string result[] = { "Friends", "Rooms", "M", "P", "G" };
	return result[id];
}

std::string Debater::getSubType(int id)
{
	std::string result[] = { "Req", "Ack", "Invite", "chM", "chP", "chG" };
	return result[id];
}
