package org.apache.hadoop.hbase.ipc;

//import org.apache.yetus.audience.InterfaceAudience;
import java.nio.ByteBuffer;
//@InterfaceAudience.Public

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
