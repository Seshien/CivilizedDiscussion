#include "main.h"


int main(int argc, char **argv)
{
	/* Tworzenie w¹tków, inicjalizacja itp */
	int provided;

	MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
	check_thread_support(provided);
	initStruct();

	int rank, sizeDebaters;
	MPI_Comm_rank(MPI_COMM_WORLD, &rank);
	MPI_Comm_size(MPI_COMM_WORLD, &sizeDebaters);
	srand(rank);

	Debater debater(rank, sizeDebaters, MPI_structure);
	debater.run();
	finish();
	return 0;
}