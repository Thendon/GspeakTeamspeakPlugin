#include "status.h"

namespace Gspeak
{
	std::ostream& operator<<(std::ostream& os, Status const& arg)
	{
		os << "clientID = " << arg.clientID << '\n'
			<< "inChannel = " << arg.inChannel << '\n'
			<< "name = " << arg.name << '\n'
			<< "tslibV = " << arg.tslibV << '\n'
			<< "gspeakV = " << arg.gspeakV << '\n'
			<< "radio_downsampler = " << arg.radioEffect.downsampler << '\n'
			<< "radio_distortion = " << arg.radioEffect.distortion << '\n'
			<< "radio_volume = " << arg.radioEffect.volume << '\n'
			<< "radio_volume_noise = " << arg.radioEffect.noise << '\n'
			<< "upward = " << arg.upward[0] << ' ' << arg.upward[1] << ' ' << arg.upward[2] << '\n'
			<< "forward = " << arg.forward[0] << ' ' << arg.forward[1] << ' ' << arg.forward[2] << '\n'
			//<< "password = " << arg.password << '\n'
			<< "status = " << arg.status << '\n'
			<< "talking = " << arg.talking << '\n'
			<< "command = " << arg.command << '\n';
			//<< "channelName = " << arg.channelName << '\n'
			//<< "channelId = " << arg.channelId;
		return os;
	}

	std::string to_string(Status const& arg)
	{
		std::ostringstream ss;
		ss << arg;
		return std::move(ss).str();  // enable efficiencies in c++17
	}
}