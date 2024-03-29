#include <mpi.h>
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <algorithm>
/* wątki */
#include <pthread.h>

/* sem_init sem_destroy sem_post sem_wait */
//#include <semaphore.h>
/* flagi dla open */
//#include <fcntl.h>

/* boolean */
#define TRUE 1
#define FALSE 0

/* typy wiadomości */
#define FINISH 1
#define APP_MSG 2

char passive = FALSE;

pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clockMut = PTHREAD_MUTEX_INITIALIZER;
int lamportClock = 0;

typedef struct {
    int appdata;
    int clock; /* można zmienić nazwę na bardziej pasujące */
} packet_t;


int sendMsg(int data, int count, MPI_Datatype datatype, int dest, int tag)
{
    packet_t packet;
    packet.appdata = data;
	pthread_mutex_lock(&clockMut);
    lamportClock++;
    packet.clock = lamportClock;
    int res =MPI_Send(&packet , count , datatype , dest , tag,MPI_COMM_WORLD);
    pthread_mutex_unlock(&clockMut);
    return res;
}

int recvMsg(packet_t* buf, int count, MPI_Datatype datatype, int source, int tag,MPI_Comm comm, MPI_Status * status)
 {
    int res = MPI_Recv(buf , count , datatype , source , tag, comm , status);
    pthread_mutex_lock(&clockMut);
    lamportClock = std::max(lamportClock , buf->clock) + 1;
    pthread_mutex_unlock(&clockMut);
    return res;
 }            

             

/* Kod funkcji wykonywanej przez wątek */
void *startFunc(void *ptr)
{
    /* wątek się kończy, gdy funkcja się kończy */
}

int main(int argc, char **argv)
{
    printf("poczatek\n");
    int provided;

    MPI_Init_thread(&argc, &argv,MPI_THREAD_MULTIPLE, &provided);

    printf("THREAD SUPPORT: %d\n", provided);
    switch (provided) {
        case MPI_THREAD_SINGLE: 
            printf("Brak wsparcia dla wątków, kończę\n");
            /* Nie ma co, trzeba wychodzić */
	    fprintf(stderr, "Brak wystarczającego wsparcia dla wątków - wychodzę!\n");
	    MPI_Finalize();
	    exit(-1);
	    break;
        case MPI_THREAD_FUNNELED: 
            printf("tylko te wątki, ktore wykonaly mpi_init_thread mogą wykonać wołania do biblioteki mpi\n");
	    break;
        case MPI_THREAD_SERIALIZED: 
            /* Potrzebne zamki wokół wywołań biblioteki MPI */
            printf("tylko jeden watek naraz może wykonać wołania do biblioteki MPI\n");
	    break;
        case MPI_THREAD_MULTIPLE: printf("Pełne wsparcie dla wątków\n");
	    break;
        default: printf("Nikt nic nie wie\n");
    }

    /* Stworzenie typu */
    /* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
       brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
    */
    /* sklejone z stackoverflow */
    const int nitems=2;
    int       blocklengths[2] = {1,1};
    MPI_Datatype typy[2] = {MPI_INT, MPI_INT};
    MPI_Datatype MPI_PAKIET_T;
    MPI_Aint     offsets[2];

    offsets[0] = offsetof(packet_t, appdata);
    offsets[1] = offsetof(packet_t, clock);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);


    pthread_t threadA;
    /* Tworzenie wątku */
    pthread_create( &threadA, NULL, startFunc, 0);

    /* Zamykanie muteksa */
  //pthread_mutex_lock(&mut);
    /* Otwieranie muteksa */
  //pthread_mutex_unlock(&mut);


    int size,rank;
    char end = FALSE;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);

    MPI_Status status;
    int data;     
    packet_t pakiet;

    if (rank==0) {
        /* Usuń kod, jest tylko po to, by przypomnieć działanie MPI_Send
           oraz, by program przykładowy się zakończył */
        int i;
        for (i=1;i<size;i++) {
	    MPI_Send(&pakiet, 1, MPI_PAKIET_T, i, FINISH, MPI_COMM_WORLD);
            printf("Poszło do %d\n", i);
        }
        end = TRUE;
    } 

    srand(rank);

    /* Obrazuje pętlę odbierającą pakiety o różnych typach */
    while ( !end ) {
    MPI_Recv( &pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

       switch (status.MPI_TAG) {
	    case FINISH: end = TRUE; break;
            case APP_MSG: 
                passive = FALSE;
                break;
            /* więcej case */
            default: 
                break;
       }

    }

    pthread_mutex_destroy( &mut);

    /* Czekamy, aż wątek potomny się zakończy */
    pthread_join(threadA,NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
}
