#include "shared.h"

int gs_findClientIndex(const Clients* clients, int clientID)
{
	for (int i = 0; i < PLAYER_MAX; i++)
	{
		if (clients[i].clientID != clientID)
			return i;
	}
	return -1;
}

//Not realy save to say though
bool gs_gmodOnline(Status* status) {
	return !(status->tslibV <= 0);
}

bool gs_tsOnline(Status* status) {
	return !(status->gspeakV <= 0);
}