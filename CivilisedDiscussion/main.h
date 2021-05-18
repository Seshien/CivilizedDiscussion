#pragma once
#include <thread>
#include <mpi.h>
#include "MsgStructure.h"
#include "Debater.h"

MPI_Datatype MPI_structure;


void check_thread_support(int provided)
{
	printf("THREAD SUPPORT: chcemy %d. Co otrzymamy?\n", provided);
	switch (provided) {
	case MPI_THREAD_SINGLE:
		printf("Brak wsparcia dla w¹tków, koñczê\n");
		/* Nie ma co, trzeba wychodziæ */
		fprintf(stderr, "Brak wystarczaj¹cego wsparcia dla w¹tków - wychodzê!\n");
		MPI_Finalize();
		exit(-1);
		break;
	case MPI_THREAD_FUNNELED:
		printf("tylko te w¹tki, ktore wykonaly mpi_init_thread mog¹ wykonaæ wo³ania do biblioteki mpi\n");
		break;
	case MPI_THREAD_SERIALIZED:
		/* Potrzebne zamki wokó³ wywo³añ biblioteki MPI */
		printf("tylko jeden watek naraz mo¿e wykonaæ wo³ania do biblioteki MPI\n");
		break;
	case MPI_THREAD_MULTIPLE: printf("Pe³ne wsparcie dla w¹tków\n"); /* tego chcemy. Wszystkie inne powoduj¹ problemy */
		break;
	default: printf("Nikt nic nie wie\n");
	}
}

void initStruct()
{
	const int nitems = 3; /* bo packet_t ma trzy pola */
	int       blocklengths[3] = { 1,1,1 };
	MPI_Datatype typy[4] = { MPI_INT, MPI_INT, MPI_INT, MPI_INT };

	MPI_Aint     offsets[4];
	offsets[0] = offsetof(MsgStructure, ts);
	offsets[1] = offsetof(MsgStructure, rank);
	offsets[2] = offsetof(MsgStructure, type);
	offsets[3] = offsetof(MsgStructure, subtype);

	MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_structure);
	MPI_Type_commit(&MPI_structure);
}

void finish()
{

}

int main(int argc, char **argv)
{
	/* Tworzenie w¹tków, inicjalizacja itp */
	int provided;
	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	check_thread_support(provided);
	initStruct();

	int rank, size;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &size);
	srand(rank);

	Debater debater = Debater(rank, size, MPI_structure);

	debater.run();

	finish();
	return 0;
}