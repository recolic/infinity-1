#include <unistd.h>
#include <cstdlib>
#include <sys/socket.h>
#include <sys/types.h>
#include <stdexcept>
#include <exception>
#include <netdb.h>

typedef int fd;
#include <cerrno>
#define nullptr NULL

#include <iostream>
using namespace std;
#include <time.h>
#define dat_size 100

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

namespace rlib {
    namespace impl {
        static inline fd unix_quick_listen(const std::string &addr, uint16_t port) {
            addrinfo *psaddr;
            addrinfo hints{0};
            fd listenfd;

            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            hints.ai_flags = AI_PASSIVE;    /* For wildcard IP address */
            int _f = getaddrinfo(addr.c_str(), std::to_string((long long int)port).c_str(), &hints, &psaddr);
            if (_f != 0) throw std::runtime_error("Failed to getaddrinfo. returnval={}, check `man getaddrinfo`'s return value.");

            bool success = false;
            for (addrinfo *rp = psaddr; rp != nullptr; rp = rp->ai_next) {
                listenfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (listenfd == -1)
                    continue;
                int reuse = 1;
                if (setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse, sizeof(int)) < 0)
                    throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
                if (bind(listenfd, rp->ai_addr, rp->ai_addrlen) == 0) {
                    success = true;
                    break;
                }
                close(listenfd);
            }
            if (!success) throw std::runtime_error("Failed to bind {}:{}.");

            if (-1 == ::listen(listenfd, 16)) throw std::runtime_error("listen failed.");

            //rlib_defer([psaddr] { freeaddrinfo(psaddr); });
            return listenfd;
        }

        static inline fd unix_quick_connect(const std::string &addr, uint16_t port) {
            addrinfo *paddr;
            addrinfo hints{0};
            fd sockfd;

            hints.ai_family = AF_UNSPEC;
            hints.ai_socktype = SOCK_STREAM;
            int _f = getaddrinfo(addr.c_str(), std::to_string((long long int)port).c_str(), &hints, &paddr);
            if (_f != 0)
                throw std::runtime_error("getaddrinfo failed. Check network connection to {}:{}; returnval={}, check `man getaddrinfo`'s return value.");
            //rlib_defer([paddr] { freeaddrinfo(paddr); });

            bool success = false;
            for (addrinfo *rp = paddr; rp != NULL; rp = rp->ai_next) {
                sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
                if (sockfd == -1)
                    continue;
                int reuse = 1;
                if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, (const char *) &reuse, sizeof(int)) < 0)
                    throw std::runtime_error("setsockopt(SO_REUSEADDR) failed");
                if (connect(sockfd, rp->ai_addr, rp->ai_addrlen) == 0) {
                    success = true;
                    break; /* Success */
                }
                close(sockfd);
            }
            if (!success) throw std::runtime_error("Failed to connect to any of these addr.");

            return sockfd;
        }
    }
    using impl::unix_quick_connect;
    using impl::unix_quick_listen;
}

static ssize_t readn(int fd, void *vptr, size_t n) //Return -1 on error, read bytes on success, blocks until nbytes done.
{
    size_t  nleft;
    ssize_t nread;
    char   *ptr;

    ptr = (char *)vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nread = read(fd, ptr, nleft)) < 0) {
            if (errno == EINTR)
                nread = 0;      /* and call read() again */
            else
                return (-1);
        } else if (nread == 0)
            return (-1);              /* EOF */

        nleft -= nread;
        ptr += nread;
    }
    return (n);         /* return success */
}
static ssize_t writen(int fd, const void *vptr, size_t n) //Return -1 on error, read bytes on success, blocks until nbytes done.
{
    size_t nleft;
    ssize_t nwritten;
    const char *ptr;

    ptr = (const char *)vptr;
    nleft = n;
    while (nleft > 0) {
        if ( (nwritten = write(fd, ptr, nleft)) <= 0) {
            if (nwritten < 0 && errno == EINTR)
                nwritten = 0;   /* and call write() again */
            else
                return (-1);    /* error */
         }

         nleft -= nwritten;
         ptr += nwritten;
    }
    return (n);
}


using namespace rlib;

int main(int argc, char **argv) {
    bool isServer = true;
    int serverPort = 25543;
    string serverName;
    if(argc == 1) {
        cout << "server mode" << endl;
        void *dataPtr;
        uint64_t size;
        string responseData(dat_size, 'a');

        timespec time1, time2,time3,time4,time5;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);
    int listenfd = unix_quick_listen("0.0.0.0", serverPort);
	int conn = accept(listenfd, NULL,NULL);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time3);

    readn(conn, &size, sizeof(uint64_t));
    dataPtr = malloc(dat_size);
    readn(conn, dataPtr, dat_size);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time4);

    writen(conn, &size, sizeof(uint64_t));
    writen(conn, responseData.data(), responseData.size());
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
        string queryData(dat_size, 'i');

        timespec time1, time2,time3,time4,time5;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time1);
    int conn = unix_quick_connect(serverName, serverPort);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time2);
    uint64_t size = queryData.size();
    writen(conn, &size, sizeof(uint64_t));
    writen(conn, queryData.data(), queryData.size());

    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time3);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time4);

    readn(conn, &size, sizeof(uint64_t));
    void *buf = malloc(dat_size);
    readn(conn, buf, dat_size);
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &time5);
    cout<<diff(time1,time2).tv_sec<<":"<<diff(time1,time2).tv_nsec<<endl;
    cout<<diff(time2,time3).tv_sec<<":"<<diff(time2,time3).tv_nsec<<endl;
    cout<<diff(time3,time4).tv_sec<<":"<<diff(time3,time4).tv_nsec<<endl;
    cout<<diff(time4,time5).tv_sec<<":"<<diff(time4,time5).tv_nsec<<endl;


    }
}

