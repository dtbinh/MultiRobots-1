/*
 * network.cpp
 *
 *  Created on: 24 mars 2013
 *      Author: dom
 */

#include <iostream>
#include <sstream>

using namespace std;

#include "scheduler.h"
#include "network.h"
#include "trace.h"

unsigned int Message::nextId = 0;
unsigned int Message::nbMessages = 0;

unsigned int P2PNetworkInterface::nextId = 0;
double P2PNetworkInterface::defaultDataRate=1000000;
double P2PNetworkInterface::defaultDataRateVariability=0;

//===========================================================================================================
//
//          Message  (class)
//
//===========================================================================================================

Message::Message() {
	id = nextId;
	nextId++;
	nbMessages++;
	MESSAGE_CONSTRUCTOR_INFO();
}

Message::~Message() {
	MESSAGE_DESTRUCTOR_INFO();
	nbMessages--;
}

unsigned int Message::getNbMessages() {
	return(nbMessages);
}

string Message::getMessageName() {
	return("generic message");
}


//===========================================================================================================
//
//          P2PNetworkInterface  (class)
//
//===========================================================================================================

P2PNetworkInterface::P2PNetworkInterface(BaseSimulator::BuildingBlock *b) {
#ifndef NDEBUG
	// OUTPUT << "P2PNetworkInterface constructor" << endl;
#endif
	hostBlock = b;
//	localId = block->getNextP2PInterfaceLocalId();
	connectedInterface = NULL;
	availabilityDate = 0;
	generator = boost::rand48(nextId);
	globalId = nextId;
	nextId++;
//	messageBeingTransmitted.reset();
	dataRate = defaultDataRate;
	dataRateVariability = defaultDataRateVariability;
}

P2PNetworkInterface::~P2PNetworkInterface() {
#ifndef NDEBUG
	OUTPUT << "P2PNetworkInterface destructor" << endl;
#endif
}

bool P2PNetworkInterface::addToOutgoingBuffer(MessagePtr msg) {
	stringstream info;

	if (connectedInterface != NULL) {
		outgoingQueue.push_back(msg);
		if (availabilityDate < BaseSimulator::getScheduler()->now()) availabilityDate = BaseSimulator::getScheduler()->now();
		if (outgoingQueue.size() == 1 && messageBeingTransmitted == NULL) { //TODO
			BaseSimulator::getScheduler()->schedule(new NetworkInterfaceStartTransmittingEvent(availabilityDate,this));
		}
		return(true);
	} else {
		info.str("");
		info << "*** WARNING *** [block " << hostBlock->blockId << ",interface " << globalId <<"] : trying to enqueue a Message but no interface connected";
		BaseSimulator::getScheduler()->trace(info.str());
		return(false);
	}
}

void P2PNetworkInterface::send() {
	MessagePtr msg;
	stringstream info;
	uint64_t transmissionDuration;
	double rate;

	if (!connectedInterface) {
		info << "*** WARNING *** [block " << hostBlock->blockId << ",interface " << globalId <<"] : trying to send a Message but no interface connected";
		BaseSimulator::getScheduler()->trace(info.str());
		return;
	}

	if (outgoingQueue.size()==0) {
		info << "*** ERROR *** [block " << hostBlock->blockId << ",interface " << globalId <<"] : The outgoing buffer of this interface should not be empty !";
		BaseSimulator::getScheduler()->trace(info.str());
		exit(EXIT_FAILURE);
	}

	msg = outgoingQueue.front();
	outgoingQueue.pop_front();

	rate = dataRate - dataRateVariability + (generator()/(double)RAND_MAX) * 2 * dataRateVariability;
	transmissionDuration = (msg->size()*8000000ULL)/rate;
	messageBeingTransmitted = msg;
	messageBeingTransmitted->sourceInterface = this;
	messageBeingTransmitted->destinationInterface = connectedInterface;

	availabilityDate = BaseSimulator::getScheduler()->now()+transmissionDuration;
	//info << "*** sending (interface " << localId << " of block " << hostBlock->blockId << ")";
	//getScheduler()->trace(info.str());

	BaseSimulator::getScheduler()->schedule(new NetworkInterfaceStopTransmittingEvent(BaseSimulator::getScheduler()->now()+transmissionDuration, this));
}

void P2PNetworkInterface::connect(P2PNetworkInterface *ni) {
	// test ajouté par Ben, gestion du cas : connect(NULL)
	if (ni) { // Connection
		if (ni->connectedInterface != this) {
			if (ni->connectedInterface != NULL) {
				OUTPUT << "ERROR : connecting to an already connected P2PNetwork interface" << endl;
				ni->connectedInterface->hostBlock->removeNeighbor(ni->connectedInterface);
				ni->hostBlock->removeNeighbor(ni);
			}
			ni->connectedInterface = this;
			hostBlock->addNeighbor(ni->connectedInterface, ni->hostBlock);
			ni->hostBlock->addNeighbor(ni, ni->connectedInterface->hostBlock);
		}
	} else if (connectedInterface != NULL) {
		// disconnect this interface and the remote one
		hostBlock->removeNeighbor(this);
		connectedInterface->hostBlock->removeNeighbor(connectedInterface);
		connectedInterface->connectedInterface = NULL;
	}
	connectedInterface = ni;
}
