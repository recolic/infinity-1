#include "RdmaImpl.hpp"
#include <iostream>
#include <string>
#include <unistd.h>
#include <time.h>
#include "org_apache_hadoop_hbase_ipc_RdmaNative.h"

#define dat_size 5000000
using namespace std;
timespec diff(timespec start, timespec end)
{
    timespec temp;
    if ((end.tv_nsec-start.tv_nsec)<0) {
        temp.tv_sec = end.tv_sec-start.tv_sec-1;
        temp.tv_nsec = 1000000000+end.tv_nsec-start.tv_nsec;
    } else {
        temp.tv_sec = end.tv_sec-start.tv_sec;
        temp.tv_nsec = end.tv_nsec-start.tv_nsec;
    }
    return temp;
}

int main(int argc, char **argv) {
    bool isServer = true;
    int serverPort = 25543;
    string serverName;
    Java_org_apache_hadoop_hbase_ipc_RdmaNative_rdmaInitGlobal(NULL, NULL);
    if(argc == 1) {
        cout << "server mode" << endl;
        Java_org_apache_hadoop_hbase_ipc_RdmaNative_rdmaBind(NULL, NULL, serverPort);
        CRdmaServerConnectionInfo conn;
        void *dataPtr;
        uint64_t size;
        string responseData(dat_size, 'a');

        timespec time1, time2,time3,time4,time5;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);
        conn.waitAndAccept();
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);

        while(!conn.isQueryReadable());
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time3);

        conn.readQuery(dataPtr, size);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time4);

        conn.writeResponse(responseData.data(), responseData.size());
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time5);
    cout<<diff(time1,time2).tv_sec<<":"<<diff(time1,time2).tv_nsec<<endl;
    cout<<diff(time2,time3).tv_sec<<":"<<diff(time2,time3).tv_nsec<<endl;
    cout<<diff(time3,time4).tv_sec<<":"<<diff(time3,time4).tv_nsec<<endl;
    cout<<diff(time4,time5).tv_sec<<":"<<diff(time4,time5).tv_nsec<<endl;


        sleep(5); // the client is still reading thr response!
    }
    else {
        cout << "client mode" << endl;
        serverName = argv[1];
        CRdmaClientConnectionInfo conn;
        string queryData(dat_size, 'i');
        infinity::memory::Buffer *bufPtr;

        timespec time1, time2,time3,time4,time5;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);
        conn.connectToRemote(serverName.c_str(), serverPort);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);

        conn.writeQuery((void *)queryData.data(), queryData.size());
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time3);
        while(!conn.isResponseReady());
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time4);
        conn.readResponse(bufPtr);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time5);
    cout<<diff(time1,time2).tv_sec<<":"<<diff(time1,time2).tv_nsec<<endl;
    cout<<diff(time2,time3).tv_sec<<":"<<diff(time2,time3).tv_nsec<<endl;
    cout<<diff(time3,time4).tv_sec<<":"<<diff(time3,time4).tv_nsec<<endl;
    cout<<diff(time4,time5).tv_sec<<":"<<diff(time4,time5).tv_nsec<<endl;


    }
    Java_org_apache_hadoop_hbase_ipc_RdmaNative_rdmaDestroyGlobal(NULL, NULL);
}
