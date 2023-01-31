#include <iostream>
#include <string>

#include <cstdlib>
#include <cstdio>
#include <cstring>
#include <cstdint>

#include <vector>

#include <sys/types.h> 
#include <sys/socket.h> 
#include <arpa/inet.h> 
#include <netinet/in.h>

#define P4_RETRIEVE_TASK_CODE 1
#define P4_SUBMIT_TASK_CODE 2
#define P4_TASK_FINISH_CODE 5
#define P4_QUEUE_FULL_CODE -1

#define RETRIEVE_TASK_PACKET_SIZE 28
#define FINISHED_PACKET_SIZE 35
#define SUBMIT_PACKET_BASE_SIZE 12
#define SUBMIT_PACKET_TASK_HEADER_SIZE 8

#define FUNCTION_CODE_FLOAT 1
#define FUNCTION_CODE_DROP 8
#define FUNCTION_CODE_SLEEP 9

#define INVALID_TASK_ID 65535

#define TASK_STATUS_FAIL 0
#define TASK_STATUS_FINISH 1

#define PSEUDO_SCHEDULER "192.168.126.55"
// #define PSEUDO_SCHEDULER "192.168.26.101"
#define SCHEDULER_PORT 5555

#define SWITCH_TIME_BITMASK 0x00000000ffffffff
#define SWITCH_TIME_OVERFLOW_CONSTANT 4294967296

#define FALCON_SW_SERV_MAX_TASKS_PER_SUBMIT 1200

// ------------------------ Packet Structure Definitions -----------------------------------------

// Structures for job and task submission packets
struct __attribute__((packed)) SubmitTaskHeader {
    uint16_t functionID;
    uint32_t functionParams;
    uint16_t taskID;

    SubmitTaskHeader(){
        functionID = 0;
        functionParams = 0;
        taskID = 0;
    }

    std::string toString(){
        std::string returnString = "";
        returnString+= std::string("TaskID: ").append(std::to_string(taskID)).append("\n");
        returnString+= std::string("FuncID: ").append(std::to_string(functionID)).append("\n");
        returnString+= std::string("FuncParams: ").append(std::to_string(functionParams)).append("\n");

        return returnString;
    }
};

struct __attribute__((packed)) SubmitPacket {
    uint8_t p4OperationCode;
    uint8_t userID;
    uint32_t jobID;
    uint16_t numTasks;
    uint32_t fixArrayIndex;
    SubmitTaskHeader *tasks;

    SubmitPacket(int _numTasks){
        p4OperationCode = P4_SUBMIT_TASK_CODE;
        userID = 0;
        jobID = 0;
        numTasks = _numTasks;
        fixArrayIndex = 0;
        
        tasks = new SubmitTaskHeader[numTasks]();

    }

    std::string toString(){
        std::string returnString = "";
        returnString += std::string("P4OpCode: ").append(std::to_string(p4OperationCode)).append("\n");
        returnString += std::string("UserID: ").append(std::to_string(userID)).append("\n");
        returnString += std::string("JobID: ").append(std::to_string(jobID)).append("\n");
        returnString += std::string("NumTasks: ").append(std::to_string(numTasks)).append("\n");
        returnString += std::string("FixArray: ").append(std::to_string(fixArrayIndex)).append("\n");
        
        for(int i = 0; i < numTasks; i++)
            returnString += std::string("Task:").append(tasks[i].toString());
        
        return returnString;
        }
    

   char * convertToByteStream(){

       char *byteStream = new char[SUBMIT_PACKET_BASE_SIZE + (SUBMIT_PACKET_TASK_HEADER_SIZE * numTasks)];
       char *byteStreamTemp = byteStream;

       // memcpy(byteStream, packet, sizeof(*packet));
       memcpy(byteStreamTemp, (char *)&p4OperationCode, sizeof(p4OperationCode));
       byteStreamTemp += sizeof(p4OperationCode);
       
       memcpy(byteStreamTemp, (char *)&userID, sizeof(userID));
       byteStreamTemp += sizeof(userID);

       // uint32_t cvt_jobID = htonl(jobID);
       uint32_t cvt_jobID = htonl(jobID);
       memcpy(byteStreamTemp, (char *)&cvt_jobID, sizeof(cvt_jobID));
       byteStreamTemp += sizeof(cvt_jobID);

       uint16_t cvt_numTasks = htons(numTasks);
       // uint16_t cvt_numTasks = numTasks;
       memcpy(byteStreamTemp, (char *)&cvt_numTasks, sizeof(cvt_numTasks));
       byteStreamTemp += sizeof(cvt_numTasks);

       uint32_t cvt_fixArrayIndex = htonl(fixArrayIndex);
       // uint32_t cvt_fixArrayIndex = fixArrayIndex;
       memcpy(byteStreamTemp, (char *)&cvt_fixArrayIndex, sizeof(cvt_fixArrayIndex));
       byteStreamTemp += sizeof(cvt_fixArrayIndex);

       for(int i = 0; i < numTasks; i++){
            uint16_t functionID = htons(tasks[i].functionID);
            uint32_t functionParams = htonl(tasks[i].functionParams);
            uint16_t taskID = htons(tasks[i].taskID);
            /*uint32_t functionID = tasks[i].functionID;
            uint32_t functionParams = tasks[i].functionParams;
            uint16_t taskID = tasks[i].taskID;*/

            memcpy(byteStreamTemp, (char *)&functionID, sizeof(functionID));
            byteStreamTemp += sizeof(functionID);

            memcpy(byteStreamTemp, (char *)&functionParams, sizeof(functionParams));
            byteStreamTemp += sizeof(functionParams);

            memcpy(byteStreamTemp, (char *)&taskID, sizeof(taskID));
            byteStreamTemp += sizeof(taskID);

       }

       /*std::cout << "Stream at End: ";
       for(int i = 0; i < SUBMIT_PACKET_BASE_SIZE + (SUBMIT_PACKET_TASK_HEADER_SIZE * numTasks); i++)
           std::cout << std::to_string(byteStream[i]) << " ";

       std::cout << std::endl;*/

       // byteStreamTemp[0] = '\0';


        // std::cout << "Sizeof FixArray " << sizeof(cvt_fixArrayIndex) << std::endl;

       return byteStream;
    }

    void populateFromByteStream(char *byteStream, bool populateTasks=true){

        /*std::cout << "Stream : ";
        for(int i = 0; i < SUBMIT_PACKET_BASE_SIZE; i++)
           std::cout << std::to_string(byteStream[i]) << " ";

        std::cout << std::endl;*/

        char *byteStreamTemp = byteStream;
       
        memcpy(&p4OperationCode, byteStreamTemp, sizeof(p4OperationCode));
        byteStreamTemp += sizeof(p4OperationCode);
        
        memcpy(&userID, byteStreamTemp,  sizeof(userID));
        byteStreamTemp += sizeof(userID);
     
        memcpy(&jobID, byteStreamTemp, sizeof(jobID));
        jobID = ntohl(jobID);
        byteStreamTemp += sizeof(jobID);

        memcpy(&numTasks, byteStreamTemp, sizeof(numTasks));
        numTasks = ntohs(numTasks);
        byteStreamTemp += sizeof(numTasks);

        /*memcpy(&fixArrayIndex, byteStreamTemp, sizeof(fixArrayIndex));
        fixArrayIndex = ntohl(fixArrayIndex);
        byteStreamTemp += sizeof(fixArrayIndex);

        memcpy(&fixPriority, byteStreamTemp, sizeof(fixPriority));
        byteStreamTemp += sizeof(fixPriority);*/

        //Add this functionality later if required
        
        fixArrayIndex = 0;
        byteStreamTemp += sizeof(fixArrayIndex);
     
        if(populateTasks == false)
            return;
        
        tasks = new SubmitTaskHeader[numTasks]();

        for(uint16_t i=0; i < numTasks; i++){
            memcpy((char *)&(tasks[i].functionID), byteStreamTemp, sizeof(tasks[i].functionID));
            byteStreamTemp += sizeof(tasks[i].functionID);

            memcpy((char *)&(tasks[i].functionParams), byteStreamTemp, sizeof(tasks[i].functionParams));
            byteStreamTemp += sizeof(tasks[i].functionParams);

            memcpy((char *)&(tasks[i].taskID), byteStreamTemp, sizeof(tasks[i].taskID));
            byteStreamTemp += sizeof(tasks[i].taskID);
            
            tasks[i].functionID = ntohs(tasks[i].functionID);
            tasks[i].functionParams = ntohl(tasks[i].functionParams);
            tasks[i].taskID = ntohs(tasks[i].taskID);
        }
    }

};

// Structure to represent retrieve task packets
struct __attribute__((packed)) RetrieveTaskPacket {
    uint8_t p4OperationCode;
    uint8_t userID;
    uint32_t jobID;
    uint16_t taskID;
    uint16_t functionID;
    uint32_t functionParameters;
    uint32_t userIP;
    uint16_t userPort;
    uint32_t time1;
    uint32_t time2;

    std::string toString(){
        std::string returnString = "";
        returnString += std::string("P4OpCode: ").append(std::to_string(p4OperationCode)).append("\n");
        returnString += std::string("UserID: ").append(std::to_string(userID)).append("\n");
        returnString += std::string("JobID: ").append(std::to_string(jobID)).append("\n");
        returnString += std::string("TaskID: ").append(std::to_string(taskID)).append("\n");
        returnString += std::string("FunctionID: ").append(std::to_string(functionID)).append("\n");
        returnString += std::string("FunctionParams: ").append(std::to_string(functionParameters)).append("\n");
        returnString += std::string("UserIP: ").append(std::to_string(userIP)).append("\n");
        returnString += std::string("UserPort: ").append(std::to_string(userPort)).append("\n");
        returnString += std::string("Time1: ").append(std::to_string(time1)).append("\n");
        returnString += std::string("Time2: ").append(std::to_string(time2)).append("\n");
        return returnString;
    } 

   char * convertToByteStream(){

        char *byteStream = new char[RETRIEVE_TASK_PACKET_SIZE];
        
        char *byteStreamTemp = byteStream;
        
        memcpy(byteStreamTemp, (char *)&p4OperationCode, sizeof(p4OperationCode));
        byteStreamTemp += sizeof(p4OperationCode);
        
        memcpy(byteStreamTemp, (char *)&userID, sizeof(userID));
        byteStreamTemp += sizeof(userID);

        uint32_t cvt_jobID = htonl(jobID);
        memcpy(byteStreamTemp, (char *)&cvt_jobID, sizeof(cvt_jobID));
        byteStreamTemp += sizeof(cvt_jobID);

        uint16_t cvt_taskID = htons(taskID);
        memcpy(byteStreamTemp, (char *)&cvt_taskID, sizeof(cvt_taskID));
        byteStreamTemp += sizeof(cvt_taskID);

        uint16_t cvt_functionID = htons(functionID);    
        memcpy(byteStreamTemp, (char *)&(cvt_functionID), sizeof(cvt_functionID));
        byteStreamTemp += sizeof(cvt_functionID);

        uint32_t cvt_functionParameters = htonl(functionParameters);
        memcpy(byteStreamTemp, (char *)&(cvt_functionParameters), sizeof(cvt_functionParameters));
        byteStreamTemp += sizeof(cvt_functionParameters);

        uint32_t cvt_userIP = htonl(userIP);
        memcpy(byteStreamTemp, (char *)&cvt_userIP, sizeof(cvt_userIP));
        byteStreamTemp += sizeof(cvt_userIP);

        uint16_t cvt_userPort = htons(userPort);
        memcpy(byteStreamTemp, (char *)&cvt_userPort, sizeof(cvt_userPort));
        byteStreamTemp += sizeof(cvt_userPort);

        uint32_t cvt_time1 = htonl(time1);
        memcpy(byteStreamTemp, (char *)&cvt_time1, sizeof(cvt_time1));
        byteStreamTemp += sizeof(cvt_time1);
        
        uint32_t cvt_time2 = htonl(time2);
        memcpy(byteStreamTemp, (char *)&cvt_time2, sizeof(cvt_time2));
        byteStreamTemp += sizeof(cvt_time2);

        return byteStream;
    }
   
   void populateFromByteStream(char * byteStream){
       memcpy(this, byteStream, RETRIEVE_TASK_PACKET_SIZE);     
   }
   
   RetrieveTaskPacket(){
       p4OperationCode = P4_RETRIEVE_TASK_CODE;
       userID = -1;
       jobID = -1;
       taskID = -1;
       functionID = -1;
       functionParameters = -1;
       userIP = -1;
       userPort = -1;
       time1 = -1;
       time2 = -1;
   } 
};

RetrieveTaskPacket *createRetrieveTaskPacket(){
    RetrieveTaskPacket *newTask = new RetrieveTaskPacket();
    return newTask;
}

struct __attribute__((packed)) FinishedTaskPacket {
    uint8_t p4OperationCode;
    uint8_t userID;
    uint32_t jobID;
    uint16_t taskID;
    uint8_t taskStatus;
    uint32_t userIP;
    uint16_t userPort;
    uint32_t time1;
    uint32_t time2;
    uint32_t getTaskTimeUs;
    uint32_t serviceTimeUs;
    uint32_t schedulerDispatchTime;

    FinishedTaskPacket(){
        p4OperationCode = P4_TASK_FINISH_CODE;
        userID = 0;
        jobID = 0;
        taskID = 0;
        taskStatus = TASK_STATUS_FINISH;
        userIP = 0;
        userPort = 0;
        time1 = 0;
        time2 = 0;
        getTaskTimeUs = 0;
        serviceTimeUs = 0;
        schedulerDispatchTime = 0;
    }

    char * convertToByteStream(){
        char *byteStream = new char[FINISHED_PACKET_SIZE];
        char *byteStreamTemp = byteStream;
       
        memcpy(byteStreamTemp, (char *)&p4OperationCode, sizeof(p4OperationCode));
        byteStreamTemp += sizeof(p4OperationCode);
        
        memcpy(byteStreamTemp, (char *)&userID, sizeof(userID));
        byteStreamTemp += sizeof(userID);

        uint32_t cvt_jobID = htonl(jobID);
        memcpy(byteStreamTemp, (char *)&cvt_jobID, sizeof(cvt_jobID));
        byteStreamTemp += sizeof(cvt_jobID);

        uint16_t cvt_taskID = htons(taskID);
        memcpy(byteStreamTemp, (char *)&cvt_taskID, sizeof(cvt_taskID));
        byteStreamTemp += sizeof(cvt_taskID);

        memcpy(byteStreamTemp, (char *)&taskStatus, sizeof(taskStatus));
        byteStreamTemp += sizeof(taskStatus);

        uint32_t cvt_userIP = htonl(userIP);
        memcpy(byteStreamTemp, (char *)&cvt_userIP, sizeof(cvt_userIP));
        byteStreamTemp += sizeof(cvt_userIP);

        uint16_t cvt_userPort = htons(userPort);
        memcpy(byteStreamTemp, (char *)&cvt_userPort, sizeof(cvt_userPort));
        byteStreamTemp += sizeof(cvt_userPort);

        uint32_t cvt_time1 = htonl(time1);
        memcpy(byteStreamTemp, (char *)&cvt_time1, sizeof(cvt_time1));
        byteStreamTemp += sizeof(cvt_time1);
        
        uint32_t cvt_time2 = htonl(time2);
        memcpy(byteStreamTemp, (char *)&cvt_time2, sizeof(cvt_time2));
        byteStreamTemp += sizeof(cvt_time2);

        uint32_t cvt_getTaskTime = htonl(getTaskTimeUs);
        memcpy(byteStreamTemp, (char *)&cvt_getTaskTime, sizeof(cvt_getTaskTime));
        byteStreamTemp += sizeof(cvt_getTaskTime);

        uint32_t cvt_serviceTime = htonl(serviceTimeUs);
        memcpy(byteStreamTemp, (char *)&cvt_serviceTime, sizeof(cvt_serviceTime));
        byteStreamTemp += sizeof(cvt_serviceTime);

        uint32_t cvt_schedulerDispatchTime = htonl(schedulerDispatchTime);
        memcpy(byteStreamTemp, (char *)&cvt_schedulerDispatchTime, sizeof(cvt_schedulerDispatchTime));
        byteStreamTemp += sizeof(cvt_schedulerDispatchTime);

        return byteStream;

    }

    void populateFromByteStream(char *byteStream){
        char *byteStreamTemp = byteStream;
       
        memcpy(&p4OperationCode, byteStreamTemp, sizeof(p4OperationCode));
        byteStreamTemp += sizeof(p4OperationCode);

        memcpy(&userID, byteStreamTemp, sizeof(userID));
        byteStreamTemp += sizeof(userID);
        
        memcpy(&jobID, byteStreamTemp, sizeof(jobID));
        jobID = ntohl(jobID);
        byteStreamTemp += sizeof(jobID);

        memcpy(&taskID, byteStreamTemp, sizeof(taskID));
        taskID = ntohs(taskID);
        byteStreamTemp += sizeof(taskID);

        memcpy(&taskStatus, byteStreamTemp, sizeof(taskStatus));
        byteStreamTemp += sizeof(taskStatus);

        memcpy(&userIP, byteStreamTemp, sizeof(userIP));
        userIP = ntohl(userIP);
        byteStreamTemp += sizeof(userIP);

        memcpy(&userPort, byteStreamTemp, sizeof(userPort));
        userPort = ntohs(userPort);
        byteStreamTemp += sizeof(userPort);

        memcpy(&time1, byteStreamTemp, sizeof(time1));
        byteStreamTemp += sizeof(time1);
        time1 = ntohl(time1);
        time1 = time1 & SWITCH_TIME_BITMASK;
        
        
        memcpy(&time2, byteStreamTemp, sizeof(time2));
        byteStreamTemp += sizeof(time2);
        time2 = ntohl(time2);
        time2 = time2 & SWITCH_TIME_BITMASK;
        

        memcpy(&getTaskTimeUs, byteStreamTemp, sizeof(getTaskTimeUs));
        getTaskTimeUs = ntohl(getTaskTimeUs);
        byteStreamTemp += sizeof(getTaskTimeUs);

        memcpy(&serviceTimeUs, byteStreamTemp, sizeof(serviceTimeUs));
        serviceTimeUs = ntohl(serviceTimeUs);
        byteStreamTemp += sizeof(serviceTimeUs);

        memcpy(&schedulerDispatchTime, byteStreamTemp, sizeof(schedulerDispatchTime));
        byteStreamTemp += sizeof(schedulerDispatchTime);
        schedulerDispatchTime = ntohl(schedulerDispatchTime);
        schedulerDispatchTime = schedulerDispatchTime & SWITCH_TIME_BITMASK;

    }

    std::string toString(){
        std::string returnString = "";
        returnString += std::string("P4OpCode: ").append(std::to_string(p4OperationCode)).append("\n");
        returnString += std::string("UserID: ").append(std::to_string(userID)).append("\n");
        returnString += std::string("JobID: ").append(std::to_string(jobID)).append("\n");
        returnString += std::string("TaskID: ").append(std::to_string(taskID)).append("\n");
        returnString += std::string("TaskStatus: ").append(std::to_string(taskStatus)).append("\n");
        returnString += std::string("UserIP: ").append(std::to_string(userIP)).append("\n");
        returnString += std::string("UserPort: ").append(std::to_string(userPort)).append("\n");
        returnString += std::string("Time1: ").append(std::to_string(time1)).append("\n");
        returnString += std::string("Time2: ").append(std::to_string(time2)).append("\n");
        returnString += std::string("GetTaskTimeUs: ").append(std::to_string(getTaskTimeUs)).append("\n");
        returnString += std::string("ServiceTimeUs: ").append(std::to_string(serviceTimeUs)).append("\n");
        returnString += std::string("SchedDispatchTime: ").append(std::to_string(schedulerDispatchTime)).append("\n");
                
        return returnString;
    }

};

// ------------------------------------------------ Network Helper Functions -------------------------------------------------
// Function to set up a socket address for the pseudo Falcon scheduler server
sockaddr_in setupPseudoFalconServAddr(){

    struct sockaddr_in servaddr;
    memset(&servaddr, 0, sizeof(servaddr)); 
    
    servaddr.sin_family = AF_INET; 
    servaddr.sin_port = htons(SCHEDULER_PORT); 
    servaddr.sin_addr.s_addr = inet_addr(PSEUDO_SCHEDULER);

    return servaddr;
}

// Function to create a new socket
int createThreadSocket() {
    int sockfd; 
   
    // Creating socket file descriptor 
    if ( (sockfd = socket(AF_INET, SOCK_DGRAM, 0)) < 0 ) { 
        perror("Socket creation failed"); 
        exit(EXIT_FAILURE); 
    } 
  
   return sockfd;         
}

uint16_t getNumTasksFromSubmitByteStream(char *byteStream){

        SubmitPacket submitPacket(0);

        char *byteStreamTemp = byteStream;
        byteStreamTemp += sizeof(submitPacket.p4OperationCode);
        byteStreamTemp += sizeof(submitPacket.userID);
        byteStreamTemp += sizeof(submitPacket.jobID);

        uint16_t numTasks;
        memcpy(&numTasks, byteStreamTemp, sizeof(numTasks));
        numTasks = ntohs(numTasks);
        
        return numTasks;    
    }

// ------------------------------- Misc Helper Functions ------------------------------------------------
// Helper function to split strings based on delimiter
std::vector<std::string> stringSplit(const std::string& str, const std::string& delim)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}
