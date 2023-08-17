#include "client.h"

namespace Gspeak
{
	std::ostream& operator<<(std::ostream& os, Client const& arg)
	{
		os << "clientID = " << arg.clientID <<
			", pos = " << arg.pos[0] << ' ' << arg.pos[0] << ' ' << arg.pos[0] <<
			", volume_gm = " << arg.volume_gm <<
			", volume_ts = " << arg.volume_ts <<
			", effect = " << arg.effect <<
			", talking = " << arg.talking;
		return os;
	}

	std::string to_string(Client const& arg)
	{
		std::ostringstream ss;
		ss << arg;
		return std::move(ss).str();  // enable efficiencies in c++17
	}
}