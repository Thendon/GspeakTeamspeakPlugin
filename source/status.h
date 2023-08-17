#pragma once

#include "defines.h"
#include <sstream>

namespace Gspeak
{
	struct Status {
		int clientID;
		bool inChannel;
		char name[NAME_BUF];
		short tslibV;
		short gspeakV;
		short radio_downsampler;
		short radio_distortion;
		float upward[3];
		float forward[3];
		float radio_volume;
		float radio_volume_noise;
		char password[PASS_BUF];
		bool status;
		bool talking;
		int command;

		char channelName[NAME_BUF];
		int channelId;
	};

	std::ostream& operator<<(std::ostream& os, Status const& arg);;
	std::string to_string(Status const& arg);
}