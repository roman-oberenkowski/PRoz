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
#define DEN 4
#define ANS 5
#define ACK 6

int thread_rank, size;
int state = 1;
int current_pair = -1;
int lamportClock = 10;
int ans_send_to=-1;
vector<int> looking;

MPI_Datatype MPI_PAKIET_T;
pthread_mutex_t mut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t clockMut = PTHREAD_MUTEX_INITIALIZER;

typedef struct
{
    int appdata;
    int clock; /* można zmienić nazwę na bardziej pasujące */
} packet_t;

string getTabs()
{
    string a = to_string(thread_rank) + " ";
    for (int i = 0; i < thread_rank; i++)
    {
        a += ' ';
    }
    return a;
}

string vecToString(vector<int> vec)
{
    string out = "";
    for (int i = 0; i < vec.size(); i++)
    {
        out += to_string(vec[i]) + ",";
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

void process_INV(packet_t pakiet, MPI_Status status, int thread_rank)
{
    int from = status.MPI_SOURCE;
    if (from == thread_rank)return;
    
    cout << getTabs() << "INV from " << from;
    if (std::find(looking.begin(), looking.end(), from) == looking.end())
    {
        looking.push_back(from);
    }
    else
    {
        cout << getTabs() << "duplicate inv recived!";
    }
    cout << " looking: " << vecToString(looking) << endl;
    if(state==2){
        sendMsg(0,1,MPI_PAKIET_T,from,ANS);
        state=3;
    }
}

void process_ANS(packet_t pakiet, int from)
{
    switch (state)
    {
    case 1:
        sendMsg(0, 1, MPI_PAKIET_T, from, DEN);
        break;
    case 2:
        state=4;
        remove(looking.begin(),looking.end(),from);
        current_pair=from;
        sendMsg(0,1,MPI_PAKIET_T,from, ACK);
        break;
    case 3:
        if(from==ans_send_to){
            remove(looking.begin(),looking.end(),from);
            sendMsg(0,1,MPI_PAKIET_T,from,ACK);
        }else{
            sendMsg(0,1,MPI_PAKIET_T,from, DEN);
        }
        break;
    case 4:
        sendMsg(0,1,MPI_PAKIET_T,from, DEN);
        break;
    }

}

void process_DEN(packet_t pakiet, int from)
{
    switch (state)
    {
    case 3:
        remove(looking.begin(),looking.end(),from);
        state=2;
        break;
    }
}
void process_ACK(packet_t pakiet, int from)
{
    switch (state)
    {
    case 3:
        state=4;
        current_pair=from;
        remove(looking.begin(),looking.end(),from);
        break;
    default:
        cout<<"got strange ACK";
    }
}

void send_finish_to_all()
{
    for (int i = 0; i < size; i++)
    {
        sendMsg(0, 1, MPI_PAKIET_T, i, FINISH);
    }
}

/* Kod funkcji wykonywanej przez wątek */
void *mainThreadFunc(void *ptr)
{
    while(1){
        switch (state)
        {
        case 1:
            cout<<getTabs()<<"Odpoczywam"<<endl;
            sleep(rand() % 6 + 1);
            if (looking.size() == 0)
            {
                for (int i = 0; i < size; i++)
                {
                    if (i != thread_rank)
                    {
                        sendMsg(0, 1, MPI_PAKIET_T, i, INV);
                    }
                }
                state = 2;
            }
            else
            {
                sendMsg(0, 1, MPI_PAKIET_T, looking.back(), ANS);
                state = 3;
            }
            break;
        case 4:
            cout<<getTabs()<<"Debautjem!!!! z "<<current_pair<<endl;
            sleep(10+rand()%10);
            state=1;
            break;
        }
        sleep(1);
        cout<<getTabs()<<"state: "<<state<<endl;
    }
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

    cout << getTabs() << "started " << thread_rank << endl;
    srand((int)time(0) + thread_rank);

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
            process_INV(pakiet, status, thread_rank);
            break;
        case ANS:
            process_ANS(pakiet, status.MPI_SOURCE);
            break;
        case DEN:
            process_DEN(pakiet, status.MPI_SOURCE);
            break;
        case ACK:
            process_ACK(pakiet, status.MPI_SOURCE);
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
