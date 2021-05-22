#include <mpi.h>

#include <cstdlib>
#include <stdio.h>
#include <stddef.h>
#include <unistd.h>
#include <algorithm>
#include <pthread.h>
#include <iostream>
#include <vector>
#include <ctime>
#include <string>
using namespace std;

/* boolean */
#define TRUE 1
#define FALSE 0

/* typy wiadomości */
#define FINISH 1
#define APP_MSG 2
#define INV 3


MPI_Datatype MPI_PAKIET_T;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clockMut = PTHREAD_MUTEX_INITIALIZER;
int lamportClock = 10;
vector<int> looking;

typedef struct
{
    int appdata;
    int clock; /* można zmienić nazwę na bardziej pasujące */
} packet_t;

string getTabs(int rank){
    string a="";
    for(int i=0;i<rank;i++){
        a+='\t';
    }
    return a;
}

string vecToString(vector<int> vec){
    string out="";
    for(int i=0;i<vec.size();i++){
        out+= to_string(vec[i])+",";
    }
    return out;
}
int sendMsg(int data, int count, MPI_Datatype datatype, int dest, int tag)
{
    packet_t packet;
    packet.appdata = data;
    pthread_mutex_lock(&clockMut);
    lamportClock++;
    packet.clock = lamportClock;
    int res = MPI_Send(&packet, count, datatype, dest, tag, MPI_COMM_WORLD);
    pthread_mutex_unlock(&clockMut);
    return res;
}

int recvMsg(packet_t *buf, int count, MPI_Datatype datatype, int source, int tag, MPI_Comm comm, MPI_Status *status)
{
    int res = MPI_Recv(buf, count, datatype, source, tag, comm, status);
    pthread_mutex_lock(&clockMut);
    lamportClock = std::max(lamportClock, buf->clock) + 1;
    pthread_mutex_unlock(&clockMut);
    return res;
}

void process_INV(packet_t pakiet,MPI_Status status,int rank){
    int from=status.MPI_SOURCE;
    if(from==rank)return;
    cout<<getTabs(rank)<<"INV from "<<from<<" c:"<<pakiet.clock<<";d:"<<pakiet.appdata<<endl;
    looking.push_back(status.MPI_SOURCE);
    cout<<getTabs(rank)<<"looking: "<<vecToString(looking)<<endl;
}

/* Kod funkcji wykonywanej przez wątek */
void *startFunc(void *ptr)
{
    packet_t pakiet;

    /* wątek się kończy, gdy funkcja się kończy */
    sendMsg(965, 1, MPI_PAKIET_T, 2, INV);
    sleep(5);
    
    sendMsg(0,1,MPI_PAKIET_T,0,FINISH);
    sendMsg(0,1,MPI_PAKIET_T,1,FINISH);
    sendMsg(0,1,MPI_PAKIET_T,2,FINISH);
    sendMsg(0,1,MPI_PAKIET_T,3,FINISH);
    return 0;
}

int main(int argc, char **argv)
{
    int size, rank;
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    /* Stworzenie typu */
    /* Poniższe (aż do MPI_Type_commit) potrzebne tylko, jeżeli
       brzydzimy się czymś w rodzaju MPI_Send(&typ, sizeof(pakiet_t), MPI_BYTE....
    */
    /* sklejone z stackoverflow */
    const int nitems = 2;
    int blocklengths[2] = {1, 1};
    MPI_Datatype typy[2] = {MPI_INT, MPI_INT};
    
    MPI_Aint offsets[2];

    offsets[0] = offsetof(packet_t, appdata);
    offsets[1] = offsetof(packet_t, clock);

    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);

    pthread_t threadA;
    /* Tworzenie wątku */
    pthread_create(&threadA, NULL, startFunc, 0);

    
    char end = FALSE;

    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    cout<<getTabs(rank)<<"started "<<rank<<endl;
    srand((int)time(0)+rank);
    

    /* Obrazuje pętlę odbierającą pakiety o różnych typach */
    packet_t pakiet;
    MPI_Status status;
    while (!end)
    {
        recvMsg(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);

        switch (status.MPI_TAG)
        {
        case FINISH:
            end = TRUE;
            break;
        case APP_MSG:
            break;
        case INV:
            process_INV(pakiet,status,rank);
            break;
        /* więcej case */
        default:
            break;
        }
    }

    pthread_mutex_destroy(&mut);

    /* Czekamy, aż wątek potomny się zakończy */
    pthread_join(threadA, NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
}

