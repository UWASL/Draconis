#include <iostream>
#include <fstream>
#include <pthread.h>
#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>
#include <chrono>

#include <vector>
#include <map>

#include <string>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>
#include <unistd.h>

#include "draconis_util.h"
#include "dpdk.h"

#define DEFAULT_USER_ID 1

#define MAX_RECV_DELAY 5
#define RECV_THREADS_NUM 14
#define SRVR_PORTS_NUM 5 

using namespace std;

const char* srcIP = "192.168.126.112";
const char* serverIP = "192.168.126.55";

uint32_t totalTasksToLaunch = 0;
uint32_t finishedTasks = 0;
uint32_t finishedTasksPerThread[RECV_THREADS_NUM];
uint32_t tasksLostQueueFull = 0;
uint32_t tasksLostQueueFullPerThread[RECV_THREADS_NUM];

uint32_t negativeSchedTimeTasks = 0;

int64_t startTimeMs;

const int src_ports[17] = {11001,12002,13003,14004,15005,16006,17007,18008,19009,20010,21011,22012,23013,24014,25015,26016,27017};
// const int src_ports[4] = {11001,12002,13003,14004};
//const int dst_ports[14] = {6555,7555,8555,9555,10555,11555,12555,13555,14555,15555};
const int dst_ports[14] = {5955,6155,6355,6555,6755,6955,7155,7355,7555,7755,7955,8155};


pthread_mutex_t finishedLock;

struct submitJobArgs{
    int threadID;
    struct sockaddr_in servAddr;
};

struct handleMessageArgs{
    int threadID;
    double runTimeSec;
};

struct jobEntry {
    string jobID;
    double submissionTime;
    double runTime;
    int numTasks;
    int priority;

    jobEntry(string _jobID, double _submissionTime, double _runTime, int _numTasks, int _priority){
        this->jobID = _jobID;
        this->submissionTime = _submissionTime;
        this->runTime = _runTime;
        this->numTasks = _numTasks;
        this->priority = _priority;
    }
    string toString(){
        return jobID + ":" + to_string(submissionTime) + ";" + to_string(runTime) + ";" + to_string(numTasks) + ";" + to_string(priority);
    }
};

std::vector<jobEntry> *jobList;

void jobListOutput(){
    for(jobEntry j: *jobList){
        cout << j.toString() << endl;
    }
}

struct taskCompletionDetails {
    uint16_t taskID;

    uint8_t taskStatus;
    uint32_t schedulerSubmitTime;
    uint32_t schedulerEndTime;
    uint32_t schedulerDispatchTime;
    // uint32_t clientSendTimeMs;
    // uint32_t clientRecieveTimeMs;
    float getTaskTimeMs;
    float serviceTimeMs;

    string toString(){
        string returnString = "{";
        returnString += string("TaskID:").append(to_string(taskID).append(","));
        returnString += string("TaskStatus:").append(to_string(taskStatus).append(","));
        returnString += string("SchedSubmitTime:").append(to_string(schedulerSubmitTime).append(","));
        returnString += string("SchedEndTime:").append(to_string(schedulerEndTime).append(","));
        returnString += string("SchedDispatchTime:").append(to_string(schedulerDispatchTime).append(","));
        // returnString += string("clientSendTimeMs:").append(to_string(clientSendTimeMs).append(","));
        // returnString += string("clientRecieveTimeMs:").append(to_string(clientRecieveTimeMs).append(","));
        returnString += string("GetTaskTime:").append(to_string(getTaskTimeMs).append(","));      
        returnString += string("ServiceTime:").append(to_string(serviceTimeMs).append(","));
        returnString+= string("}");

        return returnString;
    }
};

map<string, uint32_t > taskSubmissionTimeMap;
map<string, vector<taskCompletionDetails>> taskCompletionMap;
map<string, vector<taskCompletionDetails>> taskCompletionMapPerThread[RECV_THREADS_NUM];

string taskCompletionMapOutput(){
    //cout << "Task Completion Map Output: " << endl;
    string returnString = "{";
    for(auto it = taskCompletionMap.begin(); it != taskCompletionMap.end(); it++){
        returnString += it->first + ":[";
        for(auto taskEntry: it->second)
            returnString += taskEntry.toString();
        returnString += "]";
    }
    returnString += "},";
    return returnString;
}

void readInputFile(string inputFilePath){
    string lineFromFile;

    jobList = new vector<jobEntry>();

    ifstream inFileStream;
    inFileStream.open(inputFilePath, ios::in);

    if(inFileStream.is_open()){
        getline(inFileStream, lineFromFile);
        while(getline(inFileStream, lineFromFile)){
            vector<string> lineSplit = stringSplit(lineFromFile, ",");

            string jobID = lineSplit[0];
            double submissionTimeMs = stod(lineSplit[1]);
            double runTimeMs = stod(lineSplit[2]);
            int numTasks = stoi(lineSplit[3]);

            totalTasksToLaunch += numTasks;
            jobEntry newEntry(jobID, submissionTimeMs, runTimeMs, numTasks, 0);

            jobList->push_back(newEntry);
                        
        }
        //jobListOutput();

    }
    else {
        cout << "Error in opening the specified file path" << endl;
    }

    inFileStream.close();

}

// void *submitJobs(void *funcArgs){
int submitJobs(void *funcArgs){
 /*
  * Pulls jobs from jobList, convert them into submitPackets and send them across
  * Probably better to multi-thread this using batches of jobs from the list
  * Needs to convert from host order to network order
  */
    submitJobArgs *argStruct = (submitJobArgs *)funcArgs;

    // int sockfd = argStruct->sockfd;
    struct sockaddr_in servAddr = argStruct->servAddr; 

    uint32_t jobSubmissions = 0;

    RTE_PER_LCORE(queue_id) = 1;

    ip_tuple id;

    inet_pton(AF_INET, srcIP, &id.src_ip );
    inet_pton(AF_INET, serverIP, &id.dst_ip );
    id.src_port = 11001; //11001 , 12002
    id.dst_port = 5555;
    
    id.src_port = rte_cpu_to_be_16(id.src_port);
    // id.dst_port = rte_cpu_to_be_16(id.dst_port);
    
    id.dst_ip = rte_cpu_to_be_32(id.dst_ip);
    id.src_ip = rte_cpu_to_be_32(id.src_ip);

    struct net_sge *entry;

    auto startTime = std::chrono::system_clock::now().time_since_epoch();
    startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(startTime).count();
    auto startTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(startTime).count();

    auto currTime = std::chrono::system_clock::now().time_since_epoch();
    auto currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
    auto currTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(currTime).count();

    for(jobEntry j: *jobList){
        // SubmitPacket *newSubmit = new SubmitPacket(1);
        id.src_port = src_ports[jobSubmissions % RECV_THREADS_NUM];
        id.src_port = rte_cpu_to_be_16(id.src_port);
        // id.dst_port = src_ports[jobSubmissions % SRVR_PORTS_NUM];
        // id.dst_port = rte_cpu_to_be_16(id.dst_port);

//        cout << "Job Submission Time: " << j.submissionTime << endl;
        
        while((currTimeUs - startTimeUs) < j.submissionTime){
            currTime = std::chrono::system_clock::now().time_since_epoch();
            currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
            currTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(currTime).count();
        }

//        cout << "Submitting job ID: " << j.jobID  << " at " << (currTimeUs - startTimeUs) << " us" << endl;

        SubmitPacket *newSubmit = new SubmitPacket(j.numTasks);
        
        newSubmit->jobID = stoi(j.jobID);
        newSubmit->userID = DEFAULT_USER_ID;

        for(int i = 0; i < j.numTasks; i++){
        // for(int i = 0; i < 1; i++){
            newSubmit->tasks[i].taskID = i;
            newSubmit->tasks[i].functionID = FUNCTION_CODE_FLOAT;
            newSubmit->tasks[i].functionParams = j.runTime;        
        }

        uint32_t submitPacketSize = SUBMIT_PACKET_BASE_SIZE + ( newSubmit->numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE );
        // cout << "Submit Packet: " << newSubmit->toString();

        char *byteStream = newSubmit->convertToByteStream();

        entry = alloc_net_sge();
        assert(entry);
        entry->len += submitPacketSize;

        rte_memcpy(entry->payload , (void *)byteStream, submitPacketSize);

        /*SubmitPacket decodedPacket(j.numTasks);
        decodedPacket.populateFromByteStream(byteStream);
        cout << "Decoded Submit Packet: " << decodedPacket.toString() << endl;*/

        // cout << "Job Entry: " << j.toString() << endl;
        // cout << "Length of Stream: " << strlen(byteStream) << endl;
        /*cout << "ByteStream for packet: ";
        for(int i = 0; i < (SUBMIT_PACKET_BASE_SIZE + j.numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE); i++)
            cout << to_string(byteStream[i])  << " ";
        
        cout << endl;*/
    
    //  sendto(sockfd, byteStream, (SUBMIT_PACKET_BASE_SIZE + (SUBMIT_PACKET_TASK_HEADER_SIZE)), 0, (const struct sockaddr *) &servAddr, sizeof(servAddr));
        jobSubmissions += 1;
    	udp_send(entry, &id);

        if(taskSubmissionTimeMap.find(j.jobID) == taskSubmissionTimeMap.end()){
            taskSubmissionTimeMap.insert(pair<string, uint32_t >(j.jobID, currTimeUs));
        }

        // sendto(sockfd, byteStream, (SUBMIT_PACKET_BASE_SIZE + (j.numTasks * SUBMIT_PACKET_TASK_HEADER_SIZE)), 0, (const struct sockaddr *) &servAddr, sizeof(servAddr));
        delete byteStream;
        // break;
    }

    auto endTime = std::chrono::system_clock::now().time_since_epoch();
    auto endTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(endTime).count();
    auto endTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(endTime).count();


    std::cout << "Total job submissions: " << jobSubmissions << endl;
    std::cout << "Time for all submissions (ms) : " << endTimeMs - startTimeMs << endl;
    std::cout << "Time for all submissions (us) : " << endTimeUs - startTimeUs << endl;

    return 0;
}

void handleFinishedTask(char * finishedTaskStream, uint32_t recvTimeMicro, int mapID){
    
    /*cout << "ByteStream for packet: ";
    for(int i = 0; i < FINISHED_PACKET_SIZE; i++)
        cout << to_string(finishedTaskStream[i])  << " ";
        
    cout << endl;*/

    FinishedTaskPacket finPack;
    finPack.populateFromByteStream(finishedTaskStream);

    // cout << "Finished Packet String: " << finPack.toString() << endl;
    
    string jobIDString = to_string(finPack.jobID);

    taskCompletionDetails completionEntry;
    completionEntry.taskID = finPack.taskID;
    completionEntry.taskStatus = finPack.taskStatus;
    completionEntry.schedulerSubmitTime = finPack.time1;
    completionEntry.schedulerEndTime = finPack.time2;
    completionEntry.schedulerDispatchTime = finPack.schedulerDispatchTime;
    completionEntry.getTaskTimeMs = ((double)finPack.getTaskTimeUs)/1000.0;
    completionEntry.serviceTimeMs = ((double)finPack.serviceTimeUs)/1000.0;
    // completionEntry.clientSendTimeMs = ((double)taskSubmissionTimeMap[jobIDString])/1000.0;//CHECK THIS
    // completionEntry.clientRecieveTimeMs = ((double)recvTimeMicro)/1000.0;

    if(taskCompletionMapPerThread[mapID].find(jobIDString) == taskCompletionMapPerThread[mapID].end()){
        vector<taskCompletionDetails> taskCompleteVector;
        taskCompletionMapPerThread[mapID].insert(pair<string, vector<taskCompletionDetails>>(jobIDString, taskCompleteVector));
    }

    taskCompletionMapPerThread[mapID][jobIDString].push_back(completionEntry);

    finishedTasksPerThread[mapID] += 1;


    // pthread_mutex_lock(&finishedLock);
    // if(taskCompletionMap.find(jobIDString) == taskCompletionMap.end()){
    //     vector<taskCompletionDetails> taskCompleteVector;
    //     taskCompletionMap.insert(pair<string, vector<taskCompletionDetails>>(jobIDString, taskCompleteVector));
    // }

    // taskCompletionMap[jobIDString].push_back(completionEntry);
    // finishedTasks += 1;
    // pthread_mutex_unlock(&finishedLock);
        
}

void handleFailedTask(char *failedTaskStream, int mapID){

    SubmitPacket failedSubmit(1);
    failedSubmit.populateFromByteStream(failedTaskStream, false);

    tasksLostQueueFullPerThread[mapID] += failedSubmit.numTasks + 1;

    // pthread_mutex_lock(&finishedLock);
    // tasksLostQueueFull += failedSubmit.numTasks + 1;  
    // pthread_mutex_unlock(&finishedLock);
  
}

// void *handleTaskStreamsFromScheduler(void *funcArgs){
int handleTaskStreamsFromScheduler(void *funcArgs){

    handleMessageArgs *argStruct = (handleMessageArgs *)funcArgs;
    int threadID = argStruct->threadID;
    double runTimeSec = argStruct->runTimeSec;
    
    // char taskStream[FINISHED_PACKET_SIZE];
    char *taskStream;
    socklen_t len;
    int ret = 0, i = 0;
    struct rte_mbuf *rx_pkts[BATCH_SIZE];

    double runTimeMs = runTimeSec * 1000;
    finishedTasksPerThread[threadID - 1] = 0;
    tasksLostQueueFullPerThread[threadID - 1] = 0;

    RTE_PER_LCORE(queue_id) = threadID;

    struct net_sge *entry;

    auto startTime = std::chrono::system_clock::now().time_since_epoch();
    auto startTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(startTime).count();

    auto currTime = std::chrono::system_clock::now().time_since_epoch();
    auto currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
    auto currTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(currTime).count();

    struct timeval tv = {MAX_RECV_DELAY, 0};
    // setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, (struct timeval *)&tv, sizeof(struct timeval));

    while(currTimeMs - startTimeMs < runTimeMs){
        // memset(taskStream, 0, FINISHED_PACKET_SIZE);
    
        // int receivedLen = recv(sockfd, &taskStream, FINISHED_PACKET_SIZE , MSG_WAITALL);
        ret = rte_eth_rx_burst(0, RTE_PER_LCORE(queue_id), rx_pkts, BATCH_SIZE);
        if(ret == 0 ){
            currTime = std::chrono::system_clock::now().time_since_epoch();
            currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
            continue;
        }

        currTime = std::chrono::system_clock::now().time_since_epoch();
        currTimeUs = std::chrono::duration_cast<std::chrono::microseconds>(currTime).count();

        for (i = 0; i < ret; i++){ 
            struct ip_tuple recv_id ;
            entry = eth_in(rx_pkts[i], &recv_id);

            /*pthread_mutex_lock(&finishedLock);

            cout << "Thread " << threadID <<  " Recieved packet with port " << recv_id->dst_port << endl ;

            pthread_mutex_unlock(&finishedLock);

            */

            taskStream = (char *)entry->payload;

            if(taskStream[0] == P4_TASK_FINISH_CODE){
                handleFinishedTask(taskStream, currTimeUs, threadID - 1);
            }
            else if(taskStream[0] == P4_QUEUE_FULL_CODE){
                handleFailedTask(taskStream, threadID - 1);
            }   

            rte_pktmbuf_free((rte_mbuf*)entry->handle);
        }
        
        currTime = std::chrono::system_clock::now().time_since_epoch();
        currTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(currTime).count();
    
    }
    
    return 0;
}

void writeRunResultsToFiles(string runPrefix){

    
    ofstream schedDelayStream;
    schedDelayStream.open(runPrefix + "_SchedulingDelays.txt", ios::out);

    ofstream queueDelayStream;
    queueDelayStream.open(runPrefix + "_QueuingDelays.txt", ios::out);

    ofstream getTaskTimeStream;
    getTaskTimeStream.open(runPrefix + "_GetTaskTimes.txt", ios::out);

    ofstream serviceTimeStream;
    serviceTimeStream.open(runPrefix + "_ServiceTimes.txt", ios::out);

    ofstream taskCompletionTimeStream;
    taskCompletionTimeStream.open(runPrefix + "_TaskCompletionTimes.txt", ios::out);

    ofstream taskRunTimeStream;
    taskRunTimeStream.open(runPrefix + "_TaskRunTimes.txt", ios::out);
    
    // ofstream taskClientQueuingTimeStream;
    // taskClientQueuingTimeStream.open(runPrefix + "_ClientQueuingTimes.txt", ios::out);


    // ofstream taskMapStream;
    // taskMapStream.open(runPrefix + "_TaskRunDetails.json", ios::out);

    double switchTimerCycle = 0;

    // cout << "Job List: " << endl;
    // jobListOutput();
    

    for(auto it = taskCompletionMap.begin(); it != taskCompletionMap.end(); it++){

        // cout << "It First: " << it->first << endl;
        // cout << "It Second Size: " << it->second.size() << endl;

        double taskRunTime = -1;
        for(jobEntry j: *jobList){
            if(j.jobID == it->first){
                // For ms tasks
                // taskRunTime = j.runTime;
                // For microsec tasks
                taskRunTime = j.runTime/1000;
            }
        }

        if(taskRunTime == -1){
            cout << "Job Submission not found for job ID: " << it->first << endl;
            continue;
        }

        for(auto taskEntry: it->second){
            double totalTaskTimeMs = ((double)taskEntry.schedulerEndTime - (double)taskEntry.schedulerSubmitTime)/(1000.0 * 1000.0);
            if(totalTaskTimeMs < 0)
                totalTaskTimeMs = (SWITCH_TIME_OVERFLOW_CONSTANT + (double)taskEntry.schedulerEndTime - (double)taskEntry.schedulerSubmitTime)/(1000.0 * 1000.0);

            double queuingDelayMs = ((double)taskEntry.schedulerDispatchTime - (double)taskEntry.schedulerSubmitTime)/(1000.0 * 1000.0);
            if(queuingDelayMs < 0)
                queuingDelayMs = (SWITCH_TIME_OVERFLOW_CONSTANT + (double)taskEntry.schedulerDispatchTime - (double)taskEntry.schedulerSubmitTime)/(1000.0 * 1000.0);
                

            double schedDelayMs = totalTaskTimeMs - taskRunTime;
            if(schedDelayMs < 0){
                negativeSchedTimeTasks += 1;
                continue;
            }

            // double clientQueingDelayMs = taskEntry.clientRecieveTimeMs - taskEntry.clientSendTimeMs;

            // cout << "Queuing Delay: " << queuingDelayMs << endl;

            // taskClientQueuingTimeStream << to_string(clientQueingDelayMs) << endl;
            queueDelayStream << to_string(queuingDelayMs) << endl;
            taskCompletionTimeStream << to_string(totalTaskTimeMs) << endl;
            schedDelayStream << to_string(schedDelayMs) << endl;
            getTaskTimeStream << to_string(taskEntry.getTaskTimeMs) << endl;
            serviceTimeStream << to_string(taskEntry.serviceTimeMs) << endl;
            taskRunTimeStream << to_string(taskRunTime) << endl;

        }

    }

    // taskMapStream << taskCompletionMapOutput() << endl;

    schedDelayStream.close();
    queueDelayStream.close();
    getTaskTimeStream.close();
    serviceTimeStream.close();
    taskCompletionTimeStream.close();
    taskRunTimeStream.close();
    // taskMapStream.close();
    // taskClientQueuingTimeStream.close();

}

void combineThreadResults(){

    for (int i = 0; i < RECV_THREADS_NUM; i++)
    {
        for(auto it = taskCompletionMapPerThread[i].begin(); it != taskCompletionMapPerThread[i].end(); it++){


            if(taskCompletionMap.find(it->first) == taskCompletionMap.end()){
                vector<taskCompletionDetails> taskCompleteVector;
                taskCompletionMap.insert(pair<string, vector<taskCompletionDetails>>(it->first, taskCompleteVector));
                
            }

            taskCompletionMap[it->first].insert(taskCompletionMap[it->first].end(), (it->second).begin(), (it->second).end());

            // for(auto it2 = it->second.begin(); it2 != it->second.end(); it2++){
            //     taskCompletionDetails completionEntry;
            //     completionEntry.taskID = it2->taskID;
            //     completionEntry.taskStatus = it2->taskStatus;
            //     completionEntry.schedulerSubmitTime = it2-> ;
            //     completionEntry.schedulerEndTime = it2->schedulerEndTime;
            //     completionEntry.schedulerDispatchTime = it2->schedulerDispatchTime;
            //     completionEntry.getTaskTimeMs = it2->getTaskTimeMs;
            //     completionEntry.serviceTimeMs = it2->serviceTimeMs;

            //     taskCompletionMap[it->first].push_back(completionEntry);
            // }

        }

       // cout << " Thread : " << i << "Finished : " << finishedTasksPerThread[i] << " Task"<< endl;
        finishedTasks += finishedTasksPerThread[i];
        tasksLostQueueFull += tasksLostQueueFullPerThread [i];  
        
    }

}

int main(int argc, char* argv[]){
    if(argc < 4){
        cout << "Usage: ./Falcon_Client <jobInputFilePath> <postLaunchWaitTime_sec> <runResultPrefix>" << endl;
        return 0;
    }

    string inFilePath = argv[1];
    readInputFile(inFilePath);

    double runTimeSec = stof(argv[2]);
    string runPrefix = argv[3];


    struct sockaddr_in servAddr = setupPseudoFalconServAddr();
    // int sockfd = createThreadSocket();

    dpdk_init(&argc,&argv);

	/* RTE flow creation */
	uint8_t port_id = 0;

	for (int i = 0; i < rte_lcore_count(); i++) {
		// cout << "generating flow : portID " << port_id << " RX queue id  " << i << " udp port linked " << i * 1000 + 10000  + i << endl;
		generate_udp_flow(port_id, i , i * 1000 + 10000  + i, 0xffff);
	}

    rte_flow_isolate(0, 1, NULL);

    submitJobArgs submitStruct[RECV_THREADS_NUM];
    // submitStruct.sockfd = sockfd;
    // submitStruct.servAddr = servAddr;

    handleMessageArgs handleStruct[RECV_THREADS_NUM];
    // handleStruct.sockfd = sockfd;

    // pthread_t threads[2];
    // int rc;
    // void *threadStatus;
    int i = 0, j = 0;
    unsigned lcore_id;

    RTE_LCORE_FOREACH_SLAVE(lcore_id)
	{
        if (i == 0){
            rte_eal_remote_launch(submitJobs, (void *)&submitStruct, lcore_id);
            i++;
        }else if (i <= RECV_THREADS_NUM){
            handleStruct[j].threadID = i;
            handleStruct[j].runTimeSec = runTimeSec;
            rte_eal_remote_launch(handleTaskStreamsFromScheduler, (void *)&handleStruct[j], lcore_id);
            i++;
            j++;
        } else {
            break;
        }
        
    }


    RTE_LCORE_FOREACH_SLAVE(lcore_id)
    {
        if (rte_eal_wait_lcore(lcore_id) < 0) {
            break;
        }
    }


    /*submitJobs((void *)&submitStruct);
    handleTaskStreamsFromScheduler((void *)&handleStruct);*/

    // taskCompletionMapOutput();

    combineThreadResults();

    cout << "--------------------- Workload Stats ------------------------" << endl;
    cout << "Total Tasks in File: " << totalTasksToLaunch << endl;
    cout << "Tasks Completed by Workers: " << finishedTasks << endl;
    cout << "Tasks lost due to scheduler queues being full: " << tasksLostQueueFull << endl;
    cout << "Incomplete Tasks / Tasks Lost due to unknown issues: " << totalTasksToLaunch - tasksLostQueueFull - finishedTasks << endl;
    cout << "Tasks with negative scheduling time (indicates switch timer overflow): " << negativeSchedTimeTasks << endl;

    writeRunResultsToFiles(runPrefix);

    // close(sockfd);    
    
    return 0;
}

