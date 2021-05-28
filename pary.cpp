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
#define REQforS 101
#define ACKSection 102
#define RELofS 103

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


typedef struct
{
    int id;
    int clock;
}queueEl;
std::vector <queueEl> queueForRoom;
int receivedACKS;
pthread_mutex_t roomMut = PTHREAD_MUTEX_INITIALIZER;
int S = 1;

string getTabs(){
    string a=to_string(thread_rank)+" ";
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

string queue2Str(std::vector <queueEl> queue)
{
    string res = "";
    string pom;
    std::vector<queueEl>::iterator it;
        for(it = queue.begin() ; it<queue.end() ; it++)
        {
            pom = to_string(it->id);
            pom += ":";
            pom +=to_string(it->clock);
            res += pom+" , ";
        }
    return res;
}

void insertIntoQueue(std::vector <queueEl>* queue , queueEl el)
{
    cout<<getTabs()<<"wstawiam do kolejki"<<endl;
    if(queue->size() == 0)queue->push_back(el);
    else{
        bool inserted = false;
        std::vector<queueEl>::iterator it;
        for(it = queue->begin() ; it<queue->end() ; it++)
        {
            if(el.clock < it->clock)
            {
                queue->insert(it ,el);
                inserted = true;
                break;
            }
            else if(el.clock == it->clock && el.id < it->id)
            {
                queue->insert(it ,el);
                inserted = true;
                break;
            }
        }
        if(inserted == false)queue->push_back(el);
    }
    cout<<getTabs()<<"wstawiłem do kolejki"<<endl;
    cout<<getTabs()<<queue2Str(*queue)<<endl;
}

void sendReqForRoom(int partnerId = 0)
{
    cout<<getTabs()<<"wysłam REQ"<<endl;
    queueEl el;
    int rank,size;
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size(MPI_COMM_WORLD, &size);
    el.id = rank;
    packet_t packet;
    packet.appdata = partnerId;
	pthread_mutex_lock(&clockMut);
    lamportClock++;
    packet.clock = lamportClock;
    for(int i=0;i<size;i++)
    {
        if(i!= rank)
        {
            MPI_Send(&packet , 1 , MPI_PAKIET_T , i , REQforS,MPI_COMM_WORLD);
        }
    }
    el.clock = lamportClock;
    insertIntoQueue(&queueForRoom , el);
    pthread_mutex_unlock(&clockMut);
    cout<<getTabs()<<"wysłałem REQ"<<endl;
    
}

void resp2Req(packet_t packet , MPI_Status status , std::vector <queueEl>* queue)
{
    cout<<getTabs()<<"REQforRoom from "<<status.MPI_SOURCE<<" c:"<<packet.clock<<endl;
    queueEl el;
    el.id = status.MPI_SOURCE;
    el.clock = packet.clock;
    insertIntoQueue(queue , el);
    sendMsg(0 , 1, MPI_PAKIET_T , status.MPI_SOURCE , ACKSection);
    cout<<getTabs()<<"odpowiedziałem na REQ"<<endl;
}
void resp2Rel( packet_t packet ,MPI_Status status , std::vector <queueEl> *queue)
{
    cout<<getTabs()<<"RelOfRoom from "<<status.MPI_SOURCE<<" c:"<<packet.clock<<endl;
    std::vector<queueEl>::iterator it;
    for(it = queue->begin() ; it<queue->end() ; it++)
    {
          if(it->id == status.MPI_SOURCE)
          {
              queue->erase(it);
              break;
          }  
    }
    cout<<getTabs()<<"kolejka: "<<queue2Str(*queue)<<endl;
    cout<<getTabs()<<"obsłużyłem REL"<<endl;
}

bool check4Section(std::vector <queueEl>* queue , int* confirmed, int sectionSize)
{
    std::cout<<getTabs()<<"Sprawdzam sekcje, liczba acków: "<<*confirmed<<std::endl;
    bool res = false;
    if(*confirmed == size-1)
    {
        std::cout<<getTabs()<<"sostałem wszytski acki, rozmiar kolejki:"<<queue->size()<<std::endl;
        if(!queue->empty())
        {
            std::cout<<getTabs()<<"kolejka nie jest pusta"<<std::endl;
            int pom = queue->size();
            for(int i=0; i< std::min(sectionSize , pom);i++)
            {
                if((queue->begin()+i)->id == thread_rank)
                {
                    res = true;
                    *confirmed = 0;
                    pthread_mutex_unlock(&roomMut);
                    break;
                }
            }
            // if(queueForRoom.front().id == thread_rank)
            // {
            //     res = true;
            //     receivedACKS = 0;
            //     pthread_mutex_unlock(&roomMut);
                
            // }
        }
        
    }

    

    return res;
}
void leaveQueue(std::vector <queueEl>* queue)
{
    for(int i=0; i<queue->size();i++)
            {
                if((queue->begin()+i)->id == thread_rank)
                {
                    queue->erase(queue->begin()+i);
                    break;
                }
            }
}
/* Kod funkcji wykonywanej przez wątek */
void *mainThreadFunc(void *ptr)
{
    packet_t pakiet;
    if(thread_rank != 0)
    {
        //sleep(rand()%5);
        sendReqForRoom();
        pthread_mutex_lock(&roomMut);
        std::cout<<getTabs()<<"Hura jest w sekcji krytycznej"<<std::endl;
        sleep(20);
        std::cout<<getTabs()<<"Opuszczam sekcję"<<std::endl;
        leaveQueue(&queueForRoom);
        packet_t packet;
        pthread_mutex_lock(&clockMut);
        lamportClock++;
        packet.clock = lamportClock;
        for(int i=0;i<size;i++)
        {
            if(i!= thread_rank)
            {
                MPI_Send(&packet , 1 , MPI_PAKIET_T , i , RELofS,MPI_COMM_WORLD);
            }
        }
        pthread_mutex_unlock(&clockMut);

    }
    
    

    //sendMsg(965, 1, MPI_PAKIET_T, 2, INV);
    //sleep(5);
    //send_finish_to_all();
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

    pthread_mutex_lock(&roomMut);

    pthread_t threadA;
    pthread_create(&threadA, NULL, mainThreadFunc, 0);
    char end = FALSE;
    
    cout<<getTabs()<<"started "<<thread_rank<<endl;
    srand((int)time(0)+thread_rank);

    packet_t pakiet;
    MPI_Status status;
    receivedACKS = 0;
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
        case RELofS:
            resp2Rel(pakiet, status , &queueForRoom);
            check4Section(&queueForRoom , &receivedACKS, S);
            break;
        case REQforS:
            resp2Req(pakiet , status , &queueForRoom);
            break;
        case ACKSection:
            cout<<getTabs()<<"dostałem ACK, source:"<<status.MPI_SOURCE<<endl;
            receivedACKS += 1;
            check4Section(&queueForRoom , &receivedACKS , S);
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

