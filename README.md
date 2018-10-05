# Wrapped Infinity for HBase RDMA edition

This is a part of RDMA for HBase, 2018 RDMA Challenge. This library is specially designed for HBase, and other users must NOT use it (unless you want to spend a crazy week contributing to this project).

## Interface

Please view [this file](https://github.com/recolic/infinity/blob/master/src/infinity/java-wrapper/RdmaNative.java).

```java
public class RdmaNative {
    static {
        System.loadLibrary("RdmaNative");
        RdmaNative.rdmaInitGlobal();
        // The kernel will cleanup all resources on exit. Use finalize() for another class in the future.
    }
    // This function must be called exactly once to construct necessary structs.
    // It will construct rdmaContext and other global var.
    public static native boolean rdmaInitGlobal();
    // This function must be called exactly once to destruct global structs.
    public static native void rdmaDestroyGlobal();
    
    // Connect to remote host. Blocked operation. If failed, return null.
    public native RdmaClientConnection rdmaConnect(String addr, int port);
    // This function must be called once by server, to bind a port.
    public native boolean rdmaBind(int port);
    // Wait and accept a connection. Blocked operation. If failed, return null.
    public native RdmaServerConnection rdmaBlockedAccept();

    public class RdmaClientConnection {
        private long ptrCxxClass;

        public native boolean isClosed();
        // returned ByteBuffer MAY be invalidated on next readResponse(). (invalidated if the bytebuffer is created without copying data)
        // return null object if the read failed.
        public native ByteBuffer readResponse(); // blocked. Will wait for the server for response.
        public native boolean writeQuery(ByteBuffer data); // blocked until success.
        public native boolean close(); // You may call it automatically in destructor. It MUST be called once.
    }

    public class RdmaServerConnection {
        /* 
            The server holds two buffer registered for RDMA. DynamicBufferTokenBuffer holds struct<Magic, currentQuerySize, DynamicBufferToken>,
            and DynamicBuffer holds the real query or response data. 
            In fact, DynamicBufferTokenBuffer is the current "status" of server. Its "magic" is the current state of a state machine. 

            The Magic is inited to MAGIC_CONNECTED and set to MAGIC_SERVER_BUFFER_READY once DynamicBufferToken is ready for using.
            On accepting connection, the server creates a 4K DynamicBuffer, and register it, put the token into DynamicBufferTokenBuffer.
        begin:
            Then the server send the DynamicBufferTokenBufferToken to client as userData. Then the client write its querySize into 
            DynamicBufferTokenBuffer. On server calling isQueryReadable(), it allocates the buffer if querySize available.
            If the client's query size is less than current DynamicBufferSize, it just write it in the existing dynamic buffer. 
            If the query is larger, the client must read server magic again and again, until the Magic is MAGIC_SERVER_BUFFER_READY. 
            Then write its query in.
            If the query is larger, the server must resize and re-register the DynamicBuffer, put its new token into 
            DynamicBufferTokenBuffer, and set the Magic to MAGIC_SERVER_BUFFER_READY.
            
            Once the query is wrote into server, the client must set Magic to MAGIC_QUERY_WROTE. The server call isQueryReadable() to check 
            if the Magic is MAGIC_QUERY_WROTE. After reading it, the server set its magic to MAGIC_QUERY_READ to avoid duplicated QueryReadable.

            After the response is ready, the server use writeResponse to write its data to local buffer. The DynamicBuffer will be reallocated if 
            its not large enough. The DynamicBufferToken is updated, then the Magic is set to MAGIC_RESPONSE_READY. The client check the value again
            and again before reading the response by RDMA.

            On closing a connection, just destruct the Rdma*Connection, and destruct the insiding C++ QP. 
            If we want to reuse the connection, just `goto begin` and everything works perfectly!
        */
        private long ptrCxxClass;

        public native boolean isClosed();
        public native boolean isQueryReadable();
        // get client ip address. ipv6 is not tested yet. (code for ipv6 maybe buggy)
        public native byte [] getClientIp();
        // returned ByteBuffer MAY be invalidated on next readQuery()
        // return null object if the read failed.
        public native ByteBuffer readQuery();
        public native boolean writeResponse(ByteBuffer data);
        public native boolean close(); // You may call it automatically in destructor. It MUST be called once.
    }
}
```

## Build

```sh
$ make all
```

## Test

The test is designed for developer's private usage. So the testing is naive and not reliable. You must refer to HBase testing for what you want.

```sh
# Basic function test
$ cd release
10.10.0.111 $ ./RdmaNativeTest
10.10.0.112 $ ./RdmaNativeTest 10.10.0.111
# Naive performance test
10.10.0.111 $ ./perf
10.10.0.112 $ ./perf 10.10.0.111
```



----

# Infinity - A lightweight C++ RDMA library for InfiniBand

Infinity is a simple, powerful, object-oriented abstraction of ibVerbs. The library enables users to build sophisticated applications that use Remote Direct Memory Access (RDMA) without sacrificing performance. It significantly lowers the barrier to get started with RDMA programming. Infinity provides support for two-sided (send/receive) as well as one-sided (read/write/atomic) operations. The library is written in C++ and has been ported to Rust ([Infinity-Rust](https://github.com/utaal/infinity-rust/)) by @utaal.

## Installation

Installing ''ibVerbs'' is a prerequisite before building Infinity. The output is located in ''release/libinfinity.a''.

```sh
$ make library # Build the library
$ make examples # Build the examples
```
## Using Infinity

Using Infinity is straight-forward and requires only a few lines of C++ code.

```C
// Create new context
infinity::core::Context *context = new infinity::core::Context();

// Create a queue pair
infinity::queues::QueuePairFactory *qpFactory = new  infinity::queues::QueuePairFactory(context);
infinity::queues::QueuePair *qp = qpFactory->connectToRemoteHost(SERVER_IP, PORT_NUMBER);

// Create and register a buffer with the network
infinity::memory::Buffer *localBuffer = new infinity::memory::Buffer(context, BUFFER_SIZE);

// Get information from a remote buffer
infinity::memory::RegionToken *remoteBufferToken = new infinity::memory::RegionToken(REMOTE_BUFFER_INFO);

// Read (one-sided) from a remote buffer and wait for completion
infinity::requests::RequestToken requestToken(context);
qp->read(localBuffer, remoteBufferToken, &requestToken);
requestToken.waitUntilCompleted();

// Write (one-sided) content of a local buffer to a remote buffer and wait for completion
qp->write(localBuffer, remoteBufferToken, &requestToken);
requestToken.waitUntilCompleted();

// Send (two-sided) content of a local buffer over the queue pair and wait for completion
qp->send(localBuffer, &requestToken);
requestToken.waitUntilCompleted();

// Close connection
delete remoteBufferToken;
delete localBuffer;
delete qp;
delete qpFactory;
delete context;
```

## Citing Infinity in Academic Publications

This library has been created in the context of my work on parallel and distributed join algorithms. Detailed project descriptions can be found in two papers published at ACM SIGMOD 2015 and VLDB 2017. Further publications concerning the use of RDMA have been submitted to several leading systems conferences and are currently under review. Therefore, for the time being, please refer to the publications listed below when referring to this library.

Claude Barthels, Simon Loesing, Gustavo Alonso, Donald Kossmann.
**Rack-Scale In-Memory Join Processing using RDMA.**
*Proceedings of the 2015 ACM SIGMOD International Conference on Management of Data, June 2015.*  
**PDF:** http://barthels.net/publications/barthels-sigmod-2015.pdf


Claude Barthels, Ingo Müller, Timo Schneider, Gustavo Alonso, Torsten Hoefler.
**Distributed Join Algorithms on Thousands of Cores.**
*Proceedings of the VLDB Endowment, Volume 10, Issue 5, January 2017.*  
**PDF:** http://barthels.net/publications/barthels-vldb-2017.pdf

---

```
@inproceedings{barthels-sigmod-2015,
  author    = {Claude Barthels and
               Simon Loesing and
               Gustavo Alonso and
               Donald Kossmann},
  title     = {Rack-Scale In-Memory Join Processing using {RDMA}},
  booktitle = {{SIGMOD}},
  pages     = {1463--1475},
  year      = {2015},
  url       = {http://doi.acm.org/10.1145/2723372.2750547}
}
```



```
@article{barthels-pvldb-2017,
  author    = {Claude Barthels and
               Ingo M{\"{u}}ller and
               Timo Schneider and
               Gustavo Alonso and
               Torsten Hoefler},
  title     = {Distributed Join Algorithms on Thousands of Cores},
  journal   = {{PVLDB}},
  volume    = {10},
  number    = {5},
  pages     = {517--528},
  year      = {2017},
  url       = {http://www.vldb.org/pvldb/vol10/p517-barthels.pdf}
}
```

