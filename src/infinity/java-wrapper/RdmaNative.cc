#include "org_apache_hadoop_hbase_ipc_RdmaNative.h"
#include "org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection.h"
#include "org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection.h"

#include "RdmaImpl.hpp"

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative
 * Method:    rdmaInitGlobal
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_rdmaInitGlobal(JNIEnv *, jclass) {
    rdma_debug << "WWWWWWWWWWWARNING: Someone is creating rdmaGlobal" << std::endl;
    if (context != nullptr) {
        rdma_error << "You are init global but context is already inited" << std::endl;
    }
    try {
        context = new core::Context();
        qpFactory = new queues::QueuePairFactory(context);
    } catch (std::exception &ex) {
        rdma_error << "Init Exception: " << ex.what() << std::endl;
        checkedDelete(context);
        checkedDelete(qpFactory);
        return JNI_FALSE;
    }
    return JNI_TRUE;
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative
 * Method:    rdmaDestroyGlobal
 * Signature: ()V
 */
JNIEXPORT void JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_rdmaDestroyGlobal(JNIEnv *, jclass) {
    rdma_debug << "WWWWWWWWWWWARNING: Someone is destroying rdmaGlobal" << std::endl;
    checkedDelete(qpFactory);
    checkedDelete(context);
}

#define REPORT_FATAL(msg)                                                                                                      \
    do {                                                                                                                       \
        std::cerr << "RdmaNative FATAL: " __FILE__ ":" << __LINE__ << " errno=" << errno << ":" << msg << "Unable to pass error to Java. Have to abort..."  \
                  << std::endl;                                                                                                \
        abort();                                                                                                               \
    } while (0)

#define REPORT_ERROR(msg)                                                                                                      \
    do {                                                                                                                       \
        std::cerr << "RdmaNative ERROR: " __FILE__ ":" << __LINE__ << " errno=" << errno << ":" << msg << "Returning error code to java..." << std::endl;   \
        return NULL;                                                                                                           \
    } while (0)

#define REPORT_ERROR_BOOL(msg)                                                                                                 \
    do {                                                                                                                       \
        std::cerr << "RdmaNative ERROR: " __FILE__ ":" << __LINE__ << " errno=" << errno << ":" << msg << "Returning error code to java..." << std::endl;   \
        return JNI_FALSE;                                                                                                      \
    } while (0)

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative
 * Method:    rdmaConnect
 * Signature: (Ljava/lang/String;I)Lorg/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection;
 */
JNIEXPORT jobject JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_rdmaConnect(JNIEnv *env, jobject self,
                                                                                  jstring jServerAddr, jint jServerPort) {
    rdma_debug << "rdmaConnect called, server port " << jServerPort << std::endl;

    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection");
    if (jConnCls == NULL)
        REPORT_ERROR("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection.");
    jmethodID jConnClsInit = env->GetMethodID(jConnCls, "<init>", "(Lorg/apache/hadoop/hbase/ipc/RdmaNative;)V");
    if (jConnClsInit == NULL)
        REPORT_ERROR("Unable to find constructor org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection::<init> -> "
                     "(org/apache/hadoop/hbase/ipc/RdmaNative;)V.");
    jobject jConn = env->NewObject(jConnCls, jConnClsInit, self);
    if (jConn == NULL)
        REPORT_ERROR("Unable to create RdmaClientConnection object.");

    jboolean isCopy;
    const char *serverAddr = env->GetStringUTFChars(jServerAddr, &isCopy);
    if (serverAddr == NULL)
        REPORT_ERROR("GetStringUTFChars from jServerAddr error.");

    rdma_debug << "server addr:" << serverAddr << std::endl;

    // do connect
    try {
        CRdmaClientConnectionInfo *pConn = new CRdmaClientConnectionInfo();
        pConn->connectToRemote(serverAddr, jServerPort);
        jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
        if (jFieldCxxPtr == NULL)
            REPORT_ERROR("Unable to getFieldId `ptrCxxClass`");
        static_assert(sizeof(jlong) == sizeof(CRdmaClientConnectionInfo *), "jlong must have same size with C++ Pointer");
        env->SetLongField(jConn, jFieldCxxPtr, (jlong)pConn);
        rdma_debug << "connected! cxxptr is " << (jlong)pConn << std::endl;
    } catch (std::exception &e) {
        REPORT_ERROR(e.what());
    }

    // cleanup
    if (isCopy == JNI_TRUE) {
        env->ReleaseStringUTFChars(jServerAddr, serverAddr);
    }
    return jConn;
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative
 * Method:    rdmaBind
 * Signature: (I)Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_rdmaBind(JNIEnv *, jobject, jint jListenPort) {
    rdma_debug << "rdmaBind called with arg " << jListenPort << std::endl;
    try {
        qpFactory->bindToPort(jListenPort);
    } catch (std::exception &e) {
        REPORT_ERROR_BOOL("Failed to bind to port " << std::to_string(jListenPort) << e.what());
    }
    return JNI_TRUE;
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative
 * Method:    rdmaBlockedAccept
 * Signature: (I)Lorg/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection;
 */
JNIEXPORT jobject JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_rdmaBlockedAccept(JNIEnv *env, jobject self) {
    rdma_debug << "rdmaBlockedAccept called" << std::endl;
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection");
    if (jConnCls == NULL)
        REPORT_ERROR("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection.");
    jmethodID jConnClsInit = env->GetMethodID(jConnCls, "<init>", "(Lorg/apache/hadoop/hbase/ipc/RdmaNative;)V");
    if (jConnClsInit == NULL)
        REPORT_ERROR("Unable to find constructor org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection::<init> -> "
                     "(org/apache/hadoop/hbase/ipc/RdmaNative;)V.");
    jobject jConn = env->NewObject(jConnCls, jConnClsInit, self);
    if (jConn == NULL)
        REPORT_ERROR("Unable to create RdmaServerConnection object.");

    try {
        CRdmaServerConnectionInfo *pConn = new CRdmaServerConnectionInfo();
        pConn->waitAndAccept();
        jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
        if (jFieldCxxPtr == NULL)
            REPORT_ERROR("Unable to getFieldId `ptrCxxClass`");
        static_assert(sizeof(jlong) == sizeof(CRdmaClientConnectionInfo *), "jlong must have same size with C++ Pointer");
        env->SetLongField(jConn, jFieldCxxPtr, (jlong)pConn);
    } catch (std::exception &e) {
        REPORT_ERROR(e.what());
    }
    rdma_debug << "Accepted! jconn is " << jConn << std::endl;
    return jConn;
}

////////////////////////////////////// RdmaClientConnection Methods
/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
 * Method:    isClosed
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaClientConnection_isClosed(JNIEnv *, jobject) {
    return JNI_TRUE;
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
 * Method:    readResponse
 * Signature: ()Ljava/nio/ByteBuffer;
 */
memory::Buffer *previousResponseDataPtr = nullptr;
JNIEXPORT jobject JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaClientConnection_readResponse(JNIEnv *env,
                                                                                                             jobject self) {
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection");
    if (jConnCls == NULL)
        REPORT_ERROR("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection.");
    jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
    if (jFieldCxxPtr == NULL)
        REPORT_ERROR("Unable to getFieldId `ptrCxxClass`");
    jlong cxxPtr = env->GetLongField(self, jFieldCxxPtr);
    rdma_debug << "rdmaClientConn.readResponse called at cxxptr " << cxxPtr << std::endl;
    CRdmaClientConnectionInfo *pConn = (CRdmaClientConnectionInfo *)cxxPtr;
    if (pConn == nullptr)
        REPORT_ERROR("cxx conn ptr is nullptr. is the connection closed?");

    checkedDelete(previousResponseDataPtr);
    try {
        while (!pConn->isResponseReady())
            ;
        pConn->readResponse(previousResponseDataPtr);
        if (previousResponseDataPtr == nullptr)
            throw std::runtime_error("readResponse return null");
    } catch (std::exception &e) {
        REPORT_ERROR(e.what());
    }
    return env->NewDirectByteBuffer(previousResponseDataPtr->getData(), previousResponseDataPtr->getSizeInBytes());
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
 * Method:    writeQuery
 * Signature: (Ljava/nio/ByteBuffer;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaClientConnection_writeQuery(
    JNIEnv *env, jobject self, jobject jDataBuffer) {
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection");
    if (jConnCls == NULL)
        REPORT_ERROR_BOOL("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection.");
    jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
    if (jFieldCxxPtr == NULL)
        REPORT_ERROR_BOOL("Unable to getFieldId `ptrCxxClass`");
    jlong cxxPtr = env->GetLongField(self, jFieldCxxPtr);
    rdma_debug << "rdmaClientConn.writeQuery called at conn " << cxxPtr << std::endl;
    CRdmaClientConnectionInfo *pConn = (CRdmaClientConnectionInfo *)cxxPtr;
    if (pConn == nullptr)
        REPORT_ERROR_BOOL("cxx conn ptr is nullptr. is the connection closed?");

    void *tmpJAddr = env->GetDirectBufferAddress(jDataBuffer);
    if (tmpJAddr == NULL)
        REPORT_ERROR_BOOL("jDataBuffer addr is null");
    try {
        pConn->writeQuery(tmpJAddr, env->GetDirectBufferCapacity(jDataBuffer));
    } catch (std::exception &e) {
        REPORT_ERROR_BOOL(e.what());
    }

    return JNI_TRUE;
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
 * Method:    close
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaClientConnection_close(JNIEnv *env,
                                                                                                       jobject self) {
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection");
    if (jConnCls == NULL)
        REPORT_ERROR_BOOL("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection.");
    jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
    if (jFieldCxxPtr == NULL)
        REPORT_ERROR_BOOL("Unable to getFieldId `ptrCxxClass`");
    jlong cxxPtr = env->GetLongField(self, jFieldCxxPtr);
    CRdmaClientConnectionInfo *pConn = (CRdmaClientConnectionInfo *)cxxPtr;
    if (pConn == nullptr)
        REPORT_ERROR_BOOL("cxx conn ptr is nullptr. is the connection closed?");
    rdma_debug << "rdmaClientConn.close called at object" << cxxPtr << std::endl;

    checkedDelete(pConn);
    return JNI_TRUE;
}

/////////////////////////////////////////////////////////RdmaServerArea
/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    isClosed
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_isClosed(JNIEnv *env, jobject self) {
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection");
    if (jConnCls == NULL)
        REPORT_ERROR_BOOL("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaClientConnection.");
    jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
    if (jFieldCxxPtr == NULL)
        REPORT_ERROR_BOOL("Unable to getFieldId `ptrCxxClass`");
    jlong cxxPtr = env->GetLongField(self, jFieldCxxPtr);
    CRdmaClientConnectionInfo *pConn = (CRdmaClientConnectionInfo *)cxxPtr;
 
    return pConn == nullptr;
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    getClientIp
 * Signature: ()[B
 */
JNIEXPORT jbyteArray JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_getClientIp(JNIEnv *env,
                                                                                                               jobject self) {
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection");
    if (jConnCls == NULL)
        REPORT_ERROR("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection.");
    jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
    if (jFieldCxxPtr == NULL)
        REPORT_ERROR("Unable to getFieldId `ptrCxxClass`");
    jlong cxxPtr = env->GetLongField(self, jFieldCxxPtr);
    CRdmaServerConnectionInfo *pConn = (CRdmaServerConnectionInfo *)cxxPtr;
    if (pConn == nullptr)
        REPORT_ERROR("cxx conn ptr is nullptr. is the connection closed?");

    static_assert(sizeof(jbyte) == sizeof(char), "jbyte must have the same size with char");
    std::string clientIp = pConn->getClientIp();
    const jbyte *jbyteBuf = (const jbyte *)clientIp.data();
    jbyteArray ret = env->NewByteArray(clientIp.size());
    if (ret == NULL)
        REPORT_ERROR("newByteArray: return null");
    env->SetByteArrayRegion(ret, 0, clientIp.size(), jbyteBuf);
    return ret;
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    isQueryReadable
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_isQueryReadable(JNIEnv *env,
                                                                                                                 jobject self) {
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection");
    if (jConnCls == NULL)
        REPORT_FATAL("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection.");
    jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
    if (jFieldCxxPtr == NULL)
        REPORT_FATAL("Unable to getFieldId `ptrCxxClass`");
    jlong cxxPtr = env->GetLongField(self, jFieldCxxPtr);
    CRdmaServerConnectionInfo *pConn = (CRdmaServerConnectionInfo *)cxxPtr;
    if (pConn == nullptr)
        REPORT_FATAL("cxx conn ptr is nullptr. is the connection closed?");

    try {
        if (pConn->isQueryReadable()) {
            rdma_debug << "serverconn.isqueryreadable successed at obj" << self << std::endl;
            return true;
        } // debug TODO
        else
            return false;
        return pConn->isQueryReadable();
    } catch (std::exception &e) {
        REPORT_FATAL(e.what());
    }
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    readQuery
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_readQuery(JNIEnv *env,
                                                                                                          jobject self) {
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection");
    if (jConnCls == NULL)
        REPORT_ERROR("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection.");
    jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
    if (jFieldCxxPtr == NULL)
        REPORT_ERROR("Unable to getFieldId `ptrCxxClass`");
    jlong cxxPtr = env->GetLongField(self, jFieldCxxPtr);
    CRdmaServerConnectionInfo *pConn = (CRdmaServerConnectionInfo *)cxxPtr;
    if (pConn == nullptr)
        REPORT_ERROR("cxx conn ptr is nullptr. is the connection closed?");
    rdma_debug << "serverconn.readQuery called at obj" << cxxPtr << std::endl;

    try {
        void *dat = nullptr;
        uint64_t size = 0;
        pConn->readQuery(dat, size);
        return env->NewDirectByteBuffer(dat, size);
    } catch (std::exception &e) {
        REPORT_ERROR(e.what());
    }
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    writeResponse
 * Signature: (Ljava/nio/ByteBuffer;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_writeResponse(
    JNIEnv *env, jobject self, jobject jDataBuffer) {
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection");
    if (jConnCls == NULL)
        REPORT_ERROR_BOOL("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection.");
    jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
    if (jFieldCxxPtr == NULL)
        REPORT_ERROR_BOOL("Unable to getFieldId `ptrCxxClass`");
    jlong cxxPtr = env->GetLongField(self, jFieldCxxPtr);
    CRdmaServerConnectionInfo *pConn = (CRdmaServerConnectionInfo *)cxxPtr;
    if (pConn == nullptr)
        REPORT_ERROR_BOOL("cxx conn ptr is nullptr. is the connection closed?");
    rdma_debug << "serverconn.writeResponse called at obj " << cxxPtr << std::endl;

    try {
        void *tmpDataBuf = env->GetDirectBufferAddress(jDataBuffer);
        if (tmpDataBuf == nullptr)
            REPORT_ERROR_BOOL("writeresponse jDataBuffer addr is null");
        pConn->writeResponse(tmpDataBuf, env->GetDirectBufferCapacity(jDataBuffer));
    } catch (std::exception &e) {
        REPORT_ERROR_BOOL(e.what());
    }
    return JNI_TRUE;
}

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaServerConnection
 * Method:    close
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaServerConnection_close(JNIEnv *env,
                                                                                                       jobject self) {
    rdma_debug << "serverconn.close called at obj " << self << std::endl;
    jclass jConnCls = env->FindClass("org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection");
    if (jConnCls == NULL)
        REPORT_ERROR_BOOL("Unable to find class org/apache/hadoop/hbase/ipc/RdmaNative$RdmaServerConnection.");
    jfieldID jFieldCxxPtr = env->GetFieldID(jConnCls, "ptrCxxClass", "J");
    if (jFieldCxxPtr == NULL)
        REPORT_ERROR_BOOL("Unable to getFieldId `ptrCxxClass`");
    jlong cxxPtr = env->GetLongField(self, jFieldCxxPtr);
    CRdmaServerConnectionInfo *pConn = (CRdmaServerConnectionInfo *)cxxPtr;
    if (pConn == nullptr)
        REPORT_ERROR_BOOL("cxx conn ptr is nullptr. is the connection closed?");

    checkedDelete(pConn);
    return JNI_TRUE;
}
