/*******************************************************************************
*
* BSD 3-Clause License
*
* Copyright (c) 2017, Marc Jakobi
* All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* * Redistributions of source code must retain the above copyright notice, this
*   list of conditions and the following disclaimer.
*
* * Redistributions in binary form must reproduce the above copyright notice,
*   this list of conditions and the following disclaimer in the documentation
*   and/or other materials provided with the distribution.
*
* * Neither the name of the copyright holder nor the names of its
*   contributors may be used to endorse or promote products derived from
*   this software without specific prior written permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
* AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
* IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
* FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
* DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
* SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
* CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
* OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
* OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*
* FORTE License
*
* Copyright (c) 2010-2014 fortiss, TU Wien ACIN and Profactor GmbH.
* All rights reserved. This program and the accompanying materials
* are made available under the terms of the Eclipse Public License v1.0
* which accompanies this distribution, and is available at
* http://www.eclipse.org/legal/epl-v10.html
*
*
********************************************************************************
* IP Com Layer used by the HTTP Com Layer.
* This class is similar to the CIPComLayer, with some changes for performance improvement.
* Opening the connection, sending and receiving data all occurs in the sendData() method.
* This is done because mose HTTP servers have high requirements for speed that currently cannot be
* met by FORTE's standard CIPComLayer.
********************************************************************************/
#include "httpiplayer.h"
#include "../../arch/devlog.h"
#include "commfb.h"
#include "comlayersmanager.h"
#include "../../core/datatypes/forte_dint.h"
#include <stdio.h>
#include <string.h>
#include <time.h>

using namespace forte::com_infra;

CHttpIPComLayer::CHttpIPComLayer(CComLayer* pa_poUpperLayer, CCommFB* pa_poComFB) :
	CComLayer(pa_poUpperLayer, pa_poComFB),
	m_nSocketID(CIPComSocketHandler::scm_nInvalidSocketDescriptor),
	m_nListeningID(CIPComSocketHandler::scm_nInvalidSocketDescriptor),
	m_eInterruptResp(e_Nothing),
	m_unBufFillSize(0) {
}

CHttpIPComLayer::~CHttpIPComLayer() {
}

void CHttpIPComLayer::closeConnection() {
	DEVLOG_DEBUG("CSocketBaseLayer::closeConnection() \n");
	closeSocket(&m_nSocketID);
	closeSocket(&m_nListeningID);

	m_eConnectionState = e_Disconnected;
}

EComResponse CHttpIPComLayer::sendData(void *pa_pvData, unsigned int pa_unSize) {
	EComResponse eRetVal = e_ProcessDataSendFailed;
	if (0 != m_poFb) {
		switch (m_poFb->getComServiceType()) {
		case e_Server:
			// TODO Implement HTTP server behaviour
			// if (0
			//	>= CIPComSocketHandler::sendDataOnTCP(m_nSocketID, static_cast<char*>(pa_pvData), pa_unSize)) {
			//	closeSocket(&m_nSocketID);
			//	m_eConnectionState = e_Listening;
			//	eRetVal = e_InitTerminated;
			//}
			break;
		case e_Client:
		{
			bool withinTimeoutPeriod = true;
			closeConnection(); // In case it is currently open
			char* request = static_cast<char*>(pa_pvData);
			m_unBufFillSize = 0;
			m_acRecvBuffer[0] = 0;
			time_t start; // for timeout
			time_t timer;
			time(&start);
			// Loop runs until the end of the time out period or until the HTTP response has been received completely
			while (withinTimeoutPeriod && (0 == m_acRecvBuffer || m_unBufFillSize <= 0 || 0 == strstr(m_acRecvBuffer, "\r\n\r\n"))) {
				if (e_Connected != m_eConnectionState) {
					m_unBufFillSize = 0;
					m_acRecvBuffer[0] = 0;
					openConnection();
				}
				if (0
					>= CIPComSocketHandler::sendDataOnTCP(m_nSocketID, request, pa_unSize)) {
					eRetVal = e_ProcessDataSendFailed;
					m_eInterruptResp = eRetVal;
				}
				handledConnectedDataRecv();
				if (difftime(time(&timer), start) > kTimeOutS) { // Timeout?
					eRetVal = e_ProcessDataSendFailed;
					m_eInterruptResp = eRetVal;
					withinTimeoutPeriod = false;
				}
			}
			if (e_ProcessDataOk == m_eInterruptResp) {
				eRetVal = m_poTopLayer->recvData(m_acRecvBuffer, m_unBufFillSize);
			}
			closeConnection(); // This sets m_eInterruptedResp to e_InitTerminated
			m_eInterruptResp = eRetVal;
			break;
		}
		case e_Publisher:
		case e_Subscriber:
			// Do nothing as HTTP does not use UDP
			break;
		}
	}
	// Ensure event output is sent (sendData does not trigger e_ProcessDataOk events for clients/servers/subscribers)
	m_poFb->interruptCommFB(this);
	CEventChainExecutionThread *poEventChainExecutor = m_poFb->getEventChainExecutor();
	m_poFb->receiveInputEvent(cg_nExternalEventID, *poEventChainExecutor);
	// Make sure sendData() does not trigger additional INIT- event in case of failure
	return e_Nothing;
}

EComResponse CHttpIPComLayer::processInterrupt() {
	return m_eInterruptResp;
}

EComResponse CHttpIPComLayer::recvData(const void *pa_pvData, unsigned int) {
	// Data is received in the sendData method.
	return e_Nothing;
}

EComResponse CHttpIPComLayer::openConnection(char *pa_acLayerParameter) {
	// For HTTP client: cache parameters and return INIT+ event
	memcpy(mParams, pa_acLayerParameter, strlen(pa_acLayerParameter) + 1);
	return e_InitOk;
}

EComResponse CHttpIPComLayer::openConnection() {
	// Copy params to new char* and perform the same actions as in CIPComLayer
	char pa_acLayerParameter[CHttpComLayer::kAllocSize];
	memcpy(pa_acLayerParameter, mParams, strlen(mParams) + 1);
	EComResponse eRetVal = e_InitInvalidId;
	char *acPort = strchr(pa_acLayerParameter, ':');
	if (0 != acPort) {
		*acPort = '\0';
		++acPort;

		TForteUInt16 nPort = static_cast<TForteUInt16>(forte::core::util::strtoul(acPort, 0, 10));

		CIPComSocketHandler::TSocketDescriptor nSockDes =
			CIPComSocketHandler::scm_nInvalidSocketDescriptor;
		m_eConnectionState = e_Connected;

		switch (m_poFb->getComServiceType()) {
		case e_Server:
			// TODO: HTTP server has not yet been implemented
			//nSockDes = m_nListeningID =
			//	CIPComSocketHandler::openTCPServerConnection(pa_acLayerParameter, nPort);
			//m_eConnectionState = e_Listening;
			break;
		case e_Client:
			nSockDes = m_nSocketID =
				CIPComSocketHandler::openTCPClientConnection(pa_acLayerParameter, nPort);
			break;
		case e_Publisher:
		case e_Subscriber:
			break;
		}

		if (CIPComSocketHandler::scm_nInvalidSocketDescriptor != nSockDes) {
			eRetVal = e_InitOk;
		}
		else {
			m_eConnectionState = e_Disconnected;
		}
	}
	return eRetVal;
}

void CHttpIPComLayer::closeSocket(CIPComSocketHandler::TSocketDescriptor *pa_nSocketID) {
	if (CIPComSocketHandler::scm_nInvalidSocketDescriptor != *pa_nSocketID) {
		CIPComSocketHandler::getInstance().removeComCallback(*pa_nSocketID);
		CIPComSocketHandler::closeSocket(*pa_nSocketID);
		*pa_nSocketID = CIPComSocketHandler::scm_nInvalidSocketDescriptor;
	}
}

void CHttpIPComLayer::handledConnectedDataRecv() {
	// in case of fragmented packets, it can occur that the buffer is full,
	// to avoid calling receiveDataFromTCP with a buffer size of 0 wait until buffer is larger than 0
	while ((cg_unIPLayerRecvBufferSize - m_unBufFillSize) <= 0) {
#ifdef WIN32
		Sleep(0);
#else
		sleep(0);
#endif
	}
	if (CIPComSocketHandler::scm_nInvalidSocketDescriptor != m_nSocketID) {
		// TODO: sync buffer and bufFillSize
		int nRetVal = 0;
		switch (m_poFb->getComServiceType()) {
		case e_Server:
		case e_Client:
			nRetVal =
				CIPComSocketHandler::receiveDataFromTCP(m_nSocketID, &m_acRecvBuffer[m_unBufFillSize], cg_unIPLayerRecvBufferSize
					- m_unBufFillSize);
			break;
		case e_Publisher:
		case e_Subscriber:
			// Do nothing because HTTP does not use UDP
			break;
		}
		switch (nRetVal) {
		case 0:
			DEVLOG_INFO("Connection closed by peer\n");
			m_eInterruptResp = e_InitTerminated;
			closeSocket(&m_nSocketID);
			if (e_Server == m_poFb->getComServiceType()) {
				//Move server into listening mode again
				m_eConnectionState = e_Listening;
			}
			break;
		case -1:
			m_eInterruptResp = e_ProcessDataRecvFaild;
			break;
		default:
			//we successfully received data
			m_unBufFillSize += nRetVal;
			m_eInterruptResp = e_ProcessDataOk;
			break;
			}
		}
}