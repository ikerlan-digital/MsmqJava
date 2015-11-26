//
// MsmqQueue.hpp
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
// Time-stamp: <2010-March-27 20:58:48>
//
// ------------------------------------------------------------------
//
// This is an include for the MsmqQueue C++ class.
//
// ------------------------------------------------------------------

class MsmqQueue
{
private:
	QUEUEHANDLE             hQueue;

public:

	HRESULT createQueue(
		char    *szQueuePath,
		char    *szQueueLabel,
		LPWSTR  wszFormatName,
		DWORD   *p_dwFormatNameBufferLength,
		int     isTransactional
		);

	HRESULT deleteQueue(
		char    *szQueuePath
		);

	HRESULT openQueue(
		char    *szMSMQQueuePath,
		int     openmode
		);

	//     HRESULT read(
	//         char    *szMessageBody,
	//         int     iMessageBodySize,
	//         char    *szCorrelationID,
	//         char    *szLabel,
	//         int     timeout,
	//         int     ReadOrPeek
	//                  );

	//                 HRESULT sendMessage(
	//                              char    *szMessageBody,
	//                              int     iMessageBodySize,
	//                              char    *szLabel,
	//                              char    *szCorrelationID,
	//                              int     transactionFlag
	//                              );

	HRESULT receiveBytes(
		BYTE    **ppbMessageBody,
		DWORD   *dwBodyLen,
		WCHAR   *swzMessageLabel,
		BYTE    *pCorrelationId,
		DWORD   dwTimeOut,
		int     ReadOrPeek
		);

	HRESULT sendBytes(
		BYTE    *pbMessageBody,
		DWORD   dwBodyLen,
		WCHAR   *swzMessageLabel,
		BYTE    *pCorrelationId,
		DWORD   dwCorIdLen,
		DWORD   transactionFlag,
		BOOL    fHighPriority
		);

	HRESULT closeQueue(void);

};
