#include "Node.h"

#include<stdio.h>
#include<string.h>	//strlen
#include<stdlib.h>	//strlen
#include<sys/socket.h>
#include<arpa/inet.h>	//inet_addr
#include<unistd.h>	//write
#include<pthread.h> //for threading , link with lpthread
#include<stdint.h>
#include<netinet/in.h>

#define MaxListener 10
#define MaxLenOfEntry 128
#define MaxNumOfRes 128
#define MaxNumOfReqForQuery 128
#define MaxNumOfResList 64

int lengthOfCommand = 128;
int currentConnectedNodes = 0;
int currentConnectedNodesForOutgoing = 0;
int messageSequence = 0; //start from 0, add one when send;
int currentHoldResNum = 0;
struct sockaddr_in server;

struct resourceContent{
    uint16_t resource_id;
    uint16_t SBZ;
    uint32_t resource_value;
};

struct QueryHitMsg{
    struct P2P_h header;
    uint16_t entry_size;
    uint16_t SBZ;
    struct resourceContent resourceList[64];
};

struct ResourceEntry{
    int key;
    char entry[128];
};

struct ResourceEntry resourceEntryTable[MaxNumOfRes];


struct VisitedResourceEntry{
    int key;
    int client_port;
    int valid;
};
struct VisitedResourceEntry visitedResourceEntry[MaxNumOfReqForQuery];

struct Node{
    unsigned short   sin_port;     // e.g. htons(3490)
    struct in_addr   sin_addr;
    int port;
    int valid;
    int * socket;
};

struct JoinResponseMsg{
    struct P2P_h header;
    struct P2P_join joinBody;
};


struct PingBMsg{
    struct P2P_h header;
    struct P2P_pong_front pong_front;
    struct P2P_pong_entry pong_entry_array[MAX_PEER_AD];
};


struct QueryMsg{
    struct P2P_h header;
    int message_key;
};


struct Node nodeTable[MaxListener];
struct Node nodeTableForOutgoing[MaxListener];


uint32_t hashMessageID(uint32_t address, uint16_t port, int sequence){
    messageSequence++;
    return (uint32_t)((address*port+sequence)%1048576);
}


void iniResTable(){
    resourceEntryTable[0].key = 1;
    strcpy(resourceEntryTable[0].entry, "/home/user/Desktop/fileOne.txt");
    
    
    resourceEntryTable[1].key = 3;
    strcpy(resourceEntryTable[1].entry, "/home/user/Desktop/fileTwo.txt");
    
    currentHoldResNum = 2;
    
    for(int i=0; i<MaxNumOfReqForQuery; i++){
        visitedResourceEntry[i].key = -1;
        visitedResourceEntry[i].client_port = -1;
        visitedResourceEntry[i].valid = 0;
    }
}



void iniNodeTable(){
    for(int i=0; i<MaxListener; i++){
        nodeTable[i].sin_port = 0;
        nodeTable[i].sin_addr.s_addr = 0;
        nodeTable[i].port = 0;
        nodeTable[i].valid=0;
        nodeTable[i].socket = (int* )malloc(1);
        
        nodeTableForOutgoing[i].sin_port = 0;
        nodeTableForOutgoing[i].sin_addr.s_addr = 0;
        nodeTableForOutgoing[i].port = 0;
        nodeTableForOutgoing[i].valid=0;
        nodeTableForOutgoing[i].socket = (int* )malloc(1);
    }
}




//heatbeat function: from client to server;
void *heart_beat(void *mes){
    struct P2P_h heatbeatMsg;
    while(1){
        for(int i=0; i<MaxListener; i++){
            
            if(nodeTableForOutgoing[i].valid == 1){
            
                //get its ip and port
                struct sockaddr_in local_address;
                socklen_t addr_size = sizeof(local_address);
                getsockname(*nodeTableForOutgoing[i].socket, (struct sockaddr *)&local_address, &addr_size);
        
        
                //create heart beat msg;
                heatbeatMsg.version = P_VERSION;
                heatbeatMsg.ttl = 1;
                heatbeatMsg.msg_type = MSG_PING;
                heatbeatMsg.org_port = (uint16_t)local_address.sin_port;
                heatbeatMsg.org_ip = local_address.sin_addr.s_addr;
                heatbeatMsg.length = 0;
                heatbeatMsg.msg_id = hashMessageID(heatbeatMsg.org_ip, heatbeatMsg.org_port, messageSequence);
                if(send(*nodeTableForOutgoing[i].socket, (char *)&heatbeatMsg, HLEN + 1, 0) < 0){
                    printf("heart_beat@failed to send!\n");
                }
                
            }
        }
        sleep(10);
    }
}





/***************************** communicate with other Node ****************************/
void *connection_handler(void *socket_desc)
{
    //Get the socket descriptor
    int sock = *(int*)socket_desc;
    int read_size;
    char client_message[128];
    
    while( (read_size = recv(sock , client_message , sizeof(client_message), 0)) > 0 )
    {
//        test code: print msg content;
//        printf("connection_handler@msg:");
//        for(int i=0; i<sizeof(client_message); i++){
//            printf("%d ", (uint8_t)client_message[i]);
//        }
//        printf("\n");
        
        
        //parse the message;
        struct P2P_h message;
        message.version = (uint8_t)client_message[0];
        message.ttl = (uint8_t)client_message[1];
        
        
//        printf("connection_handler@version:%d ttl:%d\n", message.version, message.ttl);
        
        if(message.version != P_VERSION){
//            printf("connection_handler@message version not correct!\n");
            continue;
        }else if(message.ttl < 0 || message.ttl > 5){
//            printf("connection_handler@message ttl not valid!\n");
            continue;
        }
        message.ttl--;
        message.msg_type = (uint8_t)client_message[2];
        if(message.msg_type == MSG_JOIN){
            printf("connection_handler@join msg!\n");
            //get its ip and port
            struct sockaddr_in local_address;
            socklen_t addr_size = sizeof(local_address);
            getsockname(sock, (struct sockaddr *)&local_address, &addr_size);
            
            //construct response msg;
            struct JoinResponseMsg responseToJoin;
            responseToJoin.header.version = (uint8_t)client_message[0];
            responseToJoin.header.ttl  = 1;
            responseToJoin.header.msg_type = (uint8_t)client_message[2];
            responseToJoin.header.org_port = (uint16_t)local_address.sin_port;
            responseToJoin.header.length = 2;
            responseToJoin.header.org_ip = local_address.sin_addr.s_addr;
            responseToJoin.header.msg_id = (uint32_t)client_message[12]*256*256*256 + (uint32_t)client_message[13]*256*256 + (uint32_t)client_message[14]*256 + (uint32_t)client_message[15];
            responseToJoin.joinBody.status = JOIN_ACC;
            if(send(sock, (char *)&responseToJoin, sizeof(struct JoinResponseMsg)+1, 0) < 0){
                printf("connection_handler@failed to send.\n");
            }
            printf("connection_handler@join response!\n");
            
        }else if(message.msg_type == MSG_PING){
            if(message.ttl == 0){
                printf("connection_handler@heart bit ping msg or pingA msg.\n");
                
                //should response pong;
                //find the ip that this sock connected to.
                int _tempCountForFindIP = 0;
                struct in_addr connectedIP;
                while(_tempCountForFindIP < MaxListener){
                    if(nodeTable[_tempCountForFindIP].valid == 1 && *nodeTable[_tempCountForFindIP].socket == sock){
                        connectedIP.s_addr = nodeTable[_tempCountForFindIP].sin_addr.s_addr;
                        break;
                    }
                    _tempCountForFindIP++;
                }
                
                //get its ip and port
                struct sockaddr_in local_address;
                socklen_t addr_size = sizeof(local_address);
                getsockname(sock, (struct sockaddr *)&local_address, &addr_size);
                
                /*construct pingA response msg*/
                //pingB header
                struct P2P_h pongAMessage;
                pongAMessage.version = P_VERSION;
                pongAMessage.ttl = 1;
                pongAMessage.msg_type = MSG_PONG;
                pongAMessage.org_port = (uint16_t)local_address.sin_port;
                pongAMessage.org_ip = local_address.sin_addr.s_addr;
                pongAMessage.msg_id = (uint32_t)client_message[12]*256*256*256 + (uint32_t)client_message[13]*256*256 + (uint32_t)client_message[14]*256 + (uint32_t)client_message[15];
                if(send(*nodeTableForOutgoing[_tempCountForFindIP].socket, (char *)&pongAMessage, HLEN+1, 0) < 0){
                    printf("pongA@send pong failed. receiver may not alive!\n");
                    continue;
                }
                
                
                
                
            }else if(message.ttl >= 1){
                printf("connection_handler@pingB msg.\n");
                //get its ip and port
                struct sockaddr_in local_address;
                socklen_t addr_size = sizeof(local_address);
                getsockname(sock, (struct sockaddr *)&local_address, &addr_size);
                
                /*construct pingB response msg*/
                //pingB header
                struct PingBMsg pingBMessage;
                pingBMessage.header.version = P_VERSION;
                pingBMessage.header.ttl = message.ttl;
                pingBMessage.header.msg_type = MSG_PONG;
                pingBMessage.header.org_port = (uint16_t)local_address.sin_port;
                pingBMessage.header.org_ip = local_address.sin_addr.s_addr;
                pingBMessage.header.msg_id = (uint32_t)client_message[12]*256*256*256 + (uint32_t)client_message[13]*256*256 + (uint32_t)client_message[14]*256 + (uint32_t)client_message[15];
                
                //pingB front
                int selectedNumberForPongIP = 0;
                if(currentConnectedNodes >= 5){
                    selectedNumberForPongIP = 5;
                }else if(currentConnectedNodes >=0 && currentConnectedNodes <5){
                    selectedNumberForPongIP = currentConnectedNodes;
                }else{
                    printf("connection_handler@wrong current connected nodes in pong response\n");
                    continue;
                }
                pingBMessage.pong_front.entry_size = selectedNumberForPongIP;
                pingBMessage.pong_front.sbz = 0;
                
                //pingB front entries
                int i=0, j=0;
                while(i<selectedNumberForPongIP && j<MaxListener){
                        if(nodeTable[j].valid == 1){
                            pingBMessage.pong_entry_array[i].ip.s_addr = nodeTable[j].sin_addr.s_addr;
                            pingBMessage.pong_entry_array[i].port = nodeTable[j].sin_port;
                            pingBMessage.pong_entry_array[i].sbz = 0;
                            i++;
                        }
                    j++;
                }
                if(send(sock, (char *)&pingBMessage, 129, 0) < 0){
                    printf("connection_handler->pongMsg@failed to send pong msg\n");
                }
                
                
            }
            else{
                printf("connection_handler->pingA@wrong ttl number\n");
            }
        }else if(message.msg_type == MSG_PONG){
            
            struct in_addr pongServer;
            pongServer.s_addr = (uint32_t)client_message[8] + (uint32_t)client_message[9]*256 + (uint32_t)client_message[10]*256*256 + (uint32_t)(client_message[11])*256*256*256;
            
            printf("pong@%s is alive!\n", inet_ntoa(pongServer));
            
        }else if(message.msg_type == MSG_BYE){
            printf("connection_handler@bye msg\nClosing tcp connection...\n");
            int i;
            for(i=0; i<MaxListener; i++){
                if(sock == *nodeTable[i].socket){
                    free(socket_desc);
                    nodeTable[i].valid = 0;
                    return 0;
                }
            }
            if(i==MaxListener){
                printf("IP not found in our table\n");
            }
            
        }else if(message.msg_type == MSG_QUERY){
//            printf("connection_handler@query msg\n");
            printf("connection_handler@msg_query:search %d\n", client_message[16]);
            //broadcast to other nodes except the sender;
            int sentNum = 0;
            
            //construct query msg;
            struct QueryMsg queryMessage;
            queryMessage.message_key = client_message[16];
            queryMessage.header.version = P_VERSION;
            queryMessage.header.ttl = message.ttl;
            queryMessage.header.msg_type = MSG_QUERY;
            queryMessage.header.org_ip = (uint32_t)inet_addr(inet_ntoa(server.sin_addr));
            
            
            
            //find the ip that this sock connected to.
            int _tempCountForFindIP = 0;
            struct in_addr connectedIP;
            while(_tempCountForFindIP < MaxListener){
                if(nodeTable[_tempCountForFindIP].valid == 1 && *nodeTable[_tempCountForFindIP].socket == sock){
                    connectedIP.s_addr = nodeTable[_tempCountForFindIP].sin_addr.s_addr;
                    break;
                }
                _tempCountForFindIP++;
            }
            
            
            
            
            
            for(int j=0; j<currentConnectedNodes; j++){
                //broadcast the query;
                if(nodeTableForOutgoing[j].valid == 1 && nodeTableForOutgoing[j].sin_addr.s_addr != connectedIP.s_addr){
                    queryMessage.header.org_port = nodeTableForOutgoing[j].sin_port;
                    queryMessage.header.msg_id = hashMessageID(queryMessage.header.org_ip, queryMessage.header.org_port, messageSequence);
                    messageSequence++;
                    
                    //send msg and receive;
                    if(send(*nodeTableForOutgoing[j].socket, (char *)&queryMessage, HLEN + 129, 0)<0){
                        printf("query@send query msg failed.\n");
                        break;
                    }
                    sentNum++;
                    
                    //test code;
                    printf("query@broadcast to %s\n", inet_ntoa(nodeTableForOutgoing[j].sin_addr));
                }
            }
            
            //last node; no one to send;
            if(sentNum == 0){
                //get its own ip and port
                struct sockaddr_in local_address;
                socklen_t addr_size = sizeof(local_address);
                getsockname(sock, (struct sockaddr *)&local_address, &addr_size);
                
                
                struct QueryHitMsg queryHitMessage;
                queryHitMessage.header.version = P_VERSION;
                queryHitMessage.header.ttl = message.ttl;
                queryHitMessage.header.msg_type = MSG_QHIT;
                queryHitMessage.header.org_ip = (uint32_t)inet_addr(inet_ntoa(server.sin_addr));
                
                queryHitMessage.header.org_port = local_address.sin_port;
                queryHitMessage.header.msg_id = hashMessageID(queryHitMessage.header.org_ip, queryHitMessage.header.org_port, messageSequence);
                messageSequence++;
                
                
                //search local resource;
                printf("connection_handler@msg_query: search local %d\n", client_message[16]);
                
                for(int i=0; i<MaxNumOfRes; i++){
                    if(client_message[16] == resourceEntryTable[i].key){
                        queryHitMessage.resourceList[0].resource_id = resourceEntryTable[i].key;
                        queryHitMessage.resourceList[0].resource_value = atoi(resourceEntryTable[i].entry);
                        queryHitMessage.entry_size = 1;
                        break;
                    }else{
                        queryHitMessage.entry_size = 0;
                    }
                }
                
                if(queryHitMessage.entry_size == 1){
                    printf("MSG_QUERY@resource found.\n");
                }else{
                    printf("MSG_QUERY@resource not found.\n");
                }
                
                //send msg and receive;
                if(send(*nodeTableForOutgoing[_tempCountForFindIP].socket, (char *)&queryHitMessage, HLEN + 129, 0)<0){
                    printf("query@send query msg failed.\n");
                    break;
                }
            }
            
        }else if(message.msg_type == MSG_QHIT){
//            printf("connection_handler@query hit msg\n");
            
            //display and copy the result;
            printf("connection_handler->MSG_QHIT@entry_size=%d\n", client_message[16]);
            if(client_message[16] == 0){
                printf("connnection_handler@msg_qhit: resource not found!\n");
            }
            else{
                printf("QHIT@%d resources found\n", client_message[16]);
            }
//            struct QueryHitMsg queryHitMessage;
//            queryHitMessage.header.version = P_VERSION;
//            queryHitMessage.header.ttl = MAX_TTL;
//            queryHitMessage.header.msg_type = MSG_QHIT;
//            queryHitMessage.entry_size = 4;
//            send(sock, (char *)&queryHitMessage, HLEN + 129, 0);
        }
        else{
            printf("connection_handler@wrong msg type!\n");
            continue;
        }
        
//        printf("connection_handler@message.msg_type:%x\n", message.msg_type);
        
        
    }
    if(read_size == 0){
        printf("Client disconnected.\n");
        fflush(stdout);
    }
    else if(read_size == -1)
    {
        perror("LOL! recv failed.\n");
    }
    
    //Free the socket pointer
    free(socket_desc);
    return 0;
}







/***************** handle incoming tcp connection requests *****************************/
void *server_handler(void *mes){

    //Set up socket
    int socket_desc, client_sock, c , *new_sock;
    struct sockaddr_in client;
    
    //Create socket
    socket_desc = socket(AF_INET , SOCK_STREAM , 0);
    if (socket_desc == -1)
    {
        printf("server:Could not create socket.\n");
        return NULL;
    }
    
    //Prepare the sockaddr_in structure
    server.sin_family = AF_INET;
    server.sin_addr.s_addr = INADDR_ANY;
    server.sin_port = htons(PORT_DEFAULT);
    
    //Bind
    if( bind(socket_desc,(struct sockaddr *)&server , sizeof(server)) < 0)
    {
        //print the error message
        perror("server:Bind failed. Error");
        return NULL;
    }
    
    //Listen
    listen(socket_desc, MaxListener);
    //printf("Listening on port:%d\n",PORT_DEFAULT);
    
    //initialize the node table;
    iniNodeTable();
    
    //initialize resource table;
    iniResTable();
    
    //Accept and incoming connection
    c = sizeof(struct sockaddr_in);
    while( (client_sock = accept(socket_desc, (struct sockaddr *)&client, (socklen_t*)&c)) )
    {
        //		printf("Connection accepted.\n");
        printf("Shell:coming connection.\n");
        pthread_t sniffer_thread;
        new_sock = (int* )malloc(1);
        *new_sock = client_sock;
        
        if( pthread_create( &sniffer_thread , NULL ,  connection_handler , (void*) new_sock) < 0)
        {
            perror("server:could not create thread");
            return NULL;
        }
        
        //maintain the neighbor ip port table with specific socket number;
        for(int i=0; i<MaxListener; i++){
            if(nodeTable[i].valid == 0){
                nodeTable[i].sin_port = client.sin_port;
                nodeTable[i].sin_addr.s_addr = client.sin_addr.s_addr;
                nodeTable[i].socket = (int *)new_sock;
                nodeTable[i].valid = 1;
                currentConnectedNodes++;
                break;
            }
        }
        
        
        
    }
    
    if (client_sock < 0)
    {
        perror("server:accept failed");
        return NULL;
    }
}


/********************** Start of socket connection ********************************/
int main(int argc , char *argv[])
{
    int mes = 1;
    pthread_t serverThread;
    pthread_t heart_beat_thread;
    char shellCommand[lengthOfCommand];
    if( pthread_create( &serverThread , NULL ,  server_handler , (void *)mes) < 0)
    {
        perror("main:could not create thread");
        return -1;
    }
    
    if( pthread_create( &heart_beat_thread , NULL ,  heart_beat , (void *)mes) < 0)
    {
        perror("main:could not create thread");
        return -1;
    }
    
    //command identification area
    while(1){
        printf("$");
        fgets(shellCommand, 128, stdin);
        char tempShellCommand[lengthOfCommand];
        char tempCommandForParse[lengthOfCommand];
        memset(tempShellCommand, '\0', lengthOfCommand * sizeof(char));
        memset(tempCommandForParse, '\0', lengthOfCommand * sizeof(char));
        strcpy(tempCommandForParse, shellCommand);
        
        //identify the command;
        int i=0;
        while(shellCommand[i] != '\n'){
            if(shellCommand[i] != ' '){
                tempShellCommand[i] = shellCommand[i];
            }else{
                break;
            }
            i++;
        }
        tempShellCommand[i+1] = '\0';
        
        if(strcmp(tempShellCommand, "help") == 0){
            printf("quit -- quit application\njoin ip -- join node with its ip\npingA ip -- A type ping node with its ip address\npingB -- B type ping node with its ip address\nquery keyword -- search keyword\nBye ip -- disconnect from node using the ip of node\n");
        }
        else if(strcmp(tempShellCommand, "quit") == 0){
            //should kill all threads;
            printf("Shell:quit.\n");
            return 0;
        }
        else if(strcmp(tempShellCommand, "join") == 0){
            //hint the correct command;
            printf("Shell:join\n");
            
            int sock;
            struct sockaddr_in server;
            char serverAddress[128];
            
            int i=0;
            while(tempCommandForParse[i] != ' '&& tempCommandForParse[i] != '\n'){
                i++;
            }
            i++;
            int _tempCount = 0;
            while(tempCommandForParse[i] != '\n'){
                serverAddress[_tempCount] = tempCommandForParse[i];
                _tempCount++;
                i++;
            }
            serverAddress[_tempCount] = '\0';
            printf("Shell:Connect to:%s\n", serverAddress);
            
            //Create socket
            sock = socket(AF_INET , SOCK_STREAM , 0);
            if (sock == -1)
            {
                printf("Could not create socket\n");
            }
            
            server.sin_addr.s_addr = inet_addr(serverAddress);
            server.sin_family = AF_INET;
            server.sin_port = htons(PORT_DEFAULT);
            
            //Connect to remote server
            if (connect(sock , (struct sockaddr *)&server , sizeof(server)) < 0)
            {
                perror("Connect failed");
                continue;
            }
            
            //get current port number:
            struct sockaddr_in local_address;
            socklen_t addr_size = sizeof(local_address);
            getsockname(sock, (struct sockaddr *)&local_address, &addr_size);
//            printf("Port:%d\n", local_address.sin_port);
            
            //parse command;
            printf("Connected to %s. Try to join....\n", serverAddress);
            struct P2P_h joinMessage;
            joinMessage.version = P_VERSION;
            joinMessage.ttl = PING_TTL_HB;
            joinMessage.msg_type = MSG_JOIN;
            joinMessage.org_port = (uint16_t)local_address.sin_port;
            joinMessage.length = 0;
            joinMessage.org_ip = (uint32_t)inet_addr(serverAddress);
            joinMessage.msg_id = hashMessageID(joinMessage.org_ip, joinMessage.org_port, messageSequence);
            send(sock, (char *)&joinMessage, HLEN+1, 0);
            printf("join message sent...\n");
            messageSequence++;
            
            
            //ack from server;
            struct JoinResponseMsg responseToJoin;
            recv(sock, (char *)&responseToJoin, sizeof(struct JoinResponseMsg)+1, 0);
            if(responseToJoin.joinBody.status == JOIN_ACC){
                printf("Join succeed.\n");
                
                //add to outgoing Node table;
                for(int i=0; i<MaxListener; i++){
                    if(nodeTableForOutgoing[i].valid == 0){
                        nodeTableForOutgoing[i].valid = 1;
                        nodeTableForOutgoing[i].sin_port = responseToJoin.header.org_port;
                        nodeTableForOutgoing[i].sin_addr.s_addr = responseToJoin.header.org_ip;
                        nodeTableForOutgoing[i].socket = (int *)&sock;
                        currentConnectedNodesForOutgoing++;
                        break;
                    }
                }
            }else{
                printf("Join failed.\n");
                continue;
                //kill tcp connection;
                close(sock);
            }
        }
        else if(strcmp(tempShellCommand, "pingA") == 0){
            //get server address
            char serverAddress[128];
            int rankInInodeTable = -1;
            
            int i=0;
            while(tempCommandForParse[i] != ' '&& tempCommandForParse[i] != '\n'){
                i++;
            }
            i++;
            int _tempCount = 0;
            while(tempCommandForParse[i] != '\n'){
                serverAddress[_tempCount] = tempCommandForParse[i];
                _tempCount++;
                i++;
            }
            serverAddress[_tempCount] = '\0';
            printf("pingA to %s\n", serverAddress);
            
            int j = 0;
            for(; j < MaxListener; j++){
                if(strcmp(inet_ntoa(nodeTableForOutgoing[j].sin_addr), serverAddress) == 0 && nodeTableForOutgoing[j].valid == 1){
                    rankInInodeTable = j;
                    break;
                }
            }
            if(j == MaxListener){
                printf("IP not found in our table.\n");
                continue;
            }
            
            
            //construct ping msg;
            struct P2P_h pingAMessage;
            pingAMessage.version = P_VERSION;
            pingAMessage.ttl = PING_TTL_HB;
            pingAMessage.msg_type = MSG_PING;
            pingAMessage.org_port = (uint16_t)nodeTableForOutgoing[rankInInodeTable].sin_port;
            pingAMessage.length = 0;
            pingAMessage.org_ip = (uint32_t)inet_addr(inet_ntoa(nodeTableForOutgoing[rankInInodeTable].sin_addr));
            pingAMessage.msg_id = hashMessageID(pingAMessage.org_ip, pingAMessage.org_port, messageSequence);
            messageSequence++;
            //send ping msg;
            if(send(*nodeTableForOutgoing[rankInInodeTable].socket, (char *)&pingAMessage, HLEN+1, 0) < 0){
                printf("main: send failed in pingA\n");
            }else{
                printf("pingA message sent...\n");
            }
        }
        else if(strcmp(tempShellCommand, "Bye") == 0){
            //parse ip address
            char serverAddress[128];
            int rankInInodeTable = -1;
            
            int i=0;
            while(tempCommandForParse[i] != ' '&& tempCommandForParse[i] != '\n'){
                i++;
            }
            i++;
            int _tempCount = 0;
            while(tempCommandForParse[i] != '\n'){
                serverAddress[_tempCount] = tempCommandForParse[i];
                _tempCount++;
                i++;
            }
            serverAddress[_tempCount] = '\0';
            printf("pingA to %s\n", serverAddress);
            
            int j = 0;
            for(; j < MaxListener; j++){
                if(strcmp(inet_ntoa(nodeTableForOutgoing[j].sin_addr), serverAddress) == 0 && nodeTableForOutgoing[j].valid == 1){
                    rankInInodeTable = j;
                    break;
                }
            }
            if(j == MaxListener){
                printf("IP not found in our table.\n");
                continue;
            }
            
            //construct bye msg;
            struct P2P_h byeMsg;
            byeMsg.version = P_VERSION;
            byeMsg.ttl = 1;
            byeMsg.msg_type = MSG_BYE;
            
            send(*nodeTableForOutgoing[rankInInodeTable].socket, (char *)&byeMsg, HLEN+1, 0);
            close(*nodeTableForOutgoing[rankInInodeTable].socket);
            nodeTableForOutgoing[rankInInodeTable].valid = 0;
            currentConnectedNodesForOutgoing--;
            
            
        }
        else if(strcmp(tempShellCommand, "pingB") == 0){
            printf("Shell:pingB\n");
            //ttl=5;
            //get server address
            char serverAddress[128];
            int rankInInodeTable = -1;
            
            int i=0;
            while(tempCommandForParse[i] != ' '&& tempCommandForParse[i] != '\n'){
                i++;
            }
            i++;
            int _tempCount = 0;
            while(tempCommandForParse[i] != '\n'){
                serverAddress[_tempCount] = tempCommandForParse[i];
                _tempCount++;
                i++;
            }
            serverAddress[_tempCount] = '\0';
            printf("pingB to %s\n", serverAddress);
            
            int j = 0;
            for(; j < MaxListener; j++){
                if(strcmp(inet_ntoa(nodeTableForOutgoing[j].sin_addr), serverAddress) == 0 && nodeTableForOutgoing[j].valid == 1){
                    rankInInodeTable = j;
                    break;
                }
            }
            if(j == MaxListener){
                printf("IP not found in our table.\n");
                continue;
            }
            
            
            /* construct pingB  */
            struct P2P_h pingBMessage;
            pingBMessage.version = P_VERSION;
            pingBMessage.ttl = MAX_TTL;
            pingBMessage.msg_type = MSG_PING;
            pingBMessage.org_port = (uint16_t)nodeTableForOutgoing[rankInInodeTable].sin_port;
            pingBMessage.length = 0;
            pingBMessage.org_ip = (uint32_t)inet_addr(inet_ntoa(nodeTableForOutgoing[rankInInodeTable].sin_addr));
            pingBMessage.msg_id = hashMessageID(pingBMessage.org_ip, pingBMessage.org_port, messageSequence);
            messageSequence++;
            //send pingB msg;
            if(send(*nodeTableForOutgoing[rankInInodeTable].socket, (char *)&pingBMessage, HLEN+1, 0) < 0){
                printf("main: send failed in pingB\n");
            }else{
                printf("pingB message sent...\n");
            }
            
            unsigned char responseArray[128];
            memset(responseArray, 0, 128*sizeof(unsigned char));
            recv(*nodeTableForOutgoing[rankInInodeTable].socket, (char *)&responseArray, 129, 0);
            uint16_t totalNumber = responseArray[16];
            struct in_addr serverIP;
            int serverPort = -1;
            for(int i=0; i<totalNumber; i++){
                serverIP.s_addr = (uint32_t)(responseArray[20 + 8*i]) + (uint32_t)(responseArray[21 + 8*i]<<8) + (uint32_t)(responseArray[22 + 8*i]<<16) + (uint32_t)(responseArray[23 + 8*i]<<24);
                serverPort = (responseArray[24 + 8*i]) + (responseArray[25 + 8*i]<<8);
                printf("IP:%s Port:%d\n", inet_ntoa(serverIP), serverPort);
            }
            
        }
        else if(strcmp(tempShellCommand, "query") == 0){
            //get query msg key;
            char queryKey[128];
            int queryKeyInInt = -1;
            
            int i=0;
            while(tempCommandForParse[i] != ' '&& tempCommandForParse[i] != '\n'){
                i++;
            }
            i++;
            int _tempCount = 0;
            while(tempCommandForParse[i] != '\n'){
                queryKey[_tempCount] = tempCommandForParse[i];
                _tempCount++;
                i++;
            }
            queryKey[_tempCount] = '\0';
            queryKeyInInt = atoi(queryKey);
            printf("query@query key:%d\n", queryKeyInInt);
            
            //construct the basic elements in query msg;
            struct QueryMsg queryMessage;
            queryMessage.message_key = queryKeyInInt;
            queryMessage.header.version = P_VERSION;
            queryMessage.header.ttl = MAX_TTL;
            queryMessage.header.msg_type = MSG_QUERY;
            queryMessage.header.org_ip = (uint32_t)inet_addr(inet_ntoa(server.sin_addr));
            
            
            
            //broadcast the query;
            for(int i=0; i<MaxListener; i++){
                if(nodeTableForOutgoing[i].valid == 1){
                    queryMessage.header.org_port = nodeTableForOutgoing[i].sin_port;
                    queryMessage.header.msg_id = hashMessageID(queryMessage.header.org_ip, queryMessage.header.org_port, messageSequence);
                    messageSequence++;
                    
                    //send msg and receive;
                    if(send(*nodeTableForOutgoing[i].socket, (char *)&queryMessage, HLEN + 129, 0)<0){
                        printf("query@send query msg failed.\n");
                        break;
                    }
                }
            }
        }
        else if(strcmp(tempShellCommand, "listInNode") == 0){
            for(int i=0; i < currentConnectedNodes; i++){
                printf("Valid:%d IP:%s Port:%d Sock:%d\n", nodeTable[i].valid,inet_ntoa(nodeTable[i].sin_addr), nodeTable[i].sin_port, *nodeTable[i].socket);
            }
        }
        else if(strcmp(tempShellCommand, "listOutNode") == 0){
            printf("list outgoing node\n");
            for(int i=0; i < currentConnectedNodesForOutgoing; i++){
                printf("IP:%s Port:%d Sock:%d\n", inet_ntoa(nodeTableForOutgoing[i].sin_addr), nodeTableForOutgoing[i].sin_port, *nodeTableForOutgoing[i].socket);
            }
        }
        else{
            printf("Wrong input! Try 'help' to find more details.\n");
        }
    }
    
    return 0;
}
/********************** End of socket connection ***********/