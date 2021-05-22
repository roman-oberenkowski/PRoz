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

int thread_rank,size;
int lamportClock = 10;
vector<int> looking;

MPI_Datatype MPI_PAKIET_T;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clockMut = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    int appdata;
    int clock; /* można zmienić nazwę na bardziej pasujące */
} packet_t;

string getTabs(){
    string a="";
    for(int i=0;i<thread_rank;i++){
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

void process_INV(packet_t pakiet,MPI_Status status,int thread_rank){
    int from=status.MPI_SOURCE;
    if(from==thread_rank)return;
    cout<<getTabs()<<"INV from "<<from<<" c:"<<pakiet.clock<<";d:"<<pakiet.appdata<<endl;
    looking.push_back(status.MPI_SOURCE);
    cout<<getTabs()<<"looking: "<<vecToString(looking)<<endl;
}

void send_finish_to_all(){
    for(int i=0;i<size;i++){
        sendMsg(0,1,MPI_PAKIET_T,i,FINISH);
    }
}

/* Kod funkcji wykonywanej przez wątek */
void *mainThreadFunc(void *ptr)
{
    packet_t pakiet;
    sendMsg(965, 1, MPI_PAKIET_T, 2, INV);
    sleep(5);
    send_finish_to_all();
    return 0;
}

int main(int argc, char **argv)
{
    int provided;
    MPI_Init_thread(&argc, &argv, MPI_THREAD_MULTIPLE, &provided);
    MPI_Comm_rank(MPI_COMM_WORLD, &thread_rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    const int nitems = 2;
    int blocklengths[2] = {1, 1};
    MPI_Datatype typy[2] = {MPI_INT, MPI_INT};
    MPI_Aint offsets[2];
    offsets[0] = offsetof(packet_t, appdata);
    offsets[1] = offsetof(packet_t, clock);
    MPI_Type_create_struct(nitems, blocklengths, offsets, typy, &MPI_PAKIET_T);
    MPI_Type_commit(&MPI_PAKIET_T);

    pthread_t threadA;
    pthread_create(&threadA, NULL, mainThreadFunc, 0);
    char end = FALSE;
    
    cout<<getTabs()<<"started "<<thread_rank<<endl;
    srand((int)time(0)+thread_rank);
    
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
            process_INV(pakiet,status,thread_rank);
            break;
        default:
            break;
        }
    }

    pthread_mutex_destroy(&mut);
    pthread_join(threadA, NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
}

