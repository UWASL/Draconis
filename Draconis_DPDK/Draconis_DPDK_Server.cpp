#include <iostream>
#include <pthread.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <chrono>
#include <assert.h>
#include <queue>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>

#include <unistd.h>

#include "draconis_util.h"
#include "dpdk.h"

#include <rte_service.h>

using namespace std;

#define SERVER_THREADS 32 
#define RECV_THREADS_NUM 8 
#define SEND_THREADS_NUM 8 
#define LOG_INTERVALS_SECONDS 60

// #define NUM_PRIORITIES 1  // MOVED TO DPDK.H
#define QUEUE_SIZE 64000

int scheduledTasks = 0;
int finishedNotifs = 0;
uint64_t submitted_tasks = 0;

pthread_mutex_t submissionLock; 

struct TaskEntry {
    uint8_t userID;
    uint32_t jobID;
    uint16_t taskID;
    uint32_t functionID;
    uint32_t functionParams;
    uint32_t clientIP;
    uint16_t clientPort;
    uint32_t taskSubmitTime;
};

struct StreamEntry {
    uint8_t p4OperationCode;
    char *requestStream;
    struct sockaddr_in reqCliAddr;
    struct ip_tuple id;
};

queue<TaskEntry> taskList[NUM_PRIORITIES];
pthread_mutex_t taskListLocks[NUM_PRIORITIES];


queue<StreamEntry> inputWorkStream;
queue<StreamEntry> outputWorkStream;

pthread_mutex_t inputStreamLock;
pthread_mutex_t outputStreamLock;


struct threadArgs{
    int threadID;
};

void handleJobSubmission(StreamEntry *workEntryPtr){
    StreamEntry workEntry = *workEntryPtr;

    assert(workEntry.p4OperationCode == P4_SUBMIT_TASK_CODE);

    uint16_t numTasks = getNumTasksFromSubmitByteStream(workEntry.requestStream);
    SubmitPacket submitPacket(numTasks);
    submitPacket.populateFromByteStream(workEntry.requestStream);

    /*pthread_mutex_lock(&submissionLock);
    submitted_tasks += numTasks;
    cout << "Total tasks received: " << submitted_tasks << endl;
    pthread_mutex_unlock(&submissionLock);*/

    /*pthread_mutex_lock(&inputStreamLock);

        cout << "worker request stream : ";   
        for (int i = 0 ; i < SUBMIT_PACKET_BASE_SIZE + (numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE);i++)
            cout << (unsigned)(workEntry.requestStream[i]) << " "; 
        cout << endl;   
    pthread_mutex_unlock(&inputStreamLock);
*/
    assert(submitPacket.p4OperationCode == P4_SUBMIT_TASK_CODE);

    for(uint16_t i = 0; i < submitPacket.numTasks; i++){
        TaskEntry *newTask = new TaskEntry();
        newTask->userID = submitPacket.userID;
        newTask->jobID = submitPacket.jobID;
        newTask->taskID = submitPacket.tasks[i].taskID;
        newTask->functionID = submitPacket.tasks[i].functionID;
        newTask->functionParams = submitPacket.tasks[i].functionParams;
        //newTask->clientIP = workEntry.reqCliAddr.sin_addr.s_addr;
        //newTask->clientPort = workEntry.reqCliAddr.sin_port;
        newTask->clientIP = workEntry.id.src_ip;
        newTask->clientPort = workEntry.id.src_port;
 
        auto taskSubmitTime = std::chrono::system_clock::now().time_since_epoch();
        newTask->taskSubmitTime = std::chrono::duration_cast<std::chrono::nanoseconds>(taskSubmitTime).count();

        uint8_t submitPriority = 0;

        if(submitPriority > NUM_PRIORITIES - 1){
            cout << "Invalid Priority in Submission" << endl;
            continue;
        }
        else
        {
           // pthread_mutex_lock(&taskListLocks[submitPriority]);
            // taskList[submitPriority].push(newTask);
            rte_ring_enqueue(task_list_ring[submitPriority], (void *)newTask);
            //pthread_mutex_unlock(&taskListLocks[submitPriority]);
        }
        
    }

    delete workEntry.requestStream;
    delete workEntryPtr;

}

void handleTaskRetrieval(StreamEntry *workEntry){
    // StreamEntry workEntry = *workEntryPtr;

    assert(workEntry->p4OperationCode = P4_RETRIEVE_TASK_CODE);
    // cout << "Received Task Request" << endl;

    uint8_t currPriority = 0;
    bool taskRetrieved = false;
    TaskEntry *t;

    RetrieveTaskPacket returnPacket = RetrieveTaskPacket();

    for(currPriority = 0; currPriority < NUM_PRIORITIES; currPriority++){
        if(!rte_ring_empty(task_list_ring[currPriority])){
            //pthread_mutex_lock(&taskListLocks[currPriority]);

            if(rte_ring_empty(task_list_ring[currPriority]))
                //pthread_mutex_unlock(&taskListLocks[currPriority]);
		continue;
            else{
                // t = taskList[currPriority].front();
                // taskList[currPriority].pop();
                
                void *msg;
                if (rte_ring_dequeue(task_list_ring[currPriority], &msg)< 0){
                    //pthread_mutex_unlock(&taskListLocks[currPriority]);
                    continue;
                }
                t = (TaskEntry *)msg;
                
                taskRetrieved = true;
                scheduledTasks += 1;
                // cout << "Scheduled Tasks: " << scheduledTasks << endl;                
        //        pthread_mutex_unlock(&taskListLocks[currPriority]);
            }              
        }
        if(taskRetrieved)
            break;
    }
    
    if(currPriority == NUM_PRIORITIES - 1 && taskRetrieved == false){
        // Do nothing which essentially sends back an empty RetrieveTaskPacket
        // cout << "Sending empty packet" << endl;
    }
    else if (taskRetrieved == true){
        
        // Populate RetrieveTaskPacket to send back
        // cout << "Retrieved Task. Pushing it to output queue" << endl;
        returnPacket.functionID = t->functionID;
        returnPacket.functionParameters = t->functionParams;
        returnPacket.jobID = t->jobID;
        returnPacket.taskID = t->taskID;
        returnPacket.userID = t->userID;
        returnPacket.userIP = t->clientIP;
        returnPacket.userPort = t->clientPort;
        returnPacket.time1 = t->taskSubmitTime;

        auto taskRetrieveTime = std::chrono::system_clock::now().time_since_epoch();
        returnPacket.time2 = std::chrono::duration_cast<std::chrono::nanoseconds>(taskRetrieveTime).count();

    }

    workEntry->requestStream = returnPacket.convertToByteStream();
    
 //   pthread_mutex_lock(&outputStreamLock);
    // outputWorkStream.push(workEntry);
    //cout << "Handle Retrieve enqueue " << endl;

    rte_ring_enqueue(output_stream_ring, (void *)workEntry);
   // pthread_mutex_unlock(&outputStreamLock);
}

void handleFinishedTask(StreamEntry *workEntry){

    // StreamEntry workEntry = *workEntryPtr;

    assert(workEntry->p4OperationCode = P4_TASK_FINISH_CODE);

    FinishedTaskPacket finPack;
    finPack.populateFromByteStream(workEntry->requestStream);

    assert(finPack.p4OperationCode == P4_TASK_FINISH_CODE);

    auto taskFinishTime = std::chrono::system_clock::now().time_since_epoch();
    finPack.time2 = std::chrono::duration_cast<std::chrono::nanoseconds>(taskFinishTime).count();

    delete workEntry->requestStream;    
    workEntry->requestStream = finPack.convertToByteStream();

    struct sockaddr_in newClient;
    newClient.sin_addr.s_addr = finPack.userIP;
    newClient.sin_port = finPack.userPort;
    newClient.sin_family = AF_INET;

    workEntry->reqCliAddr = newClient;

//    pthread_mutex_lock(&outputStreamLock);
    // outputWorkStream.push(workEntry);
    //cout << "Handle Finished Enqueue " << endl;
    rte_ring_enqueue(output_stream_ring, (void *)workEntry);
//    pthread_mutex_unlock(&outputStreamLock);
}

int workerThreadMain(void *args){

    threadArgs *argStruct = (threadArgs *)args;
    int threadID = argStruct->threadID;    
    RTE_PER_LCORE(queue_id) = threadID;

    /*int finCount = 0;
    int submitCount = 0;
    int logIntervalsMs = LOG_INTERVALS_SECONDS * 1000;

    auto startTime = std::chrono::system_clock::now().time_since_epoch();
    auto startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(startTime).count();

    auto currTime = std::chrono::system_clock::now().time_since_epoch();
    auto currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
*/
    while(true){

  //      currTime = std::chrono::system_clock::now().time_since_epoch();
    //    currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();

      /*  if ((currTimeMs - startTimeMs) >=  logIntervalsMs){
            startTime = std::chrono::system_clock::now().time_since_epoch();
            startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(startTime).count();

            pthread_mutex_lock(&inputStreamLock);

            cout << "Worker ID " << threadID << " Handled : " << submitCount << " Submit Packets " << endl;
            cout << "Worker ID " << threadID << " Handled : " << finCount << " Finished Packets " << endl;

            pthread_mutex_unlock(&inputStreamLock);

        }
*/
        if(!rte_ring_empty(input_stream_ring)){
//            pthread_mutex_lock(&inputStreamLock);
            
            if(rte_ring_empty(input_stream_ring))
//                pthread_mutex_unlock(&inputStreamLock);
		continue;
            else{
                void *msg;
                if (rte_ring_dequeue(input_stream_ring, &msg)< 0){
		    //cout << "dequeue failed " << endl;
//                    pthread_mutex_unlock(&inputStreamLock);
                    continue;
                }
                StreamEntry *workEntry = (StreamEntry *)msg;
//                pthread_mutex_unlock(&inputStreamLock);

                if(workEntry->p4OperationCode == P4_SUBMIT_TASK_CODE){
		                // cout << "Handle Submit " << endl;
    //                submitCount++;
                    handleJobSubmission(workEntry);}
                else if(workEntry->p4OperationCode == P4_RETRIEVE_TASK_CODE){
                    // cout << "Handle Ret " << endl;
                    handleTaskRetrieval(workEntry);}
                else if(workEntry->p4OperationCode == P4_TASK_FINISH_CODE){
                   // cout << "Handle Fin " << endl;
  //                  finCount++;

                    handleFinishedTask(workEntry);}
            }
        }
    }  

    return 0;
}

int receiverThreadMain(void *args){

    int maxSize = SUBMIT_PACKET_BASE_SIZE + FALCON_SW_SERV_MAX_TASKS_PER_SUBMIT * SUBMIT_PACKET_TASK_HEADER_SIZE;

    threadArgs *argStruct = (threadArgs *)args;
    int threadID = argStruct->threadID;

    RTE_PER_LCORE(queue_id) = threadID;

    int recvSockFd = createThreadSocket();
    struct sockaddr_in servaddr, cliaddr;
    memset(&servaddr, 0, sizeof(servaddr)); 
    memset(&cliaddr, 0, sizeof(cliaddr)); 
      
    // Filling server information 
    servaddr.sin_family    = AF_INET; // IPv4 
    servaddr.sin_addr.s_addr = INADDR_ANY; 
    servaddr.sin_port = htons(SCHEDULER_PORT);

    char recvStream[maxSize];
    socklen_t len = sizeof(cliaddr);
    
    uint8_t p4OpCode;
    int ret, i;
    struct net_sge *entry;

    while(true){

        struct rte_mbuf *rx_pkts[BATCH_SIZE];
        ret = rte_eth_rx_burst(0, RTE_PER_LCORE(queue_id), rx_pkts, BATCH_SIZE);

        if (ret == 0 ){
            continue;
        }
        for (i = 0; i < ret; i++){

            // int recvLen = recvfrom(recvSockFd, (char *)&p4OpCode, sizeof(p4OpCode), MSG_PEEK | MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
            StreamEntry *s = new StreamEntry();
            struct ip_tuple id ;
            entry = eth_in(rx_pkts[i], &id);//Need to return client address here
            
            if(entry->len == FINISHED_PACKET_SIZE){
                FinishedTaskPacket *finishedTaskPacket = (FinishedTaskPacket *)entry->payload;
                p4OpCode = (uint8_t)finishedTaskPacket->p4OperationCode;
                if (p4OpCode != P4_TASK_FINISH_CODE){
                    rte_pktmbuf_free((rte_mbuf*)entry->handle);
                    continue;
                }

                s->p4OperationCode = p4OpCode;
                s->requestStream = new char[FINISHED_PACKET_SIZE];
                rte_memcpy(s->requestStream, (char *)entry->payload, FINISHED_PACKET_SIZE);
                s->id.dst_ip = rte_cpu_to_be_32((uint32_t)finishedTaskPacket->userIP);
                s->id.dst_port = rte_cpu_to_be_16((uint16_t)finishedTaskPacket->userPort);
		//s->id.dst_ip = (uint32_t)finishedTaskPacket->userIP;
                //s->id.dst_port = (uint16_t)finishedTaskPacket->userPort;
                s->id.src_ip = id.src_ip;
                s->id.src_port = id.src_port;
                

               /* pthread_mutex_lock(&inputStreamLock);
                cout << "REC FIN packet from " << id.src_ip << " and port " << id.src_port << endl;
                cout << "FIN packet ip " << (uint32_t)finishedTaskPacket->userIP << " Fin packet port " << (uint16_t)finishedTaskPacket->userPort << endl;
                cout << "REC FIN sending it to " << s->id.dst_port << " and port " << s->id.dst_ip << endl;
		pthread_mutex_unlock(&inputStreamLock);
                 */             
              
                rte_pktmbuf_free((rte_mbuf*)entry->handle);
            }
            else if(entry->len == RETRIEVE_TASK_PACKET_SIZE){
                RetrieveTaskPacket *retrieveTaskPacket = (RetrieveTaskPacket *)entry->payload;
                p4OpCode = (uint8_t)retrieveTaskPacket->p4OperationCode;
                if (p4OpCode != P4_RETRIEVE_TASK_CODE){
                    rte_pktmbuf_free((rte_mbuf*)entry->handle);
                    continue;
                }

                s->p4OperationCode = p4OpCode;
                s->requestStream = nullptr;

                s->id.dst_ip = id.src_ip;
                s->id.dst_port = id.src_port;
                s->id.src_ip = id.dst_ip;
                s->id.src_port = id.dst_port;

                rte_pktmbuf_free((rte_mbuf*)entry->handle);
            }
            else {
                SubmitPacket *submitPacket = (SubmitPacket *)entry->payload;
                uint16_t numTasks = ntohs((uint16_t)submitPacket->numTasks);
                p4OpCode = (uint8_t)submitPacket->p4OperationCode;
                if (p4OpCode != P4_SUBMIT_TASK_CODE){
                    rte_pktmbuf_free((rte_mbuf*)entry->handle);
                    continue;
                }
                // cout << "Recieved Submit packet from " << id.src_ip << " on port " << id.dst_port << endl;

                s->p4OperationCode = p4OpCode;
                s->requestStream = new char[SUBMIT_PACKET_BASE_SIZE + (numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE)];
                rte_memcpy(s->requestStream, (char *)entry->payload, SUBMIT_PACKET_BASE_SIZE + (numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE));
           
                s->id.dst_ip = id.dst_ip;
                s->id.dst_port = id.dst_port;
                s->id.src_ip = id.src_ip;
                s->id.src_port = id.src_port;

  /*              pthread_mutex_lock(&inputStreamLock);

                cout << "REC Sub packet from " << id.src_ip << " and port " << id.src_port << endl;
                cout << "REC Sub sending it to " << s->id.dst_ip << " and port " << s->id.dst_port << endl;

                cout << "after copy request stream : ";   
                for (int i = 0 ; i < SUBMIT_PACKET_BASE_SIZE + (numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE);i++)
                    cout << (unsigned)(s->requestStream[i]) << " "; 
                cout << endl;   

                pthread_mutex_unlock(&inputStreamLock);
*/
                rte_pktmbuf_free((rte_mbuf*)entry->handle);
            }
            

            // if(p4OpCode == P4_SUBMIT_TASK_CODE){
            //     // cout << "Received OpCode: " << to_string(p4OpCode) << endl;
            
            //     recvLen = recvfrom(recvSockFd, &recvStream, SUBMIT_PACKET_BASE_SIZE, MSG_PEEK | MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
            //     uint16_t numTasks = getNumTasksFromSubmitByteStream(recvStream);

            //     recvLen = recvfrom(recvSockFd, &recvStream, SUBMIT_PACKET_BASE_SIZE + (numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE), MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);

            //     s.p4OperationCode = p4OpCode;
            //     s.requestStream = new char[SUBMIT_PACKET_BASE_SIZE + (numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE)];
            //     memcpy(s.requestStream, recvStream, SUBMIT_PACKET_BASE_SIZE + (numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE));
            //     s.reqCliAddr = cliaddr;

            //     /*SubmitPacket newPacket(numTasks);
            //     newPacket.populateFromByteStream(recvStream);
                
            //     cout << "Decoded Packet: " << newPacket.toString() << endl;*/

            // }

            // else if(p4OpCode == P4_TASK_FINISH_CODE){
            //     // cout << "Received OpCode: " << to_string(p4OpCode) << endl;
            
            //     recvLen = recvfrom(recvSockFd, &recvStream, FINISHED_PACKET_SIZE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);

            //     s.p4OperationCode = p4OpCode;
            //     s.requestStream = new char[FINISHED_PACKET_SIZE];
            //     memcpy(s.requestStream, recvStream, FINISHED_PACKET_SIZE);
            //     s.reqCliAddr = cliaddr;
            // }

            // else if(p4OpCode == P4_RETRIEVE_TASK_CODE){
            //     recvLen = recvfrom(recvSockFd, &recvStream, RETRIEVE_TASK_PACKET_SIZE, MSG_WAITALL, (struct sockaddr *)&cliaddr, &len);
                
            //     s.p4OperationCode = p4OpCode;
            //     s.requestStream = nullptr;
            //     s.reqCliAddr = cliaddr;
            // }

//            pthread_mutex_lock(&inputStreamLock);
            
            // void* temp = (void *)&s;
            // StreamEntry testTemp = *((StreamEntry *)temp);
            // uint16_t numTasks = getNumTasksFromSubmitByteStream(testTemp.requestStream);

            //     cout << "RECV stream : ";   
            //     for (int i = 0 ; i < SUBMIT_PACKET_BASE_SIZE + (numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE);i++)
            //         cout << (unsigned)(testTemp.requestStream[i]) << " "; 
            //     cout << endl;   

            rte_ring_enqueue(input_stream_ring, (void *)s);
            // inputWorkStream.push(s);
            // inQueue.push(s);

            // void *msg;
            // if (rte_ring_dequeue(input_stream_ring, &msg) < 0) {
            //     cout << "dequeue failed " << endl;
            // }
            // StreamEntry tempEntry = *((StreamEntry *)msg);

            // uint16_t deqNumTasks = getNumTasksFromSubmitByteStream(tempEntry.requestStream);
            // cout << "RECV numTasks : " << numTasks << " DEQUEUE numTasks : " << deqNumTasks << endl;   

            // cout << "DEQUEUE stream : ";   
            //     for (int i = 0 ; i < SUBMIT_PACKET_BASE_SIZE + (deqNumTasks * SUBMIT_PACKET_TASK_HEADER_SIZE);i++)
            //         cout << (unsigned)(tempEntry.requestStream[i]) << " "; 
            //     cout << endl; 

//            pthread_mutex_unlock(&inputStreamLock);
        }
    }

    return 0;

}

int senderThreadMain(void *args){

    int sendSockFd = createThreadSocket();

    threadArgs *argStruct = (threadArgs *)args;
    int threadID = argStruct->threadID;

    RTE_PER_LCORE(queue_id) = threadID;

    while(true){
        if(!rte_ring_empty(output_stream_ring)){
//            pthread_mutex_lock(&outputStreamLock);
            // StreamEntry s = outputWorkStream.front();
            // outputWorkStream.pop();
            void *msg;
            if (rte_ring_dequeue(output_stream_ring, &msg)< 0){
//                pthread_mutex_unlock(&outputStreamLock);
                continue;
            }
            StreamEntry *tempPtr = (StreamEntry *)msg;
            StreamEntry s = *tempPtr;
            
//            pthread_mutex_unlock(&outputStreamLock);
            struct net_sge *entry;
            entry = alloc_net_sge();
            assert(entry);

            if(s.p4OperationCode == P4_TASK_FINISH_CODE){
                finishedNotifs += 1;

            //    cout << "Finished Notifs Sent: " << finishedNotifs << endl;
	//	cout << "Sending Finished to ip " << s.id.dst_ip << " on Port " << s.id.dst_port << endl;
                entry->len += FINISHED_PACKET_SIZE;
                rte_memcpy(entry->payload , (void *)s.requestStream, FINISHED_PACKET_SIZE);
            	udp_send(entry, &s.id);

                // sendto(sendSockFd, s.requestStream, FINISHED_PACKET_SIZE, 0, (const struct sockaddr *) &s.reqCliAddr, sizeof(s.reqCliAddr));
                // cout << "Sending task finish notif" << endl;

            }
            else if(s.p4OperationCode == P4_RETRIEVE_TASK_CODE){
                entry->len += RETRIEVE_TASK_PACKET_SIZE;

	rte_memcpy(entry->payload , (void *)s.requestStream, RETRIEVE_TASK_PACKET_SIZE);
            	udp_send(entry, &s.id);

                // sendto(sendSockFd, s.requestStream, RETRIEVE_TASK_PACKET_SIZE, 0, (const struct sockaddr *) &s.reqCliAddr, sizeof(s.reqCliAddr));    
                //cout << "Sending retrieval response" << endl;
            }
        }
    }   

    return 0;

}

int main(int argc, char *argv[]){
    pthread_t recvThread, senderThread;
    int rc;
    void *recvThreadStatus, *senderThreadStatus;

    pthread_t serverThreads[SERVER_THREADS];
    void *serverThreadStatus[SERVER_THREADS];
    threadArgs threadArgsStruct[RECV_THREADS_NUM];
   
 
    dpdk_init(&argc,&argv);

	/* RTE flow creation */
	uint8_t port_id = 0;

	for (int j = 0; j < rte_lcore_count(); j++) {
		cout << "generating flow : RX queue id  " << j << " udp port linked " << j * 1000  + 10000 + j << endl;
		generate_udp_flow(port_id, j , j * 1000 + 10000  + j , 0xffff);
	}
    rte_flow_isolate(0, 1, NULL);


    int x = 0;
    unsigned lcore_id;

    RTE_LCORE_FOREACH_SLAVE(lcore_id){
        if (lcore_id < RECV_THREADS_NUM +2){
            threadArgsStruct[x].threadID = x + 1;
	    cout << "lcore_id : " << lcore_id <<  " i : " << x << " Condition " <<  lcore_id << " < " << RECV_THREADS_NUM + 2 << " : " << (lcore_id < RECV_THREADS_NUM + 2) << " Thread ID " << threadArgsStruct[x].threadID << " running a RECV thread" << endl; 
            rte_eal_remote_launch(receiverThreadMain, (void *)&threadArgsStruct[x], lcore_id);
            x++;
        }else if (lcore_id < (RECV_THREADS_NUM + SEND_THREADS_NUM + 2)){
            threadArgsStruct[x].threadID = x + 1;
            cout << "Thread ID " << threadArgsStruct[x].threadID << " running a SEND thread" << endl;
            rte_eal_remote_launch(senderThreadMain, (void *)&threadArgsStruct[x], lcore_id);
            x++;
        }else {
		if ( lcore_id == (SERVER_THREADS + RECV_THREADS_NUM + SEND_THREADS_NUM + 2))
			break;
            threadArgsStruct[x].threadID = x + 1;
            cout << "Thread ID " << threadArgsStruct[x].threadID << " running a WORKER thread" << endl;
            rte_eal_remote_launch(workerThreadMain, (void *)&threadArgsStruct[x], lcore_id);
	    x++;
		
        }
    }

	
	RTE_LCORE_FOREACH_SLAVE(lcore_id)
	{
		if (rte_eal_wait_lcore(lcore_id) < 0) {
			break;
		}
	}

    // rc = pthread_create(&recvThread, NULL, receiverThreadMain, nullptr);
    //     if(rc) {
    //         cout << "Receiver Thread Creation Failed" << endl;
    //         exit(-1);
    //     }
    // rc = pthread_create(&senderThread, NULL, senderThreadMain, nullptr);
    //     if(rc) {
    //         cout << "Sender Thread Creation Failed" << endl;
    //         exit(-1);
    //     }
    
    // for(int i = 0; i < SERVER_THREADS; i++){
    //     rc = pthread_create(&serverThreads[i], NULL, workerThreadMain, nullptr);
    //     if(rc) {
    //         cout << "Server Thread Creation Failed" << endl;
    //         exit(-1);
    //     }    
    // }


    // rc = pthread_join(recvThread, &recvThreadStatus);
    //      if(rc) {
    //         cout << "Receiver Thread Join Failed" << endl;
    //         exit(-1);
    //     }
    // rc = pthread_join(senderThread, &senderThreadStatus);
    //      if(rc) {
    //         cout << "Sender Thread Join Failed" << endl;
    //         exit(-1);
    //     }
    
    // for(int i = 0; i < SERVER_THREADS; i++){
    //     rc = pthread_join(serverThreads[i], &serverThreadStatus[i]);
    //      if(rc) {
    //         cout << "Server Thread Join Failed" << endl;
    //         exit(-1);
    //     }    
    // }
    
    return 0;
}

