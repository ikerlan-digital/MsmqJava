//
// MsmqQueue.cpp
// ------------------------------------------------------------------
//
// Copyright (c) 2006-2010 Dino Chiesa.
// All rights reserved.
//
// This code module is part of MsmqJava, a JNI library that provides
// access to MSMQ for Java on Windows.
//
// ------------------------------------------------------------------
//
// This code is licensed under the Microsoft Public License.
// See the file License.txt for the license details.
// More info on: http://dotnetzip.codeplex.com
//
// ------------------------------------------------------------------
//
// last saved (in emacs):
// Time-stamp: <2010-March-28 00:26:45>
//
// ------------------------------------------------------------------
//
// This module provides an Object wrapper around the Win32 MSMQ
// functions.
//
// ------------------------------------------------------------------


// See the MQ Functions Ref:
// http://msdn.microsoft.com/library/default.asp?url=/library/en-us/msmq/msmq_ref_functions_4o37.asp

#include <stdio.h>
#include <MqOai.h>
#include <mq.h>

#include "MsmqQueue.hpp"

extern void _PrintByteArray(BYTE *b, int offset, int length);



#if DEBUG
#define DIAG(...) { printf(__VA_ARGS__); }
#else
#define DIAG(...) { if(FALSE) {}}
#endif


HRESULT MsmqQueue::createQueue(char *szQueuePath,
	char *szQueueLabel,
	LPWSTR wszFormatName,
	DWORD * p_dwFormatNameBufferLength,
	int isTransactional)
{
	HRESULT hr = MQ_OK;
	int len;

	// example: http://msdn.microsoft.com/library/default.asp?url=/library/en-us/msmq/msmq_using_createqueue_0f51.asp
	const int NUMBEROFPROPERTIES = 5;

	MQQUEUEPROPS   QueueProps;
	MQPROPVARIANT  aQueuePropVar[NUMBEROFPROPERTIES];
	QUEUEPROPID    aQueuePropId[NUMBEROFPROPERTIES];
	HRESULT        aQueueStatus[NUMBEROFPROPERTIES];
	DWORD          i = 0;

	if (szQueuePath == NULL)
		return MQ_ERROR_INVALID_PARAMETER;

	WCHAR wszPathName[MQ_MAX_Q_NAME_LEN];
	len = strlen(szQueuePath);
	if (MultiByteToWideChar(
		(UINT)CP_ACP,
		(DWORD)0,
		(LPCSTR)szQueuePath,
		len,
		(LPWSTR)wszPathName,
		(int) sizeof(wszPathName)) == 0)
		return MQ_ERROR_INVALID_PARAMETER;

	if (len < sizeof(wszPathName))
		wszPathName[len] = 0; // need this to terminate

	WCHAR wszLabel[MQ_MAX_Q_LABEL_LEN];
	len = strlen(szQueueLabel);
	if (MultiByteToWideChar(
		(UINT)CP_ACP,
		(DWORD)0,
		(LPCSTR)szQueueLabel,
		len,
		(LPWSTR)wszLabel,
		(int) sizeof(wszLabel)) == 0)
	{
		return MQ_ERROR_INVALID_PARAMETER;
	}
	if (len < sizeof(wszLabel))
		wszLabel[len] = 0; // need this to terminate


	DIAG("attempting to create queue with name= '%S', label='%S'\n", wszPathName, wszLabel);

	// Set the PROPID_Q_PATHNAME property with the path name provided.
	aQueuePropId[i] = PROPID_Q_PATHNAME;
	aQueuePropVar[i].vt = VT_LPWSTR;
	aQueuePropVar[i].pwszVal = wszPathName; // wszActualName
	i++;

	// Set optional queue properties. PROPID_Q_TRANSACTIONAL
	// must be set to make the queue transactional.
	aQueuePropId[i] = PROPID_Q_TRANSACTION;
	aQueuePropVar[i].vt = VT_UI1;
	aQueuePropVar[i].bVal = (unsigned char)isTransactional;
	i++;

	aQueuePropId[i] = PROPID_Q_LABEL;
	aQueuePropVar[i].vt = VT_LPWSTR;
	aQueuePropVar[i].pwszVal = wszLabel;
	i++;

	// Initialize the MQQUEUEPROPS structure
	QueueProps.cProp = i;                  //Number of properties
	QueueProps.aPropID = aQueuePropId;     //IDs of the queue properties
	QueueProps.aPropVar = aQueuePropVar;   //Values of the queue properties
	QueueProps.aStatus = aQueueStatus;     //Pointer to return status


	// http://msdn.microsoft.com/library/en-us/msmq/msmq_ref_functions_8dut.asp
	hr = MQCreateQueue(NULL,                // Security descriptor
		&QueueProps,         // Address of queue property structure
		wszFormatName,       // Pointer to format name buffer
		p_dwFormatNameBufferLength);  // Pointer to receive the queue's format name length
	return hr;

};



HRESULT MsmqQueue::deleteQueue(char *szQueuePath)
{
	HRESULT hr = MQ_OK;
	int len;

	// example: http://msdn.microsoft.com/library/default.asp?url=/library/en-us/msmq/msmq_using_createqueue_0f51.asp
	if (szQueuePath == NULL)
		return MQ_ERROR_INVALID_PARAMETER;

	// dinoch - Mon, 18 Apr 2005  16:34
	// removed the prefix.  The caller is now responsible for providing it.
	//CHAR szDestFormatName[MQ_MAX_Q_NAME_LEN] = "DIRECT=OS:";
	//strcat(szDestFormatName, szQueuePath);
	CHAR szDestFormatName[MQ_MAX_Q_NAME_LEN];
	strncpy_s(szDestFormatName, MQ_MAX_Q_NAME_LEN, szQueuePath, MQ_MAX_Q_NAME_LEN);

	WCHAR wszPathName[MQ_MAX_Q_NAME_LEN];
	len = strlen(szDestFormatName);
	if (MultiByteToWideChar(
		(UINT)CP_ACP,
		(DWORD)0,
		(LPCSTR)szDestFormatName,
		len,
		(LPWSTR)wszPathName,
		(int) sizeof(wszPathName)) == 0)
	{
		return MQ_ERROR_INVALID_PARAMETER;
	}

	if (len < sizeof(wszPathName))
		wszPathName[len] = 0; // need this to terminate

	hr = MQDeleteQueue(wszPathName);

	return hr;
};



HRESULT MsmqQueue::openQueue(char *szQueuePath, int openmode)
{
	// DIRECT=OS: says to use the computer name to identify the message queue
	// dinoch - Mon, 18 Apr 2005  16:25
	// removed initialization
	//CHAR szDestFormatName[MQ_MAX_Q_NAME_LEN] = "DIRECT=OS:";
	CHAR  szDestFormatName[MQ_MAX_Q_NAME_LEN];
	WCHAR wszDestFormatName[2 * MQ_MAX_Q_NAME_LEN];
	long  accessmode = openmode;  // bit field: MQ_{RECEIVE,SEND,PEEK,ADMIN}_ACCESS,
	long  sharemode = MQ_DENY_NONE;

	// Validate the input string.
	if (szQueuePath == NULL)
	{
		return MQ_ERROR_INVALID_PARAMETER;
	}


	// dinoch - Mon, 18 Apr 2005  16:24
	// removed.  The caller now needs to add any required prefix.
	// add a prefix of DIRECT=OS: to MSMQ queuepath
	//strcat(szDestFormatName, szQueuePath);

	strncpy_s(szDestFormatName, MQ_MAX_Q_NAME_LEN, szQueuePath, sizeof(szDestFormatName) / sizeof(CHAR));

	// convert to wide characters;
	if (MultiByteToWideChar(
		(UINT)CP_ACP,
		(DWORD)0,
		(LPCSTR)szDestFormatName,
		(int) sizeof(szDestFormatName),
		(LPWSTR)wszDestFormatName,
		(int) sizeof(wszDestFormatName)) == 0)
	{
		return MQ_ERROR_INVALID_PARAMETER;
	}

	HRESULT hr = MQ_OK;

	// dinoch Mon, 18 Apr 2005  16:12
	DIAG("open: ");
	DIAG("fmtname(%ls) ", wszDestFormatName);
	DIAG("accessmode(%d) ", accessmode);
	DIAG("sharemode(%d) ", sharemode);
	DIAG("\n");

	hr = MQOpenQueue(
		wszDestFormatName,           // Format name of the queue
		accessmode,                  // Access mode
		sharemode,                   // Share mode
		&hQueue                      // OUT: Handle to queue
		);

	// Retry to handle AD replication delays.
	//
	if (hr == MQ_ERROR_QUEUE_NOT_FOUND)
	{
		int iCount = 0;
		while ((hr == MQ_ERROR_QUEUE_NOT_FOUND) && (iCount < 120))
		{
			DIAG(".");

			// Wait a bit.
			iCount++;
			Sleep(50);

			// Retry.
			hr = MQOpenQueue(wszDestFormatName,
				accessmode,
				sharemode,
				&hQueue);
		}
	}

	if (FAILED(hr))
	{
		MQCloseQueue(hQueue);
	}

	return hr;
};







HRESULT MsmqQueue::receiveBytes(BYTE  **ppbMessageBody,
	DWORD *dwpBodyLen,
	WCHAR *wszMessageLabel,
	BYTE  *pCorrelationId, // sz PROPID_M_CORRELATIONID_SIZE
	DWORD dwTimeOut,
	int   ReadOrPeek
	)
{
	// for receive message
	const int     NUMBEROFPROPERTIES = 6;
	MQMSGPROPS    MsgProps;
	MQPROPVARIANT fields[NUMBEROFPROPERTIES];
	MSGPROPID     propId[NUMBEROFPROPERTIES];
	DWORD         i = 0;
	DWORD         dwAction = (ReadOrPeek == 1) ? MQ_ACTION_RECEIVE : MQ_ACTION_PEEK_CURRENT;
	HRESULT       hr = S_OK;
	int           iBodyLen = 0;
	int           iBody = 0;
	int           iLabelLen = 0;
	ULONG         ulLabelLen = MQ_MAX_MSG_LABEL_LEN;

	// initialize all out variables to NULL
	if (NULL != wszMessageLabel)
		*wszMessageLabel = L'\0';
	if (NULL != pCorrelationId)
		memset(pCorrelationId, 0, PROPID_M_CORRELATIONID_SIZE);

	int MAX_INITIAL_BODY_SIZE = 1024;
	*ppbMessageBody = new BYTE[MAX_INITIAL_BODY_SIZE];

	// prepare the property array PROPVARIANT of
	// message properties that we want to receive
	propId[i] = PROPID_M_BODY_SIZE;
	fields[i].vt = VT_UI4;
	fields[i].ulVal = *dwpBodyLen;
	iBodyLen = i;
	i++;

	propId[i] = PROPID_M_BODY;
	fields[i].vt = VT_VECTOR | VT_UI1;
	fields[i].caub.cElems = *dwpBodyLen;
	fields[i].caub.pElems = (unsigned char *)*ppbMessageBody;
	iBody = i;
	i++;

	if (NULL != pCorrelationId)
	{
		propId[i] = PROPID_M_CORRELATIONID;
		fields[i].vt = VT_VECTOR | VT_UI1;
		fields[i].caub.pElems = (LPBYTE)pCorrelationId;
		fields[i].caub.cElems = PROPID_M_CORRELATIONID_SIZE;
		i++;
	}

	propId[i] = PROPID_M_LABEL_LEN;
	fields[i].vt = VT_UI4;
	fields[i].ulVal = ulLabelLen;
	iLabelLen = i;
	i++;

	propId[i] = PROPID_M_LABEL;
	fields[i].vt = VT_LPWSTR;
	fields[i].pwszVal = wszMessageLabel;
	i++;

	// Set the MQMSGPROPS structure
	MsgProps.cProp = i;            // Number of properties.
	MsgProps.aPropID = propId;      // Id of properties.
	MsgProps.aPropVar = fields; // Value of properties.
	MsgProps.aStatus = NULL;         // No Error report.

	hr = MQReceiveMessage(
		hQueue,              // handle to the Queue.
		dwTimeOut,           // Max time (msec) to wait for the message.
		dwAction,            // Action.
		&MsgProps,           // properties to retrieve.
		NULL,                // No overlaped structure.
		NULL,                // No callback function.
		NULL,                // No Cursor.
		NULL                 // transaction
		);

	// handle the case where the buffer is too small
	do
	{
		if (MQ_ERROR_BUFFER_OVERFLOW == hr)
		{
			if (NULL != *ppbMessageBody)
			{
				delete[] * ppbMessageBody;
				*ppbMessageBody = NULL;
			}
			INT iNewMsgLen = fields[iBodyLen].ulVal;
			*ppbMessageBody = new BYTE[iNewMsgLen];

			fields[iBody].caub.cElems =
				fields[iBodyLen].ulVal;
			fields[iBody].caub.pElems =
				(unsigned char *)*ppbMessageBody;

			hr = MQReceiveMessage(
				hQueue,              // handle to the Queue.
				dwTimeOut,           // Max time (msec) to wait for the message.
				dwAction,            // Action.
				&MsgProps,           // properties to retrieve.
				NULL,                // No overlapped structure.
				NULL,                // No callback function.
				NULL,                // No Cursor.
				NULL                 // transaction
				);
		}

		if (hr == MQ_ERROR_LABEL_BUFFER_TOO_SMALL)
		{
			fields[iLabelLen].ulVal = ulLabelLen;
			fields[iLabelLen + 1].pwszVal = wszMessageLabel;

			hr = MQReceiveMessage(
				hQueue,              // handle to the Queue.
				dwTimeOut,           // Max time (msec) to wait for the message.
				dwAction,            // Action.
				&MsgProps,           // properties to retrieve.
				NULL,                // No overlapped structure.
				NULL,                // No callback function.
				NULL,                // No Cursor.
				NULL                 // transaction
				);
		}
	} while (MQ_ERROR_BUFFER_OVERFLOW == hr);

	if (FAILED(hr))
	{
		delete[] * ppbMessageBody;
		*ppbMessageBody = NULL;
		return hr;
	}

	//     printf("receive: hr 0x%08x\n", hr );
	//     printf("       : body: %d bytes (0x%x)\n", fields[iBodyLen].ulVal * 2,
	//            fields[iBodyLen].ulVal * 2);
	//     _PrintByteArray((BYTE*)*ppbMessageBody, 0, fields[iBodyLen].ulVal * 2);
	//     printf("       : label: %d bytes (0x%X)\n", fields[iLabelLen].ulVal * 2,
	//            fields[iLabelLen].ulVal * 2);
	//     _PrintByteArray((BYTE*)wszMessageLabel, 0, fields[iLabelLen].ulVal * 2);

	*dwpBodyLen = fields[iBodyLen].ulVal;

	if (0 == *dwpBodyLen)
	{
		delete[] * ppbMessageBody;
		*ppbMessageBody = NULL;
	}

	ulLabelLen = fields[iLabelLen].ulVal;

	return hr;
};





HRESULT MsmqQueue::sendBytes(BYTE    *pbMessageBody,
	DWORD   dwBodyLen,
	WCHAR   *wszMessageLabel,
	BYTE    *pCorrelationId,
	DWORD   dwCorIdLen,
	DWORD   transactionFlag,
	BOOL    fHighPriority
	)
{
	const int     MAX_NUM_PROPERTIES = 4;    // Max. Number of properties for send message
	MQMSGPROPS    MsgProps;
	MSGPROPID     propId[MAX_NUM_PROPERTIES];
	MQPROPVARIANT aPropVariant[MAX_NUM_PROPERTIES];
	DWORD         i = 0;
	HRESULT       hr = S_OK;
	BYTE          corId[PROPID_M_CORRELATIONID_SIZE];

	memset(corId, 0, PROPID_M_CORRELATIONID_SIZE);

	if (dwCorIdLen > 0) {
		// Copy correlationId truncating to MSMQ max corId size of 20.
		// We don't do any translation of this field.
		if (dwCorIdLen > PROPID_M_CORRELATIONID_SIZE) dwCorIdLen = PROPID_M_CORRELATIONID_SIZE;
		memcpy(corId, pCorrelationId, dwCorIdLen);
	}

	if (NULL != pbMessageBody)
	{
		// Set the PROPID_M_BODY property.
		propId[i] = PROPID_M_BODY;
		aPropVariant[i].vt = VT_VECTOR | VT_UI1;
		aPropVariant[i].caub.cElems = dwBodyLen;
		aPropVariant[i].caub.pElems = const_cast<BYTE *>(pbMessageBody);
		i++;
	}

	propId[i] = PROPID_M_CORRELATIONID;
	aPropVariant[i].vt = VT_VECTOR | VT_UI1;
	aPropVariant[i].caub.pElems = (LPBYTE)corId;
	aPropVariant[i].caub.cElems = PROPID_M_CORRELATIONID_SIZE;
	i++;

	propId[i] = PROPID_M_LABEL;
	aPropVariant[i].vt = VT_LPWSTR;
	aPropVariant[i].pwszVal = wszMessageLabel;
	i++;

	if (fHighPriority)
	{
		// Set the PROPID_M_PRIORITY property.
		propId[i] = PROPID_M_PRIORITY;
		aPropVariant[i].vt = VT_UI1;
		aPropVariant[i].bVal = 7;       // Highest priority
		i++;
	}

	// Set the MQMSGPROPS structure
	MsgProps.cProp = i;  // Number of properties.
	MsgProps.aPropID = propId;      // Id of properties.
	MsgProps.aPropVar = aPropVariant; // Value of properties.
	MsgProps.aStatus = NULL;         // No Error report.

	hr = MQSendMessage(hQueue,          // handle to the Queue.
		&MsgProps,       // Message properties to be sent.
		(ITransaction *)transactionFlag
		);
	return hr;
};



HRESULT MsmqQueue::closeQueue()
{
	HRESULT hr = MQ_OK;
	hr = MQCloseQueue(hQueue);
	return hr;
};