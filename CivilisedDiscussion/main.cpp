#include "main.h"


int main(int argc, char **argv)
{
	/* Tworzenie w¹tków, inicjalizacja itp */
#ifdef _WIN32
	addColours();
#endif
	int provided;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	check_thread_support(provided);
	initStruct();

	int rank, sizeDebaters;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &sizeDebaters);
	srand(rank);

	Debater debater(rank, sizeDebaters, MPI_structure);
	
	//-rAmount 5 -mAmount 5 
	debater.run();
	
	finish();
	return 0;
}

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
	case MPI_THREAD_MULTIPLE: printf("Pelne wsparcie dla watkow\n"); /* tego chcemy. Wszystkie inne powoduj¹ problemy */
		break;
	default: printf("Nikt nic nie wie\n");
	}
}

void initStruct()
{
	const int nitems = 4; /* bo packet_t ma cztery pola */
	int       blocklengths[4] = { 1,1,1,1 };
	MPI_Datatype typy[4] = { MPI_INT, MPI_INT, MPI_INT, MPI_INT };

	MPI_Aint     offsets[4];
	offsets[0] = offsetof(MsgStructure, ts);
	offsets[1] = offsetof(MsgStructure, id);
	offsets[2] = offsetof(MsgStructure, type);
	offsets[3] = offsetof(MsgStructure, subtype);

	MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_structure);
	MPI_Type_commit(&MPI_structure);
}

void finish()
{
	MPI_Type_free(&MPI_structure);
	MPI_Finalize();
}
