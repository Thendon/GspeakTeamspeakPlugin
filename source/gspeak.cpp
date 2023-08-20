
#ifdef _WIN32
#pragma warning (disable : 4100)  /* Disable Unreferenced parameter warning */
#include <Windows.h>
#endif

#include <stdio.h>
#include <thread>
#include <ts3_functions.h>
#include <teamspeak/public_errors.h>
#include <teamspeak/public_errors_rare.h>
#include <teamspeak/public_definitions.h>
#include <teamspeak/public_rare_definitions.h>
#include "gspeak.h"
#include "shared.h"
#include <list>
#include <iostream>

static struct TS3Functions ts3Functions;

#ifdef _WIN32
#define _strcpy(dest, destSize, src) strcpy_s(dest, destSize, src)
#define snprintf sprintf_s
#else
#define _strcpy(dest, destSize, src) { strncpy(dest, src, destSize-1); (dest)[destSize-1] = '\0'; }
#endif
#define PLUGIN_API_VERSION 26

#define PATH_BUFSIZE 512
#define COMMAND_BUFSIZE 128
#define INFODATA_BUFSIZE 128
#define SERVERINFO_BUFSIZE 256
#define CHANNELINFO_BUFSIZE 512
#define RETURNCODE_BUFSIZE 128
#define CHANNELNAME_BUFSIZE 128

#define GSPEAK_VERSION 3000
#define SCAN_SPEED 100
#define VOLUME_MAX 1800
#define SHORT_SIZE 32767

static char* pluginID = NULL;

#ifdef _WIN32
static int wcharToUtf8(const wchar_t* str, char** result)
{
	int outlen = WideCharToMultiByte(CP_UTF8, 0, str, -1, 0, 0, 0, 0);
	*result = (char*)malloc(outlen);
	if (WideCharToMultiByte(CP_UTF8, 0, str, -1, *result, outlen, 0, 0) == 0)
	{
		*result = NULL;
		return -1;
	}
	return 0;
}
#endif

enum SampleRate
{
	_8kHz = 8000,
	_16kHz = 16000,
	_32kHz = 32000,
	_48kHz = 48000
};

bool statusThreadActive;
bool statusThreadBreak;
bool clientThreadActive;
bool clientThreadBreak;

char gspeakChannelName[CHANNELNAME_BUFSIZE];
SampleRate gspeakChannelSampleRate = SampleRate::_48kHz;

using namespace std;
using namespace Gspeak;

//*************************************
// REQUIRED TEAMSPEAK3 FUNCTIONS
//*************************************

const char* ts3plugin_name()
{
#ifdef _WIN32
	static char* result = NULL;
	if (!result)
	{
		const wchar_t* name = L"Gspeak3";
		if (wcharToUtf8(name, &result) == -1)
		{
			result = (char*)name;
		}
	}
	return result;
#else
	return "Gspeak3";
#endif
}

const char* ts3plugin_version()
{
	return "3.0";
}

int ts3plugin_apiVersion()
{
	return PLUGIN_API_VERSION;
}

const char* ts3plugin_author()
{
	return "Thendon.exe";
}

const char* ts3plugin_description()
{
	return "This plugin connects Garry's Mod with Teamspeak3";
}

void ts3plugin_setFunctionPointers(const struct TS3Functions funcs)
{
	ts3Functions = funcs;
}

int ts3plugin_init()
{
	std::cout << "[Gspeak] init" << std::endl;

	HMAP_RESULT result = Shared::openStatus();

	if (result != HMAP_RESULT::SUCCESS)
	{
		int err = GetLastError();
		gs_criticalError(err);
		std::cout << "[Gspeak] open status map failed " << (int)result << std::endl;
		return 1;
	}

	Shared::status()->gspeakV = GSPEAK_VERSION;
	Shared::status()->command = Command::Clear;
	Shared::status()->clientID = 0;
	Shared::status()->inChannel = false;

	//check if those will be overriden when starting gmod before activating the addon
	Shared::status()->radioEffect.downsampler = 4;
	Shared::status()->radioEffect.distortion = 1500;
	Shared::status()->radioEffect.volume = 1.5f;
	Shared::status()->radioEffect.noise = 0.01f;

	//Check for Gspeak Channel
	uint64 serverID = ts3Functions.getCurrentServerConnectionHandlerID();
	anyID clientID;
	if (ts3Functions.getClientID(serverID, &clientID) == ERROR_ok)
	{
		uint64 channelID;
		if (ts3Functions.getChannelOfClient(serverID, clientID, &channelID) == ERROR_ok)
		{
			if (gs_isGspeakChannel(serverID, channelID))
			{
				gs_setActive(serverID, channelID);
				return 0;
			}
		}
	}

	gs_setIdle();
	return 0;
}

void ts3plugin_shutdown()
{
	std::cout <<  "[Gspeak] shutdown" << std::endl;

	thread wait(gs_shutdown);
	wait.join();

	if (pluginID)
	{
		free(pluginID);
		pluginID = NULL;
	}
}

//*************************************
// GSPEAK FUNCTIONS
//*************************************

void gs_initClients(uint64 serverConnectionHandlerID, uint64 channelID)
{
	clientThreadBreak = false;
	thread(gs_clientThread, serverConnectionHandlerID, channelID).detach();
}

void gs_initStatus()
{
	statusThreadBreak = false;
	thread(gs_statusThread).detach();
}

void gs_shutClients()
{
	clientThreadBreak = true;
}

void gs_shutStatus()
{
	statusThreadBreak = true;
}

void gs_setIdle()
{
	gs_shutClients();
	gs_initStatus();
}

void gs_setActive(uint64 serverConnectionHandlerID, uint64 channelID)
{
	gs_shutStatus();
	gs_initClients(serverConnectionHandlerID, channelID);
}

void gs_shutdown()
{
	Shared::status()->gspeakV = 0;

	if (clientThreadActive)
		gs_shutClients();
	if (statusThreadActive)
		gs_shutStatus();
	while (true)
	{
		if (!clientThreadActive && !statusThreadActive)
		{
			Shared::closeStatus();
			break;
		}
		this_thread::sleep_for(chrono::milliseconds(SCAN_SPEED));
	}
}

void gs_criticalError(int errorCode)
{
	const char* msg = "[Gspeak] critical error - Code: " + errorCode;
	ts3Functions.printMessageToCurrentTab(msg);
	std::cout << msg << std::endl;
}

bool gs_openMapFile(HANDLE* hMapFile, TCHAR* name, unsigned int buf_size)
{
	*hMapFile = OpenFileMapping(FILE_MAP_ALL_ACCESS, FALSE, name);
	if (*hMapFile == NULL)
	{
		int code = GetLastError();
		std::cout <<  "[Gspeak] error code " << code << std::endl;
		if (code == 5)
		{
			ts3Functions.printMessageToCurrentTab("[Gspeak] access denied - restart Teamspeak3 as Administrator!");
			return false;
		}
		else if (code == 2)
		{
			*hMapFile = CreateFileMapping(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, buf_size, name);
			if (*hMapFile == NULL)
			{
				gs_criticalError(GetLastError());
				return false;
			}
		}
		else
		{
			gs_criticalError(GetLastError());
			return false;
		}
	}
	return true;
}

bool gs_getSampleRate(uint64 serverConnectionHandlerID, uint64 channelID, SampleRate* sampleRate)
{
	int codec;
	if (ts3Functions.getChannelVariableAsInt(serverConnectionHandlerID, channelID, CHANNEL_CODEC, &codec) != ERROR_ok)
		return false;

	switch ((CodecType)codec)
	{
	case CODEC_SPEEX_NARROWBAND:
		*sampleRate = SampleRate::_8kHz;
		return true;
	case CODEC_SPEEX_WIDEBAND:
		*sampleRate = SampleRate::_16kHz;
		return true;
	case CODEC_SPEEX_ULTRAWIDEBAND:
		*sampleRate = SampleRate::_32kHz;
		return true;
	case CODEC_CELT_MONO:
	case CODEC_OPUS_VOICE:
	case CODEC_OPUS_MUSIC:
		*sampleRate = SampleRate::_48kHz;
		return true;
	default:
		return false;
	}
}

void gs_clientMoved(uint64 serverConnectionHandlerID, anyID clientID, uint64 channelID)
{
	anyID localClientID;
	if (ts3Functions.getClientID(serverConnectionHandlerID, &localClientID) != ERROR_ok)
		return;
	uint64 localChannelID;
	if (ts3Functions.getChannelOfClient(serverConnectionHandlerID, localClientID, &localChannelID) != ERROR_ok)
		return;
	if (localChannelID == 0 || channelID == 0)
		return; //Leaving Server or closing Teamspeak

	if (localClientID == clientID)
	{
		if (gs_isGspeakChannel(serverConnectionHandlerID, channelID))
			gs_setActive(serverConnectionHandlerID, channelID);
		else if (clientThreadActive)
			gs_setIdle();
	}
}

bool gs_isDefaultChannel(uint64 serverConnectionHandlerID, uint64 channelID)
{
	int defaultFlag;
	if (ts3Functions.getChannelVariableAsInt(serverConnectionHandlerID, channelID, CHANNEL_FLAG_DEFAULT, &defaultFlag) != ERROR_ok)
		return false;

	return defaultFlag == 1;
}

bool gs_isGspeakChannel(uint64 serverConnectionHandlerID, uint64 channelID)
{
	/*if (channelID == Shared::status()->channelId)
		return true;*/

	char* chname;
	if (ts3Functions.getChannelVariableAsString(serverConnectionHandlerID, channelID, CHANNEL_NAME, &chname) != ERROR_ok)
		return false;

	if (strcmp(chname, gspeakChannelName) == 0)
		return true;

	std::string chnameString(chname);
	if (chnameString.find("Gspeak") != std::string::npos || chnameString.find("GSpeak") != std::string::npos)
		return true;

	return false;
}

void gs_scanClients(uint64 serverConnectionHandlerID)
{
	TS3_VECTOR position;
	for (int i = 0; i < PLAYER_MAX; i++)
	{
		if (Shared::clients()[i].clientID == 0)
			continue;

		position.x = Shared::clients()[i].pos[0];
		position.y = Shared::clients()[i].pos[1];
		position.z = Shared::clients()[i].pos[2];
		ts3Functions.channelset3DAttributes(serverConnectionHandlerID, Shared::clients()[i].clientID, &position);
	}
}

void split(std::string str, char seperator, std::vector<std::string>& strings)
{
	int i = 0;
	int startIndex = 0, endIndex = 0;
	while (i <= str.length())
	{
		if (str[i] == seperator || i == str.length())
		{
			endIndex = i;
			std::string subStr = "";
			subStr.append(str, startIndex, endIndex - startIndex);
			strings.push_back(subStr);
			startIndex = endIndex + 1;
		}
		i++;
	}
}

bool validateParameterCount(const std::vector<std::string>& ins, int required)
{
	if (ins.size() != required)
	{
		std::cout << "[Gspeak] invalid parameter count " << ins.size() << "/" << required << std::endl;
		return false;
	}

	return true;
}

//bool validateParameterCount(const std::vector<const char*>& ins, int min, int max)
//{
//	if (ins.size() < min || ins.size() > max)
//	{
//		std::cout << "[Gspeak] invalid parameter count " << ins.size() << "(" << min << "-" << max << ")" << std::endl;
//		return false;
//	}
//
//	return true;
//}

void gs_cmdCheck(uint64 serverConnectionHandlerID, anyID clientID)
{
	if (Shared::status()->command <= Command::Clear)
		return;

	bool success = false;

	std::vector<std::string> args;
	split(Shared::status()->commandArgs, ';', args);

	switch (Shared::status()->command)
	{
	case Command::Rename:
		success = gs_setNameCommand(serverConnectionHandlerID, clientID, args);
		break;
	case Command::ForceMove:
		success = gs_moveChannelCommand(serverConnectionHandlerID, clientID, args);
		break;
	case Command::ForceKick:
		success = gs_moveDefaultChannel(serverConnectionHandlerID, clientID);
		break;
	//case 123:
		//get client unique identity somehow 
		//ts3Functions.startConnection startConnection(serverConnectionHandlerID, )
		//break;
	}

	Shared::status()->command = success ? Command::Success : Command::Failure;
}

/*
void gs_kickClient(uint64 serverConnectionHandlerID, anyID clientID) {
	ts3Functions.requestClientKickFromChannel(serverConnectionHandlerID, clientID, "Gspeak Kick Command", NULL);
}
*/

bool gs_setNameCommand(uint64 serverConnectionHandlerID, anyID clientID, const std::vector<std::string>& args)
{
	if (!Shared::status()->inChannel)
		return false;

	if (!validateParameterCount(args, 1))
		return false;
	const char* name = args[0].c_str();

	gs_updateStatusName(serverConnectionHandlerID, clientID);

	if (strlen(name) == 0)
		return true;
	if (strcmp(Shared::status()->name, name) == 0)
		return true;

	if (ts3Functions.setClientSelfVariableAsString(serverConnectionHandlerID, CLIENT_NICKNAME, name) != ERROR_ok)
		return false;

	ts3Functions.flushClientSelfUpdates(serverConnectionHandlerID, NULL);
	return true;
}

bool gs_moveDefaultChannel(uint64 serverConnectionHandlerID, anyID clientID)
{
	if (!Shared::status()->inChannel)
		return false;

	uint64* channels;
	if (ts3Functions.getChannelList(serverConnectionHandlerID, &channels) != ERROR_ok)
		return false;

	uint64 localChannelID;
	if (ts3Functions.getChannelOfClient(serverConnectionHandlerID, clientID, &localChannelID) != ERROR_ok)
		return false;

	if (gs_isDefaultChannel(serverConnectionHandlerID, localChannelID))
		return true;

	for (int i = 0; channels[i]; i++)
	{
		if (channels[i] == localChannelID)
			continue;

		if (!gs_isDefaultChannel(serverConnectionHandlerID, channels[i]))
			continue;

		ts3Functions.requestClientMove(serverConnectionHandlerID, clientID, channels[i], "", NULL);
		return true;
	}

	return false;
}

bool gs_moveChannelCommand(uint64 serverConnectionHandlerID, anyID clientID, const std::vector<std::string>& args)
{
	if (!validateParameterCount(args, 2))
		return false;

	ts3Functions.printMessageToCurrentTab("[Gspeak] try switching into Gspeak channel");

	const char* password = args[0].c_str();
	strcpy_s(gspeakChannelName, CHANNELNAME_BUFSIZE * sizeof(char), args[1].c_str());

	uint64* channels;
	if (ts3Functions.getChannelList(serverConnectionHandlerID, &channels) != ERROR_ok)
		return false;

	uint64 localChannelID;
	if (ts3Functions.getChannelOfClient(serverConnectionHandlerID, clientID, &localChannelID) != ERROR_ok)
		return false;

	if (gs_isGspeakChannel(serverConnectionHandlerID, localChannelID))
		return true;

	for (int i = 0; channels[i]; i++)
	{
		if (channels[i] == localChannelID)
			continue;

		if (gs_isGspeakChannel(serverConnectionHandlerID, channels[i]))
		{
			//if (ts3Functions.requestClientMove(serverConnectionHandlerID, clientID, channels[i], Shared::status()->password, NULL) == ERROR_ok)

			//if we didnt succeed in joining the found channel, something is wrong (e.g server config, wrong server) and we should stop trying anyways
			ts3Functions.requestClientMove(serverConnectionHandlerID, clientID, channels[i], password, NULL);
			return true;
		}
	}

	return false;
}

//just to refresh current name in status struct to the actual name in teamspeak
void gs_updateStatusName(uint64 serverConnectionHandlerID, anyID clientID, char* clientName)
{
	if (!gs_isMe(serverConnectionHandlerID, clientID))
		return;
	if (clientName == NULL)
		ts3Functions.getClientVariableAsString(serverConnectionHandlerID, clientID, CLIENT_NICKNAME, &clientName);

	//causing crashes for special characters
	strcpy_s(Shared::status()->name, NAME_BUF * sizeof(char), clientName);
}

void gs_clientThread(uint64 serverConnectionHandlerID, uint64 channelID)
{
	clientThreadActive = true;
	std::cout <<  "[Gspeak] clientThread created" << std::endl;

	HMAP_RESULT result = Shared::openClients();
	if (result != HMAP_RESULT::SUCCESS)
	{
		gs_criticalError(GetLastError());
		std::cout << "[Gspeak] open clients view failed " << (int)result << std::endl;
		return;
	}

	std::cout << "[Gspeak] has been loaded successfully" << std::endl;
	ts3Functions.printMessageToCurrentTab("[Gspeak] has been loaded successfully!");

	TS3_VECTOR zero = { 0.0f, 0.0f, 0.0f };
	TS3_VECTOR forward;
	TS3_VECTOR upward;

	anyID clientID;
	ts3Functions.getClientID(serverConnectionHandlerID, &clientID);
	gs_updateStatusName(serverConnectionHandlerID, clientID);
	if (!gs_getSampleRate(serverConnectionHandlerID, channelID, &gspeakChannelSampleRate))
		std::cout << "[Gspeak] requesting sample rate failed" << std::endl;
	std::cout << "[Gspeak] using sample rate " << gspeakChannelSampleRate << std::endl;

	while (!clientThreadBreak)
	{
		ts3Functions.getClientID(serverConnectionHandlerID, &clientID);
		if (clientID != Shared::status()->clientID)
		{
			Shared::status()->clientID = clientID;
			Shared::status()->inChannel = true;
		}
		gs_cmdCheck(serverConnectionHandlerID, clientID);

		forward.x = Shared::status()->forward[0];
		forward.y = Shared::status()->forward[1];
		forward.z = Shared::status()->forward[2];
		upward.x = Shared::status()->upward[0];
		upward.y = Shared::status()->upward[1];
		upward.z = Shared::status()->upward[2];
		ts3Functions.systemset3DListenerAttributes(serverConnectionHandlerID, &zero, &forward, &upward);

		gs_scanClients(serverConnectionHandlerID);

		this_thread::sleep_for(chrono::milliseconds(SCAN_SPEED));
	}

	Shared::closeClients();
	ts3Functions.printMessageToCurrentTab("[Gspeak] has been shut down!");

	gs_moveDefaultChannel(serverConnectionHandlerID, clientID);

	Shared::status()->clientID = 0;
	Shared::status()->inChannel = false;

	clientThreadActive = false;
	std::cout <<  "[Gspeak] clientThread destroyed" << std::endl;
}

void gs_statusThread()
{
	std::cout <<  "[Gspeak] statusThread created" << std::endl;
	statusThreadActive = true;
	while (!statusThreadBreak)
	{
		//Gmod initialized
		if (Shared::status()->tslibV > 0)
		{
			uint64 serverID = ts3Functions.getCurrentServerConnectionHandlerID();
			anyID clientID;
			if (ts3Functions.getClientID(serverID, &clientID) == ERROR_ok)
			{
				gs_cmdCheck(serverID, clientID);
			}
		}

		this_thread::sleep_for(chrono::milliseconds(SCAN_SPEED));
	}
	statusThreadActive = false;
	std::cout <<  "[Gspeak] statusThread destroyed" << std::endl;
}

void ts3plugin_onClientDisplayNameChanged(uint64 serverConnectionHandlerID, anyID clientID, const char* displayName, const char* uniqueClientIdentifier)
{
	gs_updateStatusName(serverConnectionHandlerID, clientID, (char*)displayName);
}

void ts3plugin_onClientMoveEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, const char* moveMessage)
{
	gs_clientMoved(serverConnectionHandlerID, clientID, newChannelID);
}

void ts3plugin_onClientMoveMovedEvent(uint64 serverConnectionHandlerID, anyID clientID, uint64 oldChannelID, uint64 newChannelID, int visibility, anyID moverID, const char* moverName, const char* moverUniqueIdentifier, const char* moveMessage)
{
	gs_clientMoved(serverConnectionHandlerID, clientID, newChannelID);
}

void ts3plugin_onConnectStatusChangeEvent(uint64 serverConnectionHandlerID, int newStatus, unsigned int errorNumber)
{
	if (newStatus == STATUS_DISCONNECTED)
		if (clientThreadActive)
			gs_setIdle();

	if (newStatus == STATUS_CONNECTION_ESTABLISHED && errorNumber == ERROR_ok)
	{
		//ALL FINE
	}
}

bool gs_isMe(uint64 serverConnectionHandlerID, anyID clientID)
{
	anyID localClientID;
	ts3Functions.getClientID(serverConnectionHandlerID, &localClientID);
	if (localClientID == clientID)
		return true;
	return false;
}

void ts3plugin_onTalkStatusChangeEvent(uint64 serverConnectionHandlerID, int talkStatus, int isReceivedWhisper, anyID clientID)
{
	if (!clientThreadActive)
		return;
	if (gs_isMe(serverConnectionHandlerID, clientID))
	{
		if (talkStatus == STATUS_TALKING)
			Shared::status()->talking = true;
		else
			Shared::status()->talking = false;
	}
	else
	{
		int index = Shared::findClientIndex(clientID);
		if (index != -1)
		{
			if (talkStatus != STATUS_TALKING)
				Shared::clients()[index].talking = false;
		}
	}
}

void ts3plugin_onCustom3dRolloffCalculationClientEvent(uint64 serverConnectionHandlerID, anyID clientID, float distance, float* volume)
{
	if (!clientThreadActive)
		return;
	*volume = 1.0f;
}

void voiceEffect_mute(short* samples, int sampleCount, int channels)
{
	for (int i = 0; i < sampleCount * channels; i++)
	{
		samples[i] = 0;
		/*short sample_it = i * channels;
		for (int j = 0; j < channels; j++)
			samples[sample_it + j] = 0;*/
	}
}

void voiceEffect_radio(short* samples, int sampleCount, int channels, float clientVolume)
{
	for (int i = 0; i < sampleCount; i++)
	{
		//Downsampling ignore this iteration
		if (i % Shared::status()->radioEffect.downsampler != 0)
			continue;

		int sample_it = i * channels;

		//Noise
		float noise = (((float)rand() / RAND_MAX) * SHORT_SIZE * 2) - SHORT_SIZE;
		for (int j = 0; j < channels; j++)
		{
			//Distortion
			/*short sample_new = (short)((samples[sample_it] > Shared::status()->radioEffect.distortion ? Shared::status()->radioEffect.distortion :
										samples[sample_it] < Shared::status()->radioEffect.distortion * (-1) ? Shared::status()->radioEffect.distortion * (-1) : samples[sample_it])
										* Shared::status()->radioEffect.volume * clientVolume);*/

			short sample_new;
			//Apply caps
			if (abs(samples[sample_it]) > Shared::status()->radioEffect.distortion)
				sample_new = Shared::status()->radioEffect.distortion * (samples[sample_it] > 0 ? 1 : -1);
			else
				sample_new = samples[sample_it];
			sample_new = (short)(sample_new * Shared::status()->radioEffect.volume);// *clientVolume);

			short sample_noise = (short)(sample_new + noise * Shared::status()->radioEffect.noise);
			//Downsampling override future samples
			bool swap = false;
			for (int n = 0; n < Shared::status()->radioEffect.downsampler; n++)
			{
				//i dont get the swapping part
				//maybe so we always keep some clean values
				int index = sample_it + j + n * channels;
				if (swap)
					samples[index] = sample_noise;
				else
					samples[index] = sample_new;
				swap = !swap;
			}
		}
	}
}

/* - Three one-poles combined in parallel
 * - Output stays within input limits
 * - 18 dB/oct (approx) frequency response rolloff
 * - Quite fast, 2x3 parallel multiplications/sample, no internal buffers
 * - Time-scalable, allowing use with different samplerates
 * - Impulse and edge responses have continuous differential
 * - Requires high internal numerical precision
 */

//in ein array prop channel
double areg = 0;
double breg = 0;
double creg = 0;

void filter_lowPass2(short* samples, int sampleCount, int channel, int channels, float scale = 100.0f, float smoothness = 0.999f)
{
	/* Parameters */
	// Number of samples from start of edge to halfway to new value
	//const double        scale = 100;
	// 0 < Smoothness < 1. High is better, but may cause precision problems
	//const double        smoothness = 0.999;

	/* Precalc variables */
	double a = 1.0 - (2.4 / scale); // Could also be set directly
	double b = (double)smoothness;      //         -"-
	double acoef = a;
	double bcoef = a * b;
	double ccoef = a * b * b;
	double mastergain = 1.0 / (-1.0 / (log(a) + 2.0 * log(b)) + 2.0 /
		(log(a) + log(b)) - 1.0 / log(a));
	double again = mastergain;
	double bgain = mastergain * (log(a * b * b) * (log(a) - log(a * b)) /
		((log(a * b * b) - log(a * b)) * log(a * b))
		- log(a) / log(a * b));
	double cgain = mastergain * (-(log(a) - log(a * b)) /
		(log(a * b * b) - log(a * b)));

	/* Runtime variables */
	long sample;

	/* Main loop */
	for (sample = 0; sample < sampleCount; sample++)
	{
		int sampleIndex = sample * channels + channel;

		/* Update filters */
		areg = acoef * areg + samples[sampleIndex];
		breg = bcoef * breg + samples[sampleIndex];
		creg = ccoef * creg + samples[sampleIndex];

		/* Combine filters in parallel */
		long temp = (long)(again * areg
						 + bgain * breg
						 + cgain * creg);

		/* Check clipping */
		short temp2 = (short)max(min(32767, temp), -32768);

		/* Store new value */
		samples[sampleIndex] = temp2;
	}
}

void voiceEffect_water(short* samples, int sampleCount, int channels)
{
	for (int channel = 0; channel < channels; channel++)
	{
		filter_lowPass2(samples, sampleCount, channel, channels);
	}
}

void voiceEffect_wall(short* samples, int sampleCount, int channels)
{
	for (int channel = 0; channel < channels; channel++)
	{
		filter_lowPass2(samples, sampleCount, channel, channels);
	}
}

void voiceEffect_volume(short* samples, int sampleCount, int channels, float clientVolume)
{
	for (int i = 0; i < sampleCount * channels; i++)
	{
		samples[i] = (short)(samples[i] * clientVolume);
	}
}

void ts3plugin_onEditPostProcessVoiceDataEvent(uint64 serverConnectionHandlerID, anyID clientID, short* samples, int sampleCount, int channels, const unsigned int* channelSpeakerArray, unsigned int* channelFillMask)
{
	if (!clientThreadActive)
		return;
	int index = Shared::findClientIndex(clientID);
	if (index == -1)
	{
		voiceEffect_mute(samples, sampleCount, channels);
		return;
	}

	Client &client = Shared::clients()[index];
	//If volume between 0 and 1
	float clientVolume = min(client.volume_gm, 1.0f);
	if (clientVolume <= 0)
	{
		voiceEffect_mute(samples, sampleCount, channels);
		client.volume_ts = 0;
		return;
	}

	//data has to be processed => client must be talking
	if (client.talking != true)
		client.talking = true;

	float totalSampleVolume = 0;
	for (int i = 0; i < sampleCount; i++)
	{
		unsigned short sample_it = i * channels;
		//Average volume detection for mouth move animation (one channel should be enough)
		totalSampleVolume += min(abs(samples[sample_it]), (short)VOLUME_MAX);

		/*if (client.radio)
		{
			voiceEffect_radio(samples, sample_it, channels, clientVolume);
		}
		else
		{
			voiceEffect_normal(samples, sample_it, channels, clientVolume);
		}*/
	}
	//Sending average volume to Gmod
	client.volume_ts = totalSampleVolume / sampleCount / VOLUME_MAX;

	switch (client.effect)
	{
	case VoiceEffect::Radio:
		voiceEffect_radio(samples, sampleCount, channels, clientVolume);
		break;
	case VoiceEffect::Water:
		voiceEffect_water(samples, sampleCount, channels);
		break;
	case VoiceEffect::Wall:
		voiceEffect_wall(samples, sampleCount, channels);
		break;
	/*case VoiceEffect::None:
	default:
		break;*/
	};

	voiceEffect_volume(samples, sampleCount, channels, clientVolume);
}