﻿#include "Header.h"
#include "Telegram.h"
#include "Global.h"
#include "ILogger.h"


void Session::ProcessRevoke(HistoryMessage* pMessage)
{
	Safe::TryExcept([&]()
	{
		if (!pMessage->IsMessage()) {
			return;
		}

		QtString *pTimeText = pMessage->GetTimeText();
		if (!pTimeText->IsValidTime()) {
			ILogger::GetInstance().TraceWarn("A bad TimeText. Address: [" + Text::Format("0x%x", pMessage) + "]");
			return;
		}

		std::lock_guard<std::mutex> Lock(g::Mutex);
		g::RevokedMessages.insert(pMessage);

		// g::Logger.TraceInfo("Successful cache. Address: [" + Text::FormatA("0x%x", pMessage) + "]");

	}, [](ULONG ExceptionCode)
	{
		ILogger::GetInstance().TraceWarn("Function: [" __FUNCTION__ "] An exception was caught. Code: [" + Text::Format("0x%x", ExceptionCode) + "]");
	});
}

void ProcessItems()
{
	while (true)
	{
		Sleep(1000);

		std::lock_guard<std::mutex> Lock(g::Mutex);

		for (HistoryMessage *pMessage : g::RevokedMessages)
		{
			Safe::TryExcept([&]()
			{
				QtString *pTimeText = NULL;
				HistoryMessageEdited *pEdited = pMessage->GetEdited();
				HistoryMessageSigned *pSigned = pMessage->GetSigned();

				// Signed msg take precedence over Edited msg, and TG uses the Signed text when both exist.
				if (pSigned != NULL) {
					// Signed msg
					pTimeText = pSigned->GetTimeText();
				}
				else if (pEdited != NULL) {
					// Edited msg
					// The edited message time string is not in Item, but is managed by EditedComponent
					pTimeText = pEdited->GetTimeText();
				}
				else {
					// Normal msg
					pTimeText = pMessage->GetTimeText();
				}

				//  vvvvvvvvvvvvvvvvvvvv TODO: This is temp code, hook HistoryMessage's destructor function to improve.
				if (pTimeText == NULL || pTimeText->IsEmpty() || pTimeText->Find(g::CurrentMark.Content) != wstring::npos) {
					// [Empty] This message isn't the current channel or group. 
					// [Found] This message is marked.
					return;
				}

				// Mark deleted
				wstring MarkedTime;

				if (pSigned != NULL)
				{
					// Signed msg text: "<author>, <time>" ("xxx, 10:20")
					//
					wstring OriginalString = pTimeText->GetText();
					size_t Pos = OriginalString.rfind(L", ");
					if (Pos == wstring::npos) {
						return;
					}

					MarkedTime = OriginalString.substr(0, Pos + 2) + g::CurrentMark.Content + OriginalString.substr(Pos + 2);
				}
				else {
					MarkedTime = g::CurrentMark.Content + pTimeText->GetText();
				}

				pTimeText->Replace(MarkedTime.c_str());

				// Modify width
				HistoryViewElement *pMainView = pMessage->GetMainView();
				if (pMainView == NULL) {
					return;
				}
				pMainView->SetWidth(pMainView->GetWidth() + g::CurrentMark.Width);
				pMessage->SetTimeWidth(pMessage->GetTimeWidth() + g::CurrentMark.Width);

				if (!pMessage->IsReply() || pMessage->IsReply() && pMessage->GetHistory()->GetPeer()->IsChannel())
				{
					Media *pMainViewMedia = pMainView->GetMedia();
					if (pMainViewMedia != NULL) {
						pMainViewMedia->SetWidth(pMainViewMedia->GetWidth() + g::CurrentMark.Width);
					}
				}

			}, [&](ULONG ExceptionCode)
			{
				ILogger::GetInstance().TraceWarn("Function: [" __FUNCTION__ "] An exception was caught. Code: [" + Text::Format("0x%x", ExceptionCode) + "] Address: [" + Text::Format("0x%x", pMessage) + "]");
			});
		}
	}
}

void __cdecl DetourFree(void *block)
{
	std::unique_lock<std::mutex> Lock(g::Mutex);

	// When we delete a msg by ourselves, Telegram will free this memory block.
	// So, we will earse this msg from the vector.

	g::RevokedMessages.erase((HistoryMessage*)block);
	Lock.unlock();

	g::fnOriginalFree(block);
}