#include <iostream>
#include <pthread.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <chrono>

#include <mutex>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>

#include <unistd.h>

#include "draconis_util.h"
#include "dpdk.h"

using namespace std;

#define NUM_THREADS 20 
#define SRVR_PORTS_NUM 8

const char* srcIP = "192.168.126.101";
const char* serverIP = "192.168.126.55";

// const int dst_ports[14] = {5555,6555,7555,8555,9555,10555,11555,12555,13555,14555};
const int dst_ports[17] = {11001,12002,13003,14004,15005,16006,17007,18008,19009,20010,21011,22012,23013,24014,25015,26016,27017};

pthread_mutex_t printLock;

__attribute__ ((aligned (64))) uint64_t threadTaskCounters[NUM_THREADS];
__attribute__ ((aligned (64))) uint64_t threadPacketCounters[NUM_THREADS];

struct workerThreadData {
    int threadID;
    int runTimeSec;
};

//__attribute__ ((aligned (64))) thread_local uint64_t tasksExecuted;

int getTaskFromScheduler(int sockfd, int threadID, struct sockaddr_in servaddr, RetrieveTaskPacket *newTask, uint64_t startTimeMs, unsigned int runTimeMs, RetrieveTaskPacket* retrievedPackets){
    char receiveTaskStream[RETRIEVE_TASK_PACKET_SIZE];
    
//     char *taskStream = RetrieveTaskPacket::convertToByteStream(newTask);
//     cout << "------------------------------- SENDING -------------------------------------------------------------" << endl;
//     cout << "Sending Message: " << newTask->toString() << endl;

    socklen_t len;

    int event_num = 0;

    memset(receiveTaskStream, 0, RETRIEVE_TASK_PACKET_SIZE);

    /************ filling ip_tuple and net_sge entry ************/

	ip_tuple id;

	inet_pton(AF_INET, srcIP, &id.src_ip );
	inet_pton(AF_INET, "192.168.126.111", &id.dst_ip );
	id.src_port = threadID * 1000 + 10000 + threadID ;
	id.dst_port = 5555;

    id.src_port = rte_cpu_to_be_16(id.src_port);

    id.dst_ip = rte_cpu_to_be_32(id.dst_ip);
    id.src_ip = rte_cpu_to_be_32(id.src_ip);
    
 	struct net_sge *entry;
	entry = alloc_net_sge();
	assert(entry);

    entry->len += sizeof(*newTask);

	rte_memcpy (entry->payload , (void *)newTask, sizeof(*newTask));

	udp_send(entry, &id);

    RetrieveTaskPacket *receivedTaskPacket;

    auto currTime = std::chrono::high_resolution_clock::now().time_since_epoch();
    uint64_t currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
    uint64_t currTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(currTime).count();
    uint64_t sendStartTimeUs = currTimeUs;

    int receivedLen = 0;
    int ret = 0 , i = 0;
    while(currTimeMs - startTimeMs < runTimeMs){
   //     cout << "Stuck in getTask loop" << endl;
        struct rte_mbuf *rx_pkts[BATCH_SIZE];
        ret = rte_eth_rx_burst(0, RTE_PER_LCORE(queue_id), rx_pkts, BATCH_SIZE);
        if (ret == 0 ){
            currTime = std::chrono::high_resolution_clock::now().time_since_epoch();
            currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
            currTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(currTime).count();
            if ((currTimeUs - sendStartTimeUs) > 1000)
                break;
            continue;
        }

        for (i = 0; i < ret; i++){

            struct ip_tuple *recv_id;
            entry = eth_in(rx_pkts[i],recv_id);
            receivedTaskPacket = (RetrieveTaskPacket *)entry->payload;
            
            if(sizeof(*receivedTaskPacket) == RETRIEVE_TASK_PACKET_SIZE){
                receivedTaskPacket->functionID = ntohl((uint32_t)receivedTaskPacket->functionID); 
                receivedTaskPacket->functionParameters = ntohl((uint32_t)receivedTaskPacket->functionParameters); 
                receivedTaskPacket->userIP = ntohl((uint32_t)receivedTaskPacket->userIP); 
                receivedTaskPacket->time1 = ntohl((uint32_t)receivedTaskPacket->time1); 
                receivedTaskPacket->time2 = ntohl((uint32_t)receivedTaskPacket->time2); 

                receivedTaskPacket->jobID = ntohs((uint16_t)receivedTaskPacket->jobID); 
                receivedTaskPacket->taskID = ntohs((uint16_t)receivedTaskPacket->taskID); 
                receivedTaskPacket->userPort = ntohs((uint16_t)receivedTaskPacket->userPort); 

                // cout << "------------------------------- RECEIVAL -------------------------------------------------------------" << endl;
                // cout << "Received Bytes: " << receivedLen << endl;
                // cout << "Received Packet: " << receivedTaskPacket->toString() << endl;
            }
            else{
                receivedTaskPacket->functionID = 0;
                receivedTaskPacket->functionParameters = 0;
                receivedTaskPacket->userIP = 0;
                receivedTaskPacket->time1 = 0;
                receivedTaskPacket->time2 = 0;

                receivedTaskPacket->jobID = 0; 
                receivedTaskPacket->taskID = INVALID_TASK_ID; 
                receivedTaskPacket->userPort = 0; 
            }
            retrievedPackets[i] = *receivedTaskPacket;

            /* Print recieved packet for debuging */
            /*pthread_mutex_lock(&printLock);
                    cout << "Recieved Packet " << i << " on Thread " << threadID << " : " << endl;
                    pkt_dump((rte_mbuf*)entry->handle);
            pthread_mutex_unlock(&printLock);*/

            rte_pktmbuf_free((rte_mbuf*)entry->handle);//This might be needed to be moved further down the line, maybe after task is done running
        }

        if(ret){	
                break;
        }
        // cout << "Received Packet: " << receivedTaskPacket.toString() << endl;

        // receivedLen = recv(sockfd, &receivedTaskPacket, RETRIEVE_TASK_PACKET_SIZE , MSG_DONTWAIT);
       
        currTime = std::chrono::high_resolution_clock::now().time_since_epoch();
        currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
    
    }
    
    return ret;    
    
}

// void sendFinishedTaskPacket(RetrieveTaskPacket *taskPacket, uint8_t taskStatus, int threadID, uint32_t getTaskTime, uint32_t serviceTime){
void sendFinishedTaskPacket(RetrieveTaskPacket *taskPacket, uint8_t taskStatus, int threadID, uint32_t getTaskTime, uint32_t serviceTime, uint64_t tasksExecuted){

    FinishedTaskPacket finishedPacket;
    
    finishedPacket.p4OperationCode = P4_TASK_FINISH_CODE;
    finishedPacket.userID = taskPacket->userID;
    finishedPacket.jobID = taskPacket->jobID;
    finishedPacket.taskID = taskPacket->taskID;
    finishedPacket.taskStatus = taskStatus;
    finishedPacket.userIP = taskPacket->userIP;
    finishedPacket.userPort = taskPacket->userPort;
    finishedPacket.time1 = taskPacket->time1;
    finishedPacket.time2 = 0;
    finishedPacket.getTaskTimeUs = getTaskTime;
    finishedPacket.serviceTimeUs = serviceTime;
    finishedPacket.schedulerDispatchTime = taskPacket->time2;

    // cout << "Received Task Packet: " << taskPacket.toString() << endl;
    // cout << "Sending finished packet: " << finishedPacket.toString() << endl;

    char *byteStream = finishedPacket.convertToByteStream();

    /************ filling ip_tuple and net_sge entry ************/

	ip_tuple id;

	inet_pton(AF_INET, srcIP, &id.src_ip );
	inet_pton(AF_INET, serverIP, &id.dst_ip );
	id.src_port = threadID * 1000 + 10000 + threadID ;
	 id.dst_port = 5555;
    // id.dst_port = dst_ports[tasksExecuted % SRVR_PORTS_NUM];
    
    id.src_port = rte_cpu_to_be_16(id.src_port);
    // id.dst_port = rte_cpu_to_be_16(id.dst_port);

    id.dst_ip = rte_cpu_to_be_32(id.dst_ip);
    id.src_ip = rte_cpu_to_be_32(id.src_ip);

    struct net_sge *entry;
	entry = alloc_net_sge();
	assert(entry);

    entry->len += FINISHED_PACKET_SIZE;

	rte_memcpy(entry->payload , (void *)byteStream, FINISHED_PACKET_SIZE);

	udp_send(entry, &id);

    // std::unique_lock<std::mutex> FinishQueueLock(_FinishQueueMutex);

    /*FinishedTaskPacket decodePacket;
    decodePacket.populateFromByteStream(byteStream);
    cout << "ByteStream Decode: " <<  decodePacket.toString() << endl;*/

    // FinishQueueLock.unlock();

    /* cout << "ByteStream for FinishedTaskPacket: ";
    for(int i = 0; i < FINISHED_PACKET_SIZE; i++)
        cout << to_string(byteStream[i])  << " ";
    cout << endl;*/

    // sendto(sockfd, byteStream, FINISHED_PACKET_SIZE, 0, (const struct sockaddr *) &servAddr, sizeof(servAddr));

    delete byteStream;

}

int threadMainOld(void *threadArg){

    workerThreadData *threadData = (workerThreadData *)threadArg;

    unsigned int runTimeMs = threadData->runTimeSec * 1000;

    threadTaskCounters[threadData->threadID] = 0;
    threadPacketCounters[threadData->threadID] = 0;
    uint64_t tasksExecuted = 0;
    uint64_t packetsRecvd = 0;

    int thread_id = threadData->threadID;
    //cout << "Thread " << thread_id << "  is using port " << thread_id * 1000 + 10000 + thread_id << endl;

    RTE_PER_LCORE(queue_id)= thread_id;

    auto startTime = std::chrono::system_clock::now().time_since_epoch();
    auto startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(startTime).count();

    auto currTime = std::chrono::system_clock::now().time_since_epoch();
    auto currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();

    // int threadSocketFd = createThreadSocket();

    RetrieveTaskPacket *emptyTask = createRetrieveTaskPacket();

    struct sockaddr_in servaddr = setupPseudoFalconServAddr();//*************************************************************************

    int queued;
    bool debugFlag = false;
    RetrieveTaskPacket* newTasks = new RetrieveTaskPacket[BATCH_SIZE];
    while(currTimeMs - startTimeMs < runTimeMs){
    // while(!debugFlag && (currTimeMs - startTimeMs < runTimeMs)){//debugging loop
        // cout << "Stuck in Thread Loop" << endl;
        queued = getTaskFromScheduler(0, thread_id, servaddr, emptyTask, startTimeMs, runTimeMs, newTasks);
	    
        if(queued > 1) printf("More than 1 task recieved on this thread");
        
	    for(int i = 0; i < queued; i++){
            RetrieveTaskPacket& newTask = newTasks[i];
            if(newTask.taskID == INVALID_TASK_ID){
                // cout << "Received task with ID: -1" << endl;            
            }
            else {
                if(newTask.functionID == FUNCTION_CODE_DROP){
                    tasksExecuted += 1;
                    packetsRecvd += 1;
                    // if (tasksExecuted == 5)
                        // debugFlag = true;
                }
                else if(newTask.functionID == FUNCTION_CODE_FLOAT){
                // TODO Change this to floating point computation
                    usleep(newTask.functionParameters * 1000);
                }    
                else{
                    packetsRecvd += 1;
                }
            }      
        }
        currTime = std::chrono::system_clock::now().time_since_epoch();
        currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
    }

    /*pthread_mutex_lock(&printLock);
    cout << "Total Tasks Executed by TID: " << threadData->threadID << " = " << tasksExecuted << endl;
    pthread_mutex_unlock(&printLock);*/

    threadTaskCounters[threadData->threadID] = tasksExecuted;
    threadPacketCounters[threadData->threadID] = packetsRecvd;

    // cout << "Worker Exit" << endl;

    delete emptyTask;

    return 0;
  //  pthread_exit(NULL);
}

int threadMain(void *threadArg){

    workerThreadData *threadData = (workerThreadData *)threadArg;
    RetrieveTaskPacket *newTask = createRetrieveTaskPacket();

    unsigned int threadRunTimeMs = threadData->runTimeSec * 1000;

    threadTaskCounters[threadData->threadID] = 0;
    threadPacketCounters[threadData->threadID] = 0;
    uint64_t tasksExecuted = 0;

    unsigned int threadID = threadData -> threadID;
    uint32_t threadAddResult = 0;

    RTE_PER_LCORE(queue_id) = threadID;

    // uint8_t workerConstraints = threadData->workerConstraints;

    // threadAddCounters[threadData->threadID] = 0;

    auto startTime = std::chrono::system_clock::now().time_since_epoch();
    auto startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(startTime).count();

    auto currTime = std::chrono::system_clock::now().time_since_epoch();
    auto currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();

    uint32_t currRunTimeMs = currTimeMs - startTimeMs;

    void *taskPacket;

    bool taskReceived = false;

    while(currRunTimeMs < threadRunTimeMs){
        // cout << "Curr Runtime Ms: " << currRunTimeMs << endl;
        // cout << "In Loop" << endl;
        
        /****************** Send retrieve packet *********************/
        /************ filling ip_tuple and net_sge entry ************/

        ip_tuple id;

        inet_pton(AF_INET, srcIP, &id.src_ip );
        inet_pton(AF_INET, serverIP, &id.dst_ip );
        id.src_port = threadID * 1000 + 10000 + threadID ;
        id.dst_port = 5555;
        // id.dst_port = dst_ports[tasksExecuted % SRVR_PORTS_NUM];
        
        id.src_port = rte_cpu_to_be_16(id.src_port);
        
        id.dst_ip = rte_cpu_to_be_32(id.dst_ip);
        id.src_ip = rte_cpu_to_be_32(id.src_ip);

        struct net_sge *entry;
        entry = alloc_net_sge();
        assert(entry);

        entry->len += sizeof(*newTask);

        rte_memcpy(entry->payload , (void *)newTask, sizeof(*newTask));
        
        auto getTaskStartTime = std::chrono::system_clock::now().time_since_epoch();
        auto getTaskStartTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(getTaskStartTime).count();

    	udp_send(entry, &id);
        taskReceived = false;
        int ret = 0 , i = 0;

        /************* Wait for task packet and run it *************/
        while(!taskReceived && (currRunTimeMs < threadRunTimeMs)){

            struct rte_mbuf *rx_pkts[BATCH_SIZE];
            ret = rte_eth_rx_burst(0, RTE_PER_LCORE(queue_id), rx_pkts, BATCH_SIZE);

            if(ret == 0 ){
                currTime = std::chrono::system_clock::now().time_since_epoch();
                currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
                currRunTimeMs = currTimeMs - startTimeMs;
                continue;
            }
            else {

                for (i = 0; i < ret; i++){ //This should only recieve 1 packet, will be changed to reflect that later

                    struct ip_tuple recv_id;
                    entry = eth_in(rx_pkts[i], &recv_id);

                    auto getTaskEndTime = std::chrono::system_clock::now().time_since_epoch();
                    auto getTaskEndTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(getTaskEndTime).count();

                    RetrieveTaskPacket *newTask  = (RetrieveTaskPacket *)entry->payload;

                    newTask->functionID = ntohs((uint16_t)newTask->functionID); 
                    newTask->functionParameters = ntohl((uint32_t)newTask->functionParameters); 
                    newTask->userIP = ntohl((uint32_t)newTask->userIP); 
                    newTask->time1 = ntohl((uint32_t)newTask->time1); 
                    newTask->time2 = ntohl((uint32_t)newTask->time2); 

                    newTask->jobID = ntohl((uint32_t)newTask->jobID); 
                    newTask->taskID = ntohs((uint16_t)newTask->taskID); 
                    newTask->userPort = ntohs((uint16_t)newTask->userPort);
                    // newTask->swapConsumeIndex = ntohl(newTask->swapConsumeIndex);
                    // newTask->offsetConsumeIndex = ntohl(newTask->offsetConsumeIndex);

                    if(newTask->taskID != INVALID_TASK_ID){
                        // cout << "GetTask: TaskID = " << receivedTaskPacket->taskID << endl; 
                        // cout << "Received Packet: " << commThreadID << ": " << receivedTaskPacket->toString() << endl;
                        // exit(0);
                        auto serviceStartTime = std::chrono::system_clock::now().time_since_epoch();
                        auto serviceStartTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(serviceStartTime).count();

                        if(newTask->functionID == FUNCTION_CODE_DROP){
                            tasksExecuted += 1;
                        }
                        else if(newTask->functionID == FUNCTION_CODE_SLEEP){
                            usleep(newTask->functionParameters * 1000);
                            tasksExecuted += 1;
                        }
                        else if(newTask->functionID == FUNCTION_CODE_FLOAT){
                            auto taskStartTime = std::chrono::system_clock::now().time_since_epoch();
                            auto taskStartTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(taskStartTime).count();
                            
                            // uint32_t runTimeUs = newTask->functionParameters * 1000;
                            uint32_t runTimeUs = newTask->functionParameters;
                            // usleep(runTimeUs);

                            auto taskCurrTime = std::chrono::system_clock::now().time_since_epoch();
                            auto taskCurrTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(taskCurrTime).count();

                            uint32_t getTaskTime =  getTaskEndTimeUs - getTaskStartTimeUs;
                            while((taskCurrTimeUs - taskStartTimeUs) < runTimeUs){
                                threadAddResult += 1;
                                taskCurrTime = std::chrono::system_clock::now().time_since_epoch();
                                taskCurrTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(taskCurrTime).count();
                            }
                            // cout << "Computation Packet received. Compute ran for " << (taskCurrTimeUs - taskStartTimeUs)/1000 << " and parameter was " << newTask->functionParameters << endl;

                            tasksExecuted += 1;
                            auto serviceEndTime = std::chrono::system_clock::now().time_since_epoch();
                            auto serviceEndTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(serviceEndTime).count();

                            uint32_t serviceTime = (serviceEndTimeUs - serviceStartTimeUs);
                            // cout << "GetTask: " << getTaskTime << " Service: " << serviceTime << endl;
                            // cout << "Worker: Before sendFinishedTaskPacket " << endl;
                            // sendFinishedTaskPacket(newTask, TASK_STATUS_FINISH, threadSocketFd, servaddr, newWorkPacket.getTaskTime, serviceTime, workerNodeID);
                            // sendFinishedTaskPacket(newTask, TASK_STATUS_FINISH, threadID, getTaskTime, serviceTime);
                            sendFinishedTaskPacket(newTask, TASK_STATUS_FINISH, threadID, getTaskTime, serviceTime,tasksExecuted);

                            // cout << "Worker: After sendFinishedTaskPacket " << endl;

                            // auto afterSendTime = std::chrono::system_clock::now().time_since_epoch();
                            // auto afterSendTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(serviceEndTime).count();

                            // uint32_t afterSendTimeMs = ((double)afterSendTimeUs - (double)serviceStartTimeUs)/1000;

                            // cout << "WorkThread: Finish Queue Size: " << finishedQueue.size() << endl;
                            }    
                        else{
                            std::cout << "Unknown function ID received: " << newTask->functionID << "for job and task ID: " << newTask->jobID << " " << newTask->taskID << endl;
                        }

                        rte_pktmbuf_free((rte_mbuf*)entry->handle);

                    } 
                    else{
                        // cout << "Invalid Task" << endl;
                        // delete receivedTaskPacket;
                        rte_pktmbuf_free((rte_mbuf*)entry->handle);
                    }
                }

                currTime = std::chrono::system_clock::now().time_since_epoch();
                currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
                currRunTimeMs = currTimeMs - startTimeMs;
                break;
            }   
            currTime = std::chrono::system_clock::now().time_since_epoch();
            currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
            currRunTimeMs = currTimeMs - startTimeMs;
        }
        
        currTime = std::chrono::system_clock::now().time_since_epoch();
        currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
        currRunTimeMs = currTimeMs - startTimeMs;
    }

    /*pthread_mutex_lock(&printLock);
    cout << "Total Tasks Executed by TID: " << threadData->threadID << " = " << tasksExecuted << endl;
    pthread_mutex_unlock(&printLock);*/

    threadTaskCounters[threadData->threadID] = tasksExecuted;
    // threadAddCounters[threadData->threadID] = threadAddResult;
    // cout << "Worker Thread Exiting" << endl;
    delete newTask;

    return 0;
}

int main(int argc, char *argv[]){
    int runTimeSec = atoi(argv[1]);
    
    workerThreadData threadData[NUM_THREADS];
    int rc;
    void *threadStatus;

    cout << "Attempting to Init EAL args for Falcon C++ backend" << endl;

	dpdk_init(&argc,&argv); // need to remove argv[1] from args

	/* RTE flow creation */
	uint8_t port_id = 0;
    
	for (int i = 0; i < rte_lcore_count(); i++) {
		// cout << "generating flow : portID " << port_id << " RX queue id  " << i << " udp port linked " << i * 1000 + 10000  + i << endl;
		generate_udp_flow(port_id, i , i * 1000 + 10000  + i, 0xffff);
	}

    rte_flow_isolate(0, 1, NULL);

    cout << "Attempting to start Falcon C++ backend" << endl;

    int i = 0;
    unsigned lcore_id;

    threadData[i].threadID = i;
    threadData[i].runTimeSec = runTimeSec;

    i++;

    RTE_LCORE_FOREACH_SLAVE(lcore_id)
	{
        threadData[i].threadID = i;
        threadData[i].runTimeSec = runTimeSec;

        rte_eal_remote_launch(threadMain, (void *)&threadData[i], lcore_id);
        i++;
    }

    threadMain((void *)&threadData[0]);

    RTE_LCORE_FOREACH_SLAVE(lcore_id)
    {
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            break;
        }
    }

    uint64_t totalTasksExecuted = 0;
    for(int i = 0; i < NUM_THREADS; i++){
    	// cout << "Thread " << i << " Executed : " << threadTaskCounters[i] << " Tasks" <<  endl;
	totalTasksExecuted += threadTaskCounters[i];
    }
    uint64_t totalPacketsExecuted = 0;
    for(int i = 0; i < NUM_THREADS; i++){
        // cout << "Thread " << i << " Recieved : " << threadPacketCounters[i] << " Tasks" <<  endl;
        totalPacketsExecuted += threadPacketCounters[i];
    }

    cout << "Total Tasks Executed by Worker Node: " << totalTasksExecuted << endl;
    cout << "Total Packets recieved by Worker Node: " << totalPacketsExecuted << endl;
    cout << "Worker Execution Complete. Exiting." << endl;
    
}

