#pragma once
#include <thread>
#include "MsgStructure.h"
class Debater
{
public:
	Debater(int id, int size, MPI_Datatype msgStruct)
	{
		this->id = id;
		this->size = size;
		this->MPI_msgStruct = msgStruct;
	}

	void run()
	{
		std::thread t(&communicate, this);
		t.detach();
		while (1)
		{
			//wait
			//zeruj liczbe ACK[F] i wszystkie inne zmienne

			//umiesc sie w sekwencji Friends
			//Wyslij jakos REQ[F]
			//Czekamy az w¹tek komunikacyjny otrzyma odpowiednia iloœæ ackow
			
			//wysylamy wszystkie zalegle Acki



			//Usun wszystkie wpisy w Friendsach wyzsze od siebie i siebie
			//Zaleznie od pozycji w Friends:


			//wyslij Invite
			//broadcast REQ[S], sprawdz swoja pozycje w kolejce Rqueue, jezeli sie nie dostajesz to czekaj za watkiem komunikacyjnym


			//czekaj za watkiem komunikacyjnym,


			//losuj jeden z trzech wyborów

			//broadcast REQ[M/P/G], sprawdz swoj¹ pozycje w kolejce, jezeli sie nie dostajesz to czekaj za w¹tkiem komunikacyjnym

			//wyslij RD, sprawdz czy dostales RD od partnera


			//czekaj, porownaj swoje RD i RD partnera

			//wyslij wiadomosci ACK[S] i ACK[M/P/G]

			//jezeli wygrales, czekaj

			//jezeli przegrales, nic

			// koniec



			

			


		}
	}

	void communicate()
	{
		MPI_Status status;
		while (1)
		{
			MPI_Recv(&packet, 1, MPI_msgStruct, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
		}
	}
private:





private:
	int id;
	int size;
	MPI_Datatype MPI_msgStruct;
	MsgStructure packet;

};

