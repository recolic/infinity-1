/**
 * Examples - Read/Write/Send Operations
 *
 * (c) 2018 Claude Barthels, ETH Zurich
 * Contact: claudeb@inf.ethz.ch
 *
 */

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <cassert>

#include <infinity/core/Context.h>
#include <infinity/queues/QueuePairFactory.h>
#include <infinity/queues/QueuePair.h>
#include <infinity/memory/Buffer.h>
#include <infinity/memory/RegionToken.h>
#include <infinity/requests/RequestToken.h>

#define PORT_NUMBER 8011

int main(int argc, char **argv) {

	bool isServer = argc == 1;
	char *serverIp = NULL;

	if(isServer) {
		printf("./this for server and ./this [ServerIP] for client\n");
	}
	else {
		serverIp = argv[1];
		printf("client\n");
	}

	infinity::core::Context *context = new infinity::core::Context();
	infinity::queues::QueuePairFactory *qpFactory = new  infinity::queues::QueuePairFactory(context);
	infinity::queues::QueuePair *qp;

	if(isServer) {

		printf("Creating buffers to read from and write to\n");
		infinity::memory::Buffer *pRegionTokenBuffer = new infinity::memory::Buffer(context, sizeof(infinity::memory::RegionToken));
		infinity::memory::RegionToken *pRegionTokenBufferToken = pRegionTokenBuffer->createRegionToken();

		printf("Setting up connection (blocking)\n");
		qpFactory->bindToPort(PORT_NUMBER);
		qp = qpFactory->acceptIncomingConnection(pRegionTokenBufferToken, sizeof(infinity::memory::RegionToken));

        printf("Accepted. Now create dynamic memory buffer...");
        infinity::memory::Buffer *pResponseBuffer = new infinity::memory::Buffer(context, 1024);
        infinity::memory::RegionToken *pResponseToken = pResponseBuffer->createRegionToken();
        memcpy(pRegionTokenBuffer->getData(), pResponseToken, sizeof(infinity::memory::RegionToken));
        memcpy(pResponseBuffer->getData(), "Fucking", 8);
        
        printf("sleep 5 while client is reading and responsing...");
        sleep(5);

        printf("pResponseBuffer is now %s", pResponseBuffer->getData());

        delete pResponseBuffer;
        delete pRegionTokenBuffer;

	} else {

		printf("Connecting to remote node\n");
		qp = qpFactory->connectToRemoteHost(serverIp, PORT_NUMBER);
		infinity::memory::RegionToken *remoteBufferToken = (infinity::memory::RegionToken *) qp->getUserData();

		printf("Creating buffers\n");
		infinity::memory::Buffer *pRegionTokenBuffer = new infinity::memory::Buffer(context, sizeof(infinity::memory::RegionToken));

		printf("Sleep 3... Server is creating token.\n");
        sleep(3);
		infinity::requests::RequestToken requestToken(context);
		qp->read(pRegionTokenBuffer, remoteBufferToken, &requestToken);
		requestToken.waitUntilCompleted();

        printf("Token got! Now I'll read the real data.");
        infinity::memory::RegionToken *pRemoteToken = reinterpret_cast<infinity::memory::RegionToken*>(pRegionTokenBuffer->getData());
        int serverBufSize = pRemoteToken->getSizeInBytes();
        infinity::memory::Buffer *pBufferData = new infinity::memory::Buffer(context, serverBufSize);
        qp->read(pBufferData, pRemoteToken, &requestToken);
        requestToken.waitUntilCompleted();

        printf("It seems that I've read the data. it's %s", (char *)pBufferData->getData());
        delete pBufferData;
        delete pRegionTokenBuffer;
	}

	delete qp;
	delete qpFactory;
	delete context;

	return 0;

}
