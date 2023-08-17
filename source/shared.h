#pragma once

#include <Windows.h>
#include "status.h"
#include "client.h"

namespace Gspeak
{
	enum HMAP_RESULT {
		SUCCESS,
		ACCESS_DENIED,
		CREATION_FAILED,
		VIEW_FAILED,
		UNEXPECTED_ERROR
	};

	class Shared {
	public:
		static Client* clients();
		static Status* status();

		static int findClientIndex(int clientID);
		//unused
		static bool gmodOnline(Status* status);
		//unused
		static bool tsOnline(Status* status);

		static HMAP_RESULT openStatus();
		static HMAP_RESULT openClients();
		static void closeStatus();
		static void closeClients();
	private:
		static const TCHAR clientName[];
		static const TCHAR statusName[];

		static HANDLE hMapFileClients;
		static HANDLE hMapFileStatus;

		static LPVOID clientView;
		static LPVOID statusView;

		static void closeMap(HANDLE* hMapFile, LPVOID* view);
		static HMAP_RESULT openOrCreateMap(const TCHAR* name, HANDLE* hMapFile, LPVOID* view, unsigned int buf_size);
	};
}
