//
// MsmqQueueNativeMethods.cpp
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
// Time-stamp: <2010-March-28 17:00:04>
//
// ------------------------------------------------------------------
//
// This module defines "Native", JNI-compliant methods for the MSMQ
// APIs, to allow Java code to call them via JNI.  Java code can call C
// functions only if they conform to a JNI-specified interface. This
// code provides that interface, wrapping the MSMQ Win32 functions.
//
// ------------------------------------------------------------------
//

#include <stdio.h>
#include <WTypes.h>   // reqd for WinBase.h
#include <WinBase.h>  // for CriticalSection
#include <MqOai.h>
#include <mq.h>

#include "MsmqJava.h"
#include "MsmqQueue.hpp"


#if DEBUG
#define DIAG(...) { printf(__VA_ARGS__); }
#else
#define DIAG(...) { if(FALSE) {}}
#endif


// NB:
//
// The C lib for MSMQ requires that queues are opened with either RECEIVE
// or SEND access, but not both.  Because we want the Java Queue class to
// support both reading and writing, we therefore maintain two queue
// handles for each queue: one for reading and one for writing.  Therefore
// to handle 10 queues, we need to maintain slots for 20 queue handles.
//
// For queue N, the receiving handle is stored in qhandles[N*2],
// and the sending handle is qhandles[N*2+1].
//

#define MAX_QUEUES 32
static  MsmqQueue *qhandles[MAX_QUEUES * 2];
static  int isInited = -1;

// Global variable
static CRITICAL_SECTION CriticalSection;


void InitQueueHandles()
{
	int i;
	for (i = 0; i < MAX_QUEUES; i++) {
		qhandles[i * 2] = NULL;  // receive handle
		qhandles[i * 2 + 1] = NULL; // send handle
	}
}




int GetNextFreeHandleSlot()
{
	int i, selected = -1;
	for (i = 0; (i < MAX_QUEUES) && (selected == -1); i++) {
		if (qhandles[i * 2] == NULL && qhandles[i * 2 + 1] == NULL) {
			selected = i;
		}
	}
	return selected;
}



void FreeSlot(int slot) {
	EnterCriticalSection(&CriticalSection);
	qhandles[slot * 2] = NULL;
	qhandles[slot * 2 + 1] = NULL;
	LeaveCriticalSection(&CriticalSection);
}


int StoreHandles(MsmqQueue * receiver, MsmqQueue * sender) {
	EnterCriticalSection(&CriticalSection);

	int slot = GetNextFreeHandleSlot();
	if (slot >= MAX_QUEUES) slot = -1;
	if (slot != -1) {
		qhandles[slot * 2] = receiver; // receive handle
		qhandles[slot * 2 + 1] = sender; // send handle
	}

	LeaveCriticalSection(&CriticalSection);
	return slot;
}



int GetQueueSlot(JNIEnv *jniEnv, jobject object, HRESULT *p_hr) {
	jclass cls = jniEnv->GetObjectClass(object);
	jfieldID fieldId;
	jint queueSlot = -1;
	fieldId = jniEnv->GetFieldID(cls, "_queueSlot", "I");
	if (fieldId == 0) {
		*p_hr = -5;
		return NULL;
	}

	queueSlot = jniEnv->GetIntField(object, fieldId);
	if (queueSlot >= MAX_QUEUES) {
		*p_hr = -2;
		return NULL;
	}

	return (int)queueSlot;
}


MsmqQueue*  GetQueue(JNIEnv *jniEnv, jobject object, int * pSlot, HRESULT *p_hr, int flavor) {
	if ((flavor != 0) && (flavor != 1)) {
		*p_hr = -7;
		return NULL;
	}
	int slot = GetQueueSlot(jniEnv, object, p_hr);
	if (pSlot != NULL) *pSlot = slot;

	EnterCriticalSection(&CriticalSection);
	MsmqQueue *p = qhandles[slot * 2 + flavor];
	LeaveCriticalSection(&CriticalSection);

	return p;
}



MsmqQueue*  GetSenderQueue(JNIEnv *jniEnv, jobject object, int * pSlot, HRESULT *p_hr) {
	return GetQueue(jniEnv, object, pSlot, p_hr, 1);
}

MsmqQueue*  GetReceiverQueue(JNIEnv *jniEnv, jobject object, int * pSlot, HRESULT *p_hr) {
	return GetQueue(jniEnv, object, pSlot, p_hr, 0);
}




void SetJavaString(JNIEnv * jniEnv, jobject object, char * fieldName, const char * valueToSet)
{
	jclass cls = jniEnv->GetObjectClass(object);

	jstring value = jniEnv->NewStringUTF(valueToSet);
	jfieldID fieldId;
	fieldId = jniEnv->GetFieldID(cls, fieldName, "Ljava/lang/String;");
	if (fieldId != 0)
		jniEnv->SetObjectField(object, fieldId, value);
}


void SetJavaByteArray(JNIEnv * jniEnv, jobject object, char * fieldName, const BYTE * valueToSet, DWORD arrayLength)
{
	jclass cls = jniEnv->GetObjectClass(object);

	//API Ref :jbyteArray NewByteArray(JNIEnv *env, jsize length)
	jbyteArray value = jniEnv->NewByteArray(arrayLength);

	//API Ref :SetByteArrayRegion(JNIEnv *env, jbyteArray array, jsize startelement, jsize length, jbyte *buffer)
	jniEnv->SetByteArrayRegion(value, 0, arrayLength, (jbyte *)valueToSet);

	jfieldID fieldId = jniEnv->GetFieldID(cls, fieldName, "[B"); // get the field ID for the byte[]

	if (fieldId != 0)
		jniEnv->SetObjectField(object, fieldId, value);
}




// static //
JNIEXPORT jint JNICALL Java_ionic_Msmq_Queue_nativeInit
(JNIEnv *jniEnv, jclass clazz)

{
	// to be called once, by static initializer in Java class
	InitializeCriticalSection(&CriticalSection);
	InitQueueHandles();
	return 0;
}





// static //
JNIEXPORT jint JNICALL Java_ionic_Msmq_Queue_nativeCreateQueue
(JNIEnv *jniEnv, jclass clazz, jstring queuePath, jstring queueLabel, jint isTransactional)

{
	HRESULT hr = 0;
	try {
		MsmqQueue   *q = new MsmqQueue();

		const char * szQueuePath = jniEnv->GetStringUTFChars(queuePath, 0);
		const char * szQueueLabel = jniEnv->GetStringUTFChars(queueLabel, 0);
		WCHAR wszFormatName[256]; // MQ_MAX_Q_NAME_LEN ?
		DWORD dwFormatNameBufferLength = 256;

		hr = q->createQueue((char *)szQueuePath,
			(char *)szQueueLabel,
			(LPWSTR)wszFormatName,
			&dwFormatNameBufferLength,
			isTransactional);

		jniEnv->ReleaseStringUTFChars(queuePath, szQueuePath);
		jniEnv->ReleaseStringUTFChars(queueLabel, szQueueLabel);
	}
	catch (...) {
		DIAG("createQueue caught an error..\n");
		jniEnv->ExceptionDescribe();
		jniEnv->ExceptionClear();
		hr = -99;
	}

	fflush(stdout);
	return (jint)hr;
}


// static //
JNIEXPORT jint JNICALL Java_ionic_Msmq_Queue_nativeDeleteQueue
(JNIEnv *jniEnv, jclass clazz, jstring queuePath)

{
	HRESULT hr = 0;
	try {
		MsmqQueue   *q = new MsmqQueue();

		const char * szQueuePath = jniEnv->GetStringUTFChars(queuePath, 0);

		hr = q->deleteQueue((char *)szQueuePath);

		jniEnv->ReleaseStringUTFChars(queuePath, szQueuePath);
	}
	catch (...) {
		DIAG("deleteQueue caught an error..\n");
		jniEnv->ExceptionDescribe();
		jniEnv->ExceptionClear();
		hr = -99;
	}

	fflush(stdout);
	return (jint)hr;
}



/// not a JNI call ///
jint OpenQueueWithAccess
(JNIEnv *jniEnv, jobject object, jstring queuePath, int access)
{
	jclass cls = jniEnv->GetObjectClass(object);
	jfieldID fieldId;
	HRESULT hr;

	try {
		MsmqQueue * sender = NULL;
		MsmqQueue * receiver = NULL;

		const char *szQueuePath = jniEnv->GetStringUTFChars(queuePath, 0);
		DIAG("OpenQueueWithAccess (%s)\n", szQueuePath);

		if (access & MQ_RECEIVE_ACCESS) {
			receiver = new MsmqQueue();
			// dinoch - Wed, 11 May 2005  13:48
			// MQ_ADMIN_ACCESS == use the local, outgoing queue for remote queues. ??
			hr = receiver->openQueue((char *)szQueuePath, MQ_RECEIVE_ACCESS); // | MQ_ADMIN_ACCESS
			if (hr != 0) {
				delete receiver;
				return hr;
			}
		}

		if (access & MQ_SEND_ACCESS) {
			sender = new MsmqQueue();
			// dinoch - Wed, 11 May 2005  13:48
			// MQ_ADMIN_ACCESS == use (local) outgoing queue for remote queues
			hr = sender->openQueue((char *)szQueuePath, MQ_SEND_ACCESS); // | MQ_ADMIN_ACCESS
			if (hr != 0) {
				delete sender;
				if (receiver != NULL) delete receiver;
				return hr;
			}
		}

		jniEnv->ReleaseStringUTFChars(queuePath, szQueuePath);

		int slot = StoreHandles(receiver, sender);
		if (slot == -1) return -2;

		// use JNI to set the _queueSlot field on the caller
		fieldId = jniEnv->GetFieldID(cls, "_queueSlot", "I");
		if (fieldId == 0) return -3;
		jniEnv->SetIntField(object, fieldId, (jint)slot);

	}
	catch (...) {
		DIAG("openQueue : Exception. \n");
		jniEnv->ExceptionDescribe();
		jniEnv->ExceptionClear();
		hr = -99;
	}

	fflush(stdout);

	return (jint)hr;
}


JNIEXPORT jint JNICALL Java_ionic_Msmq_Queue_nativeOpenQueue
(JNIEnv *jniEnv, jobject object, jstring queuePath)
{
	return OpenQueueWithAccess(jniEnv, object, queuePath, MQ_SEND_ACCESS | MQ_RECEIVE_ACCESS);
}


JNIEXPORT jint JNICALL Java_ionic_Msmq_Queue_nativeOpenQueueForSend
(JNIEnv *jniEnv, jobject object, jstring queuePath)
{

	return OpenQueueWithAccess(jniEnv, object, queuePath, MQ_SEND_ACCESS);
}

JNIEXPORT jint JNICALL Java_ionic_Msmq_Queue_nativeOpenQueueForReceive
(JNIEnv *jniEnv, jobject object, jstring queuePath)
{

	return OpenQueueWithAccess(jniEnv, object, queuePath, MQ_RECEIVE_ACCESS);
}






//  void _PrintByteArray(BYTE *b, int offset, int length)
//  {
//      char buf1[128];
//      char buf2[128];
//      char tstr[80];
//      int ix2 = 0;
//      int i;
//      buf1[0]='\0';
//      buf2[0]='\0';
//      strcat_s(buf1, 128, "0000    ");
//
//      for (i = 0; i < length; i++)
//      {
//          int x = offset+i;
//          if (i != 0 && i % 16 == 0)
//          {
//              buf2[ix2]= '\0';
//              printf(buf1);
//              printf("    ");
//              printf(buf2);
//              printf("\n");
//              buf1[0]='\0';
//              buf2[0]='\0'; ix2=0;
//              printf("%04X    ", i);
//          }
//          sprintf_s(tstr, 80, "%02X ", b[x]);
//          strcat_s(buf1, 128, tstr);
//
//          if (b[x] >=32 && b[x] <= 126)
//              buf2[ix2++]= b[x];
//          else
//              buf2[ix2++]= '.';
//      }
//
//      if (ix2 > 0)
//      {
//          char tstr[80];
//          int extra = ((i%16>0)? ((16 - i%16) * 3) : 0) + 4;
//          for (i = 0; i < extra; i++)
//              tstr[i]= ' ';
//          tstr[i]= '\0';
//          buf2[ix2]= '\0';
//          printf(buf1);
//          printf(tstr);
//          printf(buf2);
//          printf("\n");
//      }
//  }





JNIEXPORT jint JNICALL Java_ionic_Msmq_Queue_nativeReceiveBytes
(JNIEnv *jniEnv, jobject object, jobject msg, jint timeout, jint ReadOrPeek)
{
	HRESULT  hr = 0;

	try {
		MsmqQueue *q = GetReceiverQueue(jniEnv, object, NULL, &hr);
		if (hr != 0) return (jint)hr;

		// get message from the Queue
		WCHAR wszMessageLabel[MQ_MAX_MSG_LABEL_LEN] = L"";
		BYTE pCorrelationId[PROPID_M_CORRELATIONID_SIZE];

		// initialize variables
		BYTE* pbMessage = NULL;
		DWORD dwMessageLength = 0;

		hr = q->receiveBytes(&pbMessage,
			&dwMessageLength,
			(WCHAR*)wszMessageLabel,
			pCorrelationId,
			timeout,
			ReadOrPeek);

		if (hr == 0) {
			CHAR szLabel[MQ_MAX_MSG_LABEL_LEN];
			int len = wcslen(wszMessageLabel);
			int rc = 0;

			if (len > 0)
				rc = WideCharToMultiByte(
				(UINT)CP_ACP,             // code page
				(DWORD)0,                 // conversion flags
				(LPCWSTR)wszMessageLabel, // wide-character string to convert
				len,                       // number of chars in string.
				(LPSTR)szLabel,           // buffer for new string
				MQ_MAX_MSG_LABEL_LEN,      // size of buffer
				(LPCSTR)NULL,             // default for unmappable chars
				(LPBOOL)NULL              // set when default char used
				);
			// terminate
			if (rc>0)
				szLabel[rc] = '\0';
			else if (rc<0)
				szLabel[0] = '\0';

			SetJavaByteArray(jniEnv, msg, "_messageBody", pbMessage, dwMessageLength);
			SetJavaString(jniEnv, msg, "_label", (char *)szLabel);
			SetJavaByteArray(jniEnv, msg, "_correlationId", pCorrelationId, PROPID_M_CORRELATIONID_SIZE);
			//SetJavaString(jniEnv, msg, "_correlationId", (char *) wszCorrelationId);
		}

		delete[] pbMessage;

		if (hr != 0)  return hr;
	}
	catch (...) {
		DIAG("Read() : Exception\n");
		jniEnv->ExceptionDescribe();
		jniEnv->ExceptionClear();
		hr = -99;
	}

	fflush(stdout);
	return (jint)hr;
}




JNIEXPORT jint JNICALL Java_ionic_Msmq_Queue_nativeSendBytes
(JNIEnv *jniEnv,
jobject object,
jbyteArray message,
jstring label,
jbyteArray correlationId,
jint transactionFlag,
jboolean fPriorityFlag)
{
	HRESULT hr = 0;
	try {
		MsmqQueue   *q = GetSenderQueue(jniEnv, object, NULL, &hr);
		if (hr != 0) return (jint)hr;

		jsize bodyLen = jniEnv->GetArrayLength(message);
		jbyte *body = jniEnv->GetByteArrayElements(message, 0);
		const char *szLabel = jniEnv->GetStringUTFChars(label, 0);
		jsize corIdLen = 0;
		jbyte *corId = NULL;
		if (correlationId != NULL) {
			corIdLen = jniEnv->GetArrayLength(correlationId);
			corId = jniEnv->GetByteArrayElements(correlationId, 0);
		}
		//const char *szCorrelationId = jniEnv->GetStringUTFChars(correlationId, 0);

		WCHAR wszLabel[MQ_MAX_MSG_LABEL_LEN];
		memset(wszLabel, 0, sizeof(MQ_MAX_MSG_LABEL_LEN));

		int len = strlen(szLabel);
		if (len > 0) {
			int rc = MultiByteToWideChar((UINT)CP_ACP,
				(DWORD)0,
				(LPCSTR)szLabel,
				len,
				(LPWSTR)wszLabel,
				(int) sizeof(wszLabel));
			// terminate
			if (rc>0)
				wszLabel[rc] = L'\0';
			else if (rc<0)
				wszLabel[0] = L'\0';
		}

		hr = q->sendBytes((BYTE *)body,
			bodyLen,
			(WCHAR *)wszLabel,
			(BYTE *)corId,
			corIdLen,
			transactionFlag,
			fPriorityFlag);

		jniEnv->ReleaseByteArrayElements(message, body, 0);
		jniEnv->ReleaseStringUTFChars(label, szLabel);
		//jniEnv->ReleaseStringUTFChars(correlationId, szCorrelationId);
		jniEnv->ReleaseByteArrayElements(correlationId, corId, 0);
	}
	catch (...) {
		jniEnv->ExceptionDescribe();
		jniEnv->ExceptionClear();
		hr = -99;
	}

	fflush(stdout);
	return (jint)hr;
}





JNIEXPORT jint JNICALL Java_ionic_Msmq_Queue_nativeClose
(JNIEnv *jniEnv, jobject object)
{
	HRESULT hr_r = 0;
	HRESULT hr_s = 0;
	HRESULT hr = 0;
	int slot;
	try {
		MsmqQueue *r, *s;
		r = GetReceiverQueue(jniEnv, object, &slot, &hr_r);
		s = GetSenderQueue(jniEnv, object, &slot, &hr_s);
		if ((hr_r == 0) && (r != NULL)){
			hr_r = r->closeQueue();
			delete r;
			if (hr_r != 0) DIAG("Zowie, can't close receiver. (hr=0x%08x)\n", hr_r);
		}

		if ((hr_s == 0) && (s != NULL)) {
			hr_s = s->closeQueue();
			delete s;
			if (hr_s != 0) DIAG("Zowie, can't close sender. (hr=0x%08x)\n", hr_s);
		}

		FreeSlot(slot);

		// we return at most one of the HRESULTs
		if (hr_r != 0) hr = hr_r;
		else if (hr_s != 0) hr = hr_s;

	}
	catch (...) {
		jniEnv->ExceptionDescribe();
		jniEnv->ExceptionClear();
		hr = -99;
	}

	fflush(stdout);
	return (jint)hr;
}