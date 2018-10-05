/* DO NOT EDIT THIS FILE - it is machine generated */
#include <jni.h>
/* Header for class org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection */

#ifndef _Included_org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
#define _Included_org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
#ifdef __cplusplus
extern "C" {
#endif
/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
 * Method:    isClosed
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaClientConnection_isClosed
  (JNIEnv *, jobject);

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
 * Method:    readResponse
 * Signature: ()Ljava/nio/ByteBuffer;
 */
JNIEXPORT jobject JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaClientConnection_readResponse
  (JNIEnv *, jobject);

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
 * Method:    writeQuery
 * Signature: (Ljava/nio/ByteBuffer;)Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaClientConnection_writeQuery
  (JNIEnv *, jobject, jobject);

/*
 * Class:     org_apache_hadoop_hbase_ipc_RdmaNative_RdmaClientConnection
 * Method:    close
 * Signature: ()Z
 */
JNIEXPORT jboolean JNICALL Java_org_apache_hadoop_hbase_ipc_RdmaNative_00024RdmaClientConnection_close
  (JNIEnv *, jobject);

#ifdef __cplusplus
}
#endif
#endif