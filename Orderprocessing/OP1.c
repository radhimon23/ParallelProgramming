/*
 * Producer-consommer, base without synchronisation
 * */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <pthread.h>
#include <unistd.h>

#define NB_COUNTERS 3
#define NB_STEPS 1              
#define NB_LINES 1
#define WAITING_LINE_SIZE 256
#define NB_CLIENTS 8
#define false 0
#define true 1

typedef struct{
    long id;
    int counterNum;
    char title[256];
    int state;
} theOrder;

typedef struct{
    int busy;
    order_t order;
} theCounter;

typedef struct{
    int numAL;
    int numStep;
} theEmployee;

int remainingClients = NB_CLIENTS;
int waiters = 0;
int idOrder = 0;
theCounter Counters[NB_COUNTERS];
int Employees[NB_LINES][NB_STEPS];
int TakeCare[NB_COUNTERS];
theEmployee EmployeeDatas [NB_LINES*NB_STEPS];
pthread_mutex_t Clientmutex, Employeemutex;
pthread_cond_t WaitLine;
pthread_cond_t Step[NB_STEPS];
pthread_cond_t OrderReady[NB_COUNTERS];

int getFreeCounter(){
    for(int i = 0; i<NB_COUNTERS; i++){
        if(Counters[i].busy == false){
            Counters[i].busy = true;
            return i;
        }
    }
    return -1;
}

int isNeeded(int step){
    for (size_t i = 0; i < NB_COUNTERS; i++) {
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
    for (size_t i = 0; i < NB_COUNTERS; i++) {
        Counters[i].busy = false;
    }
}

void lock(pthread_mutex_t* m){
    if (pthread_mutex_lock(m)!=0) {
        perror("pthread_mutex_lock");
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
    int counterNum;
    int clientNumber = param;
    
    if(waiters == 0){
        lock(&Clientmutex);
        while ((counterNum = getFreeCounter()) == -1) {
            printf("Client %d wait for a free Counter\n", clientNumber);
            wait(&Clientmutex, &WaitLine);
        }
        printf("Client %d get the counter %d\n", clientNumber, counterNum);
        unlock(&Clientmutex);
    }
    else{
        lock(&Clientmutex);
        waiters++;
        while(waiters > 0){
            wait(&Clientmutex, &WaitLine);
        }
        while ((counterNum = getFreeCounter()) == -1) {
            wait(&Clientmutex, &WaitLine);
        }
        waiters--;
        unlock(&Clientmutex);
    }
    lock(&Clientmutex);
    initOrder(&Counters[counterNum].order);
    signalTo(&Step[0]);
    printf("\t-->Client %d at counter %d has ordered \n", clientNumber, counterNum);
    wait(&Clientmutex, &OrderReady[counterNum]);
    Counters[counterNum].busy = false;
    printf("\n~~~~~~ Client %d at counter %d has finished ~~~~~~\n", clientNumber, counterNum);
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
    int numAL = EmployeeDatas[index].numAL;
    int counterNum;
    while (1) {
        
        lock(&Employeemutex);
        while ((counterNum = isNeeded(step)) == -1) {
            wait(&Employeemutex, &Step[step]);
        }
        TakeCare[counterNum] = true;
        unlock(&Employeemutex);
        
        printf("\n\t\tEmployee [%d, %d]\n\t\tIs processing the step %d for %s on the counter %d\n", numAL, step, step, Counters[counterNum].order.title, counterNum);
        usleep(100000);
        
        if (step == NB_STEPS-1) {
            lock(&Employeemutex);
            Counters[counterNum].order.state = -1;
            TakeCare[counterNum] = false;
            signal(&OrderReady[counterNum]);
            unlock(&Employeemutex);
        }
        else{
            lock(&Employeemutex);
            Counters[counterNum].order.state++;
            TakeCare[counterNum] = false;
            signal(&Step[step+1]);
            unlock(&Employeemutex);
        }
    }
}


int main(int argc, char const *argv[]) {
    pthread_cond_init(&WaitLine, 0);
    pthread_mutex_init(&Clientmutex, 0);
    pthread_mutex_init(&Employeemutex, 0);
    pthread_t Clients[NB_CLIENTS];
    pthread_t Manufacturers[NB_LINES][NB_STEPS];
    
    for (int i = 0; i < NB_STEPS; i++) {
        if (pthread_cond_init(&Step[i], 0) != 0) {
            perror("pthread_cond_init Step");
            exit(1);
        }
    }
    
    initCounters();
    for (size_t i = 0; i < NB_CLIENTS; i++) {
        if (pthread_create(&Clients[i], NULL, Client, (void*)i) != 0) {
            printf("Error : pthread_create\n");
            exit(1);
        }
    }
    for (size_t i = 0; i < NB_LINES; i++) {
        for (size_t j = 0; j < NB_STEPS; j++) {
            EmployeeDatas[i+j].numAL = i;
            EmployeeDatas[i+j].numStep = j;
            if (pthread_create(&Manufacturers[i][j], NULL, Employee, (void*)(i+j)) != 0) {
                perror("pthread_create Employee");
                exit(1);
            }
        }
    }
    
    for (size_t i = 0; i < NB_CLIENTS; i++) {
        if(pthread_join(Clients[i], NULL) != 0){
            printf("Error : pthread_join\n");
            exit(1);
        }
    }
    for (size_t i = 0; i < NB_LINES; i++) {
        for (size_t j = 0; j < NB_STEPS; j++) {
            if(pthread_join(Manufacturers[i][j], NULL) != 0){
                printf("Error : pthread_join\n");
                exit(1);
            }
        }
    }
    return 0;
}

