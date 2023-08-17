#pragma once

#include <sstream>

namespace Gspeak
{
	struct Client {
		int clientID;
		float pos[3];
		float volume_gm;
		float volume_ts;
		bool radio;
		bool talking;
	};

	std::ostream& operator<<(std::ostream& os, Client const& arg);
	std::string to_string(Client const& arg);
}