/*
 * Producer-consommer, base without synchronisation
 * */

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#define Nb_Counters 3 // no. of counters
#define Nb_Steps 3    //no. of steps
#define Nb_Lines 3      // no. of assembly lines
#define Waiting_Line_Size 3  // size of the queue
#define Nb_Clients 3    // no. of clients

typedef struct {
    long id;
    int counterNo;
    char info[256];
    int  state;

} theOrder;

typedef struct {
    int busy;
    theOrder order;
} theCounter;

typedef struct {
    int noOfAssemblyLine;
    int numStep;
} theEmployee;

int remainingClients = NB_CLIENTS;          //Number of remaining clients to order
int waiters = 0;                            //Number of waiters
int idOrder = 0;                            //Id of the order
theCounter Counters[Nb_Counters];            //Counters
int Employees[Nb_Lines][Nb_Steps]; //Employees
int TakeCare[NB_COUNTERS];                  //
theEmployee EmployeeDatas [Nb_Lines*Nb_Steps];

pthread_mutex_t Clientmutex, Employeemutex;                      //Mutex for critical resources
pthread_cond_t WaitLine;                    //Condition for the wait line
pthread_cond_t Step[Nb_Steps];              //Conditions for each steps
pthread_cond_t OrderReady[Nb_Counters];     //Conditions for clients waiting their order

int getFreeCounter(){
    for(int i = 0; i<Nb_Counters; i++){
        if(Counters[i].busy == false){
            Counters[i].busy = true;
            return i;
        }
    }
    return -1;
}

int isNeeded(int step){
    for (int i = 0; i < Nb_Counters; i++) {
        if (Counters[i].order.state == step && TakeCare[i] == false && Counters[i].busy == true) {
            return i;
        }
    }
    return -1;
}

void initOrder(theOrder* o){
    o->id = idOrder;
    sprintf(o->title, "Order n°%d", idOrder);
    o->state = 0;
    idOrder++;
}

void initCounters(){
    for (int i = 0; i < NB_COUNTERS; i++) {
        Counters[i].busy = false;
    }
}

void lock(pthread_mutex_t* m){
    if (pthread_mutex_lock(m)!=0) {
        perror("pthread_mutex_lock");
        exit(1);
    }
}

void unlock(pthread_mutex_t* m){
    if (pthread_mutex_unlock(m)!=0) {
        perror("pthread_mutex_unlock");
        exit(1);
    }
}

void wait(pthread_mutex_t* m, pthread_cond_t* c){
    if (pthread_cond_wait(c, m)!=0) {
        perror("pthread_cond_wait");
        exit(1);
    }
}

void signal(pthread_cond_t* c){
    if (pthread_cond_signal(c)!=0) {
        perror("pthread_cond_signalTo");
        exit(1);
    }
}

void* Client(void* param){
    int counterNo;
    int clientNumber = param;
    
    if(waiters == 0){
        lock(&Clientmutex);
        while ((counterNo = getFreeCounter()) == -1) {
            printf("Client %d wait for a free Counter\n", clientNumber);
            wait(&Clientmutex, &WaitLine);
        }
        printf("Client %d get the counter %d\n", clientNumber, counterNo);
        unlock(&Clientmutex);
    }
    else{
        lock(&Clientmutex);
        waiters++;
        while(waiters > 0){
            wait(&Clientmutex, &WaitLine);
        }
        while ((counterNo = getFreeCounter()) == -1) {
            wait(&Clientmutex, &WaitLine);
        }
        waiters--;
        unlock(&Clientmutex);
    }
    lock(&Clientmutex);
    initOrder(&Counters[counterNo].order);
    signal(&Step[0]);
    printf("\t-->Client %d at counter %d has ordered \n", clientNumber, counterNo);
    wait(&Clientmutex, &OrderReady[counterNo]);
    Counters[counterNo].busy = false;
    printf("\n~~~~~~ Client %d at counter %d has finished ~~~~~~\n", clientNumber, counterNo);
    remainingClients--;
    if (remainingClients > 0) {
        signal(&WaitLine);
    }
    else{
        exit(0);
    }
    
    unlock(&Clientmutex);
    
    return (void*)NULL;
}

void* Employee(void* param){
    int index = param;
    int step = EmployeeDatas[index].numStep;
    int numAL = EmployeeDatas[index].noOfAssemblyLine;
    int counterNo;
    while (1) {
        
        lock(&Employeemutex);
        while ((counterNo = isNeeded(step)) == -1) {
            wait(&Employeemutex, &Step[step]);
        }
        TakeCare[counterNum] = true;
        unlock(&Employeemutex);
        
        printf("\n\t\tEmployee [%d, %d]\n\t\t processes the step %d for %s on the counter %d\n", numAL, step, step, Counters[counterNo].order.info, counterNo);
        usleep(100000);
        
        if (step == Nb_Steps-1) {
            lock(&Employeemutex);
            Counters[counterNo].order.state = -1;
            TakeCare[counterNo] = false;
            signal(&OrderReady[counterNo]);
            unlock(&Employeemutex);
        }
        else{
            lock(&Employeemutex);
            Counters[counterNo].order.state++;
            TakeCare[counterNoc] = false;
            signal(&Step[step+1]);
            unlock(&Employeemutex);
        }
    }
}

int main(int argc, char const *argv[]) {
    pthread_cond_init(&WaitLine, 0);
    pthread_mutex_init(&Clientmutex, 0);
    pthread_mutex_init(&Employeemutex, 0);
    pthread_t Clients[Nb_Clients];
    pthread_t Manufacturers[Nb_Lines][Nb_Steps];
    
    for (int i = 0; i < Nb_Steps; i++) {
        if (pthread_cond_init(&Step[i], 0) != 0) {
            perror("pthread_cond_init Step");
            exit(1);
        }
    }
    
    initCounters();
    for (size_t i = 0; i < Nb_Clients; i++) {
        if (pthread_create(&Clients[i], NULL, Client, (void*)i) != 0) {
            printf("Error : pthread_create\n");
            exit(1);
        }
    }
    for (size_t i = 0; i < Nb_Lines; i++) {
        for (size_t j = 0; j < Nb_Steps; j++) {
            EmployeeDatas[i+j].noOfAssemblyLine = i;
            EmployeeDatas[i+j].numStep = j;
            if (pthread_create(&Manufacturers[i][j], NULL, Employee, (void*)(i+j)) != 0) {
                perror("pthread_create Employee");
                exit(1);
            }
        }
    }
    
    for (size_t i = 0; i < Nb_Clients; i++) {
        if(pthread_join(Clients[i], NULL) != 0){
            printf("Error : pthread_join\n");
            exit(1);
        }
    }
    for (size_t i = 0; i < Nb_Lines; i++) {
        for (size_t j = 0; j < Nb_Steps; j++) {
            if(pthread_join(Manufacturers[i][j], NULL) != 0){
                printf("Error : pthread_join\n");
                exit(1);
            }
        }
    }
    
    return 0;
}
