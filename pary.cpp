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
#define REQforMISKI 111
#define ACK_MISKI 112
#define RELofMISKI 113
#define REQforPINEZKI 121
#define ACK_PINEZKI 122
#define RELofPINEZKI 123
#define REQforSLIPKI 131
#define ACK_SLIPKI 132
#define RELofSlipki 133
#define DEN 4
#define ANS 5
#define ACK 6
#define GOT_ROOM 201
#define GOT_ARGUMENTS 202


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
    int clock;
} packet_t;



typedef struct
{
    int id;
    int clock;
}queueEl;
std::vector <queueEl> queueForRoom;
int receivedACKS;
pthread_mutex_t roomMut = PTHREAD_MUTEX_INITIALIZER;
int S = 16;

std::vector <queueEl> queueForMiski;
int MreceivedACKS;
int M = 16;

std::vector <queueEl> queueForSlipki;
int GreceivedACKS;

int G = 16;

std::vector <queueEl> queueForPinezki;
int PreceivedACKS;
int P = 16;

int argumentsSecured;
int myArgument;
int enemyArgument;
pthread_mutex_t argumentsMut = PTHREAD_MUTEX_INITIALIZER;


pthread_mutex_t pairMut = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t queueMut = PTHREAD_MUTEX_INITIALIZER;

void send_invites();
void search_for_pair();

string getTabs()
{
    string a = "";
    switch(thread_rank){
        case 0:
            a+="\033[0;31m";
            break;
        case 1:
            a+="\033[0;32m";
            break;
        case 2:
            a+="\033[0;33m";
            break;
        case 3:
            a+="\033[0;34m";
            break;
        case 4:
            a+="\033[0;35m";
            break;
        case 5:
            a+="\033[0;36m";
            break;
        case 6:
            a+="\033[0;37m";
            break;
        case 7:
            a+="\033[0;40m";
            break;
    }
    for (int i = 0; i < thread_rank; i++)
    {
        a += "  ";
    }
    a+="zegar:"+to_string(lamportClock)+" ";  
    return a+"id:"+to_string(thread_rank) + "  ";
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

string argumentsToString()
{
    string infoText = "";
    switch (myArgument)
    {
    case REQforMISKI:
        infoText += "miski";
        break;
    case REQforPINEZKI:
        infoText += "pinezki";
        break;
    case REQforSLIPKI:
        infoText += "slipki";
        break;
    default:
        break;
    }
    infoText +=" i ";
    switch (enemyArgument)
    {
    case REQforMISKI:
        infoText += "miski";
        break;
    case REQforPINEZKI:
        infoText += "pinezki";
        break;
    case REQforSLIPKI:
        infoText += "slipki";
        break;
    default:
        break;
    }
    return infoText;
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

void remove_from_looking(int item){
    looking.erase(remove(looking.begin(),looking.end(),item),looking.end());
}

void process_INV(packet_t pakiet, MPI_Status status, int thread_rank)
{
    int from = status.MPI_SOURCE;
    if (from == thread_rank)return;
    
    //cout << getTabs() << "INV from " << from<< " looking: " << vecToString(looking) << endl;
    if (std::find(looking.begin(), looking.end(), from) == looking.end())
    {
        looking.push_back(from);
    }
    else
    {
        //cout << getTabs() << "duplicate inv recived!"<<endl;
    }
    //cout << " looking: " << vecToString(looking) << endl;
    if(state==2){
        state=3;
        ans_send_to=from;
        sendMsg(0,1,MPI_PAKIET_T,from,ANS);
        
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
        current_pair=from;
        state=4;
        remove_from_looking(from);
        
        
        sendMsg(0,1,MPI_PAKIET_T,from, ACK);
        
        break;
    case 3:
        
        if(from==ans_send_to){
            remove_from_looking(from);
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
    cout<<getTabs()<<"Denied "<<from<<std::endl;//<<" "<<current_pair<<" "<<vecToString(looking)<<" state:" <<state<<std::endl;
    switch (state)
    {
    case 3:
        remove_from_looking(from);
        state=2;

        search_for_pair();
        break;
    }
}

void process_ACK(packet_t pakiet, int from)
{
    switch (state)
    {
    case 3:
        current_pair=from;
        state=4;
        
        
        remove_from_looking(from);
        
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

void send_invites(){
    state = 2;
    for (int i = 0; i < size; i++)
        {
            if (i != thread_rank)
            {
                sendMsg(0, 1, MPI_PAKIET_T, i, INV);
            }
        }    
}

void search_for_pair(){
    ans_send_to=-1;
    if (looking.size() == 0)
    {
        send_invites();
    }
    else
    {
        state = 3;
        ans_send_to=looking.back();
        if(ans_send_to == thread_rank)
        {
            cout<<getTabs()<<"--Wysyłam do siebie--"<<endl;
            send_finish_to_all();
        }
        sendMsg(0, 1, MPI_PAKIET_T, ans_send_to, ANS);
        
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
            res += pom+" , ";;
        }
    return res;
}

void insertIntoQueue(std::vector <queueEl>* queue , queueEl el)
{
    pthread_mutex_lock(&queueMut);
    //cout<<getTabs()<<"wstawiam do kolejki"<<endl;
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
    pthread_mutex_unlock(&queueMut);
    //cout<<getTabs()<<"wstawiłem do kolejki"<<endl;
    //cout<<getTabs()<<queue2Str(*queue)<<endl;
}

void sendReqForRoom(int partnerId = 0)
{
    cout<<getTabs()<<"wysłam żądanie o salę"<<endl;
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
    //cout<<getTabs()<<"wysłałem REQ"<<endl;
    
}

void resp2Req(packet_t packet , MPI_Status status , std::vector <queueEl>* queue , int ACK_TAG)
{
    //cout<<getTabs()<<"REQforRoom from "<<status.MPI_SOURCE<<" c:"<<packet.clock<<endl;
    queueEl el;
    el.id = status.MPI_SOURCE;
    el.clock = packet.clock;
    insertIntoQueue(queue , el);
    sendMsg(0 , 1, MPI_PAKIET_T , status.MPI_SOURCE , ACK_TAG);
    //cout<<getTabs()<<"odpowiedziałem na REQ"<<endl;
}

void resp2Rel( packet_t packet ,MPI_Status status , std::vector <queueEl> *queue  )
{
    pthread_mutex_lock(&queueMut);
    //cout<<getTabs()<<"RelOfRoom from "<<status.MPI_SOURCE<<" c:"<<packet.clock<<endl;
    std::vector<queueEl>::iterator it;
    for(it = queue->begin() ; it<queue->end() ; it++)
    {
          if(it->id == status.MPI_SOURCE)
          {
              queue->erase(it);
              break;
          }  
    }
    //cout<<getTabs()<<"kolejka: "<<queue2Str(*queue)<<endl;
    //cout<<getTabs()<<"obsłużyłem REL"<<endl;
    pthread_mutex_unlock(&queueMut);
}

bool check4Section(std::vector <queueEl>* queue , int* confirmed, int sectionSize , pthread_mutex_t* mutex)
{
    pthread_mutex_lock(&queueMut);
    //std::cout<<getTabs()<<"Sprawdzam sekcje, liczba acków: "<<*confirmed<<std::endl;
    bool res = false;
    if(*confirmed == size-1)
    {
        //std::cout<<getTabs()<<"sostałem wszytski acki, rozmiar kolejki:"<<queue->size()<<std::endl;
        if(!queue->empty())
        {
            //std::cout<<getTabs()<<"kolejka nie jest pusta"<<std::endl;
            int pom = queue->size();
            for(int i=0; i< std::min(sectionSize , pom);i++)
            {
                if((queue->begin()+i)->id == thread_rank)
                {
                    res = true;
                    *confirmed = 0;
                    if(mutex != nullptr)
                        pthread_mutex_unlock(mutex);
                    break;
                }
            }

        }
        
    }
    pthread_mutex_unlock(&queueMut);

    

    return res;
}

void leaveQueue(std::vector <queueEl>* queue)
{
    pthread_mutex_lock(&queueMut);
    for(int i=0; i<queue->size();i++)
            {
                if((queue->begin()+i)->id == thread_rank)
                {
                    queue->erase(queue->begin()+i);
                    break;
                }
            }
    pthread_mutex_unlock(&queueMut);
}

int decideArgument()
{
    int pom[] = {REQforMISKI , REQforSLIPKI , REQforPINEZKI};
    std::random_shuffle(pom , pom+3);
    for(int i=0;i<3;i++)
    {
        switch (pom[i])
        {
        case REQforMISKI:
            if(queueForMiski.size() < M)return REQforMISKI;
            break;
        case REQforPINEZKI:
            if(queueForPinezki.size() < P)return REQforPINEZKI;
            break;
        case REQforSLIPKI:
            if(queueForSlipki.size() < G)return REQforSLIPKI;
            break;
        default:
            break;
        }
    }
    return pom[0];
}

std::vector <queueEl>* getQueueForArgument(int argument)
{
    switch (argument)
        {
        case REQforMISKI:
            return &queueForMiski;
            break;
        case REQforPINEZKI:
            return &queueForPinezki;
            break;
        case REQforSLIPKI:
            return &queueForSlipki;
            break;
        default:
            break;
        }
    return nullptr;
}

void requestArguments()
{
    cout<<getTabs()<<"wysłam żądanie o "<<argumentsToString()<<endl;
    queueEl el;

    el.id = thread_rank;
    packet_t packet;

	pthread_mutex_lock(&clockMut);
    lamportClock++;
    packet.clock = lamportClock;
    for(int i=0;i<size;i++)
    {
        if(i!= thread_rank)
        {
            MPI_Send(&packet , 1 , MPI_PAKIET_T , i , myArgument,MPI_COMM_WORLD);
            MPI_Send(&packet , 1 , MPI_PAKIET_T , i , enemyArgument,MPI_COMM_WORLD);
        }
    }
    el.clock = lamportClock;

    insertIntoQueue(getQueueForArgument(myArgument) , el);
    insertIntoQueue(getQueueForArgument(enemyArgument) , el);

    pthread_mutex_unlock(&clockMut);
}
//zwraca true przy wygranej
bool debate()
{
    if(myArgument == enemyArgument)
    {
        std::cout<<getTabs()<<"Remis debaty z "<<to_string(current_pair)<<std::endl;
        return false;
    }
    else if(myArgument == REQforSLIPKI && enemyArgument == REQforPINEZKI)
    {
        std::cout<<getTabs()<<"Wygranie debaty z "<<to_string(current_pair)<<std::endl;
        return true;
    }
    else if(myArgument == REQforPINEZKI && enemyArgument == REQforMISKI)
    {
        std::cout<<getTabs()<<"Wygranie debaty z "<<to_string(current_pair)<<std::endl;
        return true;
    }
    else if(myArgument == REQforMISKI && enemyArgument == REQforSLIPKI)
    {
        std::cout<<getTabs()<<"Wygranie debaty z "<<to_string(current_pair)<<std::endl;
        return true;
    }
    std::cout<<getTabs()<<"Przegranie debaty z "<<to_string(current_pair)<<std::endl;
    return false;

}
/* Kod funkcji wykonywanej przez wątek */
void *mainThreadFunc(void *ptr)
{
    packet_t pakiet;
    bool debateResult = false;
    while(1){
        //pthread_mutex_lock(&pairMut);
        switch (state)
        {
        case 1:
            cout<<getTabs()<<"Odpoczywam"<<endl;
            
            pthread_mutex_lock(&pairMut);
            search_for_pair();
            pthread_mutex_unlock(&pairMut);
            break;
        case 2:

            break;
        
        case 4:
            cout<<getTabs()<<"Dobrałem się w parę z "<<current_pair<<endl;
            if(current_pair > thread_rank)
            {
                sendReqForRoom(current_pair);
                pthread_mutex_lock(&roomMut);
                std::cout<<getTabs()<<"Zajęto salę"<<std::endl;
                myArgument = decideArgument(); 
                sendMsg(myArgument, 1, MPI_PAKIET_T , current_pair , GOT_ROOM);

                pthread_mutex_lock(&argumentsMut);
                std::cout<<getTabs()<<"Partner pobrał argumenty"<<std::endl;
                //sleep(1);
                debateResult = debate();

                
                std::cout<<getTabs()<<"Opuszczam salę"<<std::endl;
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
            else{
                pthread_mutex_lock(&argumentsMut);
                std::cout<<getTabs()<<"Pobrano oba argumenty"<<std::endl;
                sendMsg(myArgument , 1, MPI_PAKIET_T , current_pair , GOT_ARGUMENTS);
                //sleep(1);
                debateResult = debate();
                std::cout<<getTabs()<<"Oddaję argumenty"<<std::endl;
                leaveQueue(getQueueForArgument(myArgument));
                leaveQueue(getQueueForArgument(enemyArgument));
                packet_t packet;
                pthread_mutex_lock(&clockMut);
                lamportClock++;
                packet.clock = lamportClock;
                for(int i=0;i<size;i++)
                {
                    if(i!= thread_rank)
                    {
                        MPI_Send(&packet , 1 , MPI_PAKIET_T , i , myArgument+2,MPI_COMM_WORLD);
                        MPI_Send(&packet , 1 , MPI_PAKIET_T , i , enemyArgument+2,MPI_COMM_WORLD);
                    }
                }
                pthread_mutex_unlock(&clockMut);

            }
            //sleep(10);
            //if(debateResult)sleep(10);
            pthread_mutex_lock(&pairMut);
            current_pair=-1;
            state=1;
            
            pthread_mutex_unlock(&pairMut);
            break;
        }
        //pthread_mutex_unlock(&pairMut);
        //sleep(1);
        //cout<<getTabs()<<"state: "<<state<<endl;
    }
}

void checkArgumentsSecured()
{
    argumentsSecured++;
    if(argumentsSecured == 2)
    {
        argumentsSecured =  0;
        pthread_mutex_unlock(&argumentsMut);
                    
    }   
}

int main(int argc, char **argv)
{
    std::ios_base::sync_with_stdio(0);
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

    //początkowe ustawienie mutexów dla sekcji krytycznych
    pthread_mutex_lock(&roomMut);
    //pthread_mutex_lock(&miskiMut);
    //pthread_mutex_lock(&pinezkiMut);
    //pthread_mutex_lock(&slipkiMut);
    pthread_mutex_lock(&argumentsMut);
    receivedACKS = 0;
    MreceivedACKS = 0;
    PreceivedACKS = 0;
    GreceivedACKS = 0;
    argumentsSecured = 0;

    pthread_t threadA;
    pthread_create(&threadA, NULL, mainThreadFunc, 0);
    char end = FALSE;
    
    cout<<getTabs()<<"started "<<thread_rank<<endl;
    srand((int)time(0)+thread_rank);

    packet_t pakiet;
    MPI_Status status;
    
    while (!end)
    {
        int res = recvMsg(&pakiet, 1, MPI_PAKIET_T, MPI_ANY_SOURCE, MPI_ANY_TAG, MPI_COMM_WORLD, &status);
        
        if(res!=0){
            send_finish_to_all();
            cout<<"BLAD ODBIORU\n";
            return 0;
        }

        if(status.MPI_SOURCE==thread_rank){
            send_finish_to_all();
            cout<<"BLAD ODEBRALEM\n WIADOMOSC\n OD\n SAMEGO\n SIEBIE\n";
            return 0;
        }
        
        switch (status.MPI_TAG)
        {
        case FINISH:
            end = TRUE;
            break;
        case APP_MSG:
            break;
        case INV:
            pthread_mutex_lock(&pairMut);
            process_INV(pakiet, status, thread_rank);
            pthread_mutex_unlock(&pairMut);
            break;
        case ANS:
            pthread_mutex_lock(&pairMut);
            process_ANS(pakiet, status.MPI_SOURCE);
            pthread_mutex_unlock(&pairMut);
            break;
        case DEN:
            pthread_mutex_lock(&pairMut);
            process_DEN(pakiet, status.MPI_SOURCE);
            pthread_mutex_unlock(&pairMut);
            break;
        case ACK:
            pthread_mutex_lock(&pairMut);
            process_ACK(pakiet, status.MPI_SOURCE);
            pthread_mutex_unlock(&pairMut);
            break;
        case RELofS:
            resp2Rel(pakiet, status , &queueForRoom);
            check4Section(&queueForRoom , &receivedACKS, S , &roomMut);
            break;
        case REQforS:
            pthread_mutex_lock(&pairMut);
            remove_from_looking( status.MPI_SOURCE);
            remove_from_looking(pakiet.appdata);
            pthread_mutex_unlock(&pairMut);
            resp2Req(pakiet , status , &queueForRoom , ACKSection);
            break;
        case ACKSection:
            //cout<<getTabs()<<"dostałem ACK, source:"<<status.MPI_SOURCE<<endl;
            receivedACKS += 1;
            check4Section(&queueForRoom , &receivedACKS , S , &roomMut);
            break;
        case GOT_ROOM:
            cout<<getTabs()<<"Partner zajął salę"<<endl;
            enemyArgument = pakiet.appdata;
            myArgument = decideArgument();
            requestArguments();
            break;
        case GOT_ARGUMENTS:
            enemyArgument = pakiet.appdata;
            pthread_mutex_unlock(&argumentsMut);
            
            break;
        case RELofMISKI:
            resp2Rel(pakiet, status , &queueForMiski);
            if(check4Section(&queueForMiski , &MreceivedACKS , M,nullptr))
            {
                cout<<getTabs()<<"Pobrano miski"<<endl;
                checkArgumentsSecured();
            }
                
            break;
        case REQforMISKI:
            resp2Req(pakiet , status , &queueForMiski , ACK_MISKI);
            break;
        case ACK_MISKI:
            MreceivedACKS += 1;
            if(check4Section(&queueForMiski , &MreceivedACKS , M,nullptr))
                {
                    cout<<getTabs()<<"Pobrano miski"<<endl;
                    checkArgumentsSecured();
                }
            break;
        case RELofPINEZKI:
            resp2Rel(pakiet, status , &queueForPinezki);
            if(check4Section(&queueForPinezki , &PreceivedACKS , P , nullptr))
                {
                    cout<<getTabs()<<"Pobrano pinezki"<<endl;
                    checkArgumentsSecured();
                }
            break;
        case REQforPINEZKI:
            resp2Req(pakiet , status , &queueForPinezki , ACK_PINEZKI);
            break;
        case ACK_PINEZKI:
            PreceivedACKS += 1;
            if(check4Section(&queueForPinezki , &PreceivedACKS ,P , nullptr))
                {
                    cout<<getTabs()<<"Pobrano pinezki"<<endl;
                    checkArgumentsSecured();
                }
            break;
        case RELofSlipki:
            resp2Rel(pakiet, status , &queueForSlipki);
            if(check4Section(&queueForSlipki , &GreceivedACKS , G , nullptr))
                {
                    cout<<getTabs()<<"Pobrano slipki"<<endl;
                    checkArgumentsSecured();
                }
            break;
        case REQforSLIPKI:
            resp2Req(pakiet , status , &queueForSlipki , ACK_SLIPKI);
            break;
        case ACK_SLIPKI:
            GreceivedACKS += 1;
            if(check4Section(&queueForSlipki , &GreceivedACKS ,G , nullptr))
                {
                    cout<<getTabs()<<"Pobrano slipki"<<endl;
                    checkArgumentsSecured();
                }
            break;
        default:
            break;
        }
    }

    pthread_mutex_destroy(&mut);
    pthread_mutex_destroy(&clockMut);
    pthread_mutex_destroy(&roomMut);
    pthread_mutex_destroy(&argumentsMut);
    pthread_join(threadA, NULL);
    MPI_Type_free(&MPI_PAKIET_T);
    MPI_Finalize();
}
