#define THREAD_DELAY 100

#include "../source/shared/shared.h"
#include <thread>
#include <Windows.h>
#include <iostream>
#include <string>
#include <vector>

using namespace Gspeak;
using namespace std;

bool gmodThreadRunning = false;
bool breakGmodThread = false;

void gmodThread()
{
    if (gmodThreadRunning)
        return;

    std::cout << "gmod started" << std::endl;
    gmodThreadRunning = true;
    Shared::openClients();

    while (!breakGmodThread)
    {
        this_thread::sleep_for(chrono::milliseconds(THREAD_DELAY));
    }

    Shared::closeClients();
    gmodThreadRunning = false;
    breakGmodThread = false;
    std::cout << "gmod stopped" << std::endl;
}

std::vector<string> split(string str, char seperator)
{
    std::vector<string> strings;
    int i = 0;
    int startIndex = 0, endIndex = 0;
    while (i <= str.length())
    {
        if (str[i] == seperator || i == str.length())
        {
            endIndex = i;
            string subStr = "";
            subStr.append(str, startIndex, endIndex - startIndex);
            strings.push_back(subStr);
            startIndex = endIndex + 1;
        }
        i++;
    }

    return strings;
}

bool validateParameterCount(const std::vector<string>& ins, int required)
{
    if (ins.size() != required)
    {
        std::cout << "invalid parameter count " << ins.size() << "/" << required << std::endl;
        return false;
    }

    return true;
}

bool validateParameterCount(const std::vector<string>& ins, int min, int max)
{
    if (ins.size() < min || ins.size() > max)
    {
        std::cout << "invalid parameter count " << ins.size() << "(" << min << "-" << max << ")" << std::endl;
        return false;
    }

    return true;
}

void sendRadioSettings(const std::vector<string>& ins)
{
    if (!validateParameterCount(ins, 4))
        return;

    short radioDownsampler = std::stoi(ins[0]);
    short radioDistortion = std::stoi(ins[1]);
    float radioVolume = std::stof(ins[2]);
    float radioNoise = std::stof(ins[3]);

    Shared::status()->radioEffect.downsampler = radioDownsampler;
    Shared::status()->radioEffect.distortion = radioDistortion;
    Shared::status()->radioEffect.volume = radioVolume;
    Shared::status()->radioEffect.noise = radioNoise;
}

void sendWaterSettings(const std::vector<string>& ins)
{
    if (!validateParameterCount(ins, 2))
        return;

    double smoothness = std::stod(ins[0]);
    double scale = std::stod(ins[1]);

    Shared::status()->waterEffect.smooth = smoothness;
    Shared::status()->waterEffect.scale = scale;
}

void sendLocalPlayer(const std::vector<string>& ins)
{
    if (!validateParameterCount(ins, 6))
        return;

    float fx = std::stof(ins[0]);
    float fy = std::stof(ins[1]);
    float fz = std::stof(ins[2]);

    float ux = std::stof(ins[3]);
    float uy = std::stof(ins[4]);
    float uz = std::stof(ins[5]);

    Shared::status()->forward[0] = fx;
    Shared::status()->forward[1] = fy;
    Shared::status()->forward[2] = fz;

    Shared::status()->upward[0] = ux;
    Shared::status()->upward[1] = uy;
    Shared::status()->upward[2] = uz;
}

void sendPlayer(const std::vector<string>& ins)
{
    if (!validateParameterCount(ins, 6))
        return;

    int id = std::stoi(ins[0]);
    float volume = std::stof(ins[1]);
    float x = std::stof(ins[2]);
    float y = std::stof(ins[3]);
    float z = std::stof(ins[4]);
    VoiceEffect effect = (VoiceEffect)std::stoi(ins[5]);

    int index = Shared::findClientIndex(id);
    bool isNew = index == -1;

    if (isNew)
    {
        for (int i = 0; i < PLAYER_MAX; i++)
        {
            if (Shared::clients()[i].clientID == 0)
            {
                index = i;
                break;
            }
        }
    }

    if (index == -1)
    {
        std::cout << "player not found and no free index" << std::endl;
        return;
    }

    if (isNew)
    {
        Shared::clients()[index].clientID = id;
    }

    Shared::clients()[index].volume_gm = volume;
    Shared::clients()[index].pos[0] = x;
    Shared::clients()[index].pos[1] = z;
    Shared::clients()[index].pos[2] = y;
    Shared::clients()[index].effect = effect;

    std::cout << (isNew ? "added" : "updated") << "(" << index << ") player " << Shared::clients()[index] << std::endl;
}

void removePlayer(const std::vector<string>& ins)
{
    if (!validateParameterCount(ins, 1))
        return;

    int id = std::stoi(ins[0]);
    int index = Shared::findClientIndex(id);
    if (index == -1)
    {
        std::cout << "could not remove player " << id << std::endl;
        return;
    }

    Shared::clients()[index].clientID = 0;
}

void clearPlayers()
{
    for (int i = 0; i < PLAYER_MAX; i++)
    {
        Shared::clients()[i].clientID = 0;
    }
}

void forcemove(const std::vector<string>& ins)
{
    //request password, channelid, channelname here?
    if (!validateParameterCount(ins, 1, 2))
        return;

    string password = ins[0];
    string channelName = ins.size() >= 2 ? ins[1] : "";

    string args = password + ';' + channelName;

    if (args.length() > CMD_ARGS_BUF)
    {
        std::cout << "args exceed command args buffer " << args.length() << "/" << CMD_ARGS_BUF << " " << args << std::endl;
        return;
    }
    
    Shared::status()->command = Command::ForceMove;
    strcpy_s(Shared::status()->commandArgs, CMD_ARGS_BUF * sizeof(char), args.c_str());
}

void forcekick()
{
    Shared::status()->command = Command::ForceKick;
    strcpy_s(Shared::status()->commandArgs, CMD_ARGS_BUF * sizeof(char), "");
}

void rename(const std::vector<string>& ins)
{
    if (!validateParameterCount(ins, 1))
        return;

    string name = ins[0];

    string args = name;

    Shared::status()->command = Command::Rename;
    strcpy_s(Shared::status()->commandArgs, CMD_ARGS_BUF * sizeof(char), args.c_str());
}

void printStatus()
{
    std::cout << *Shared::status() << std::endl;
}

void printPlayers()
{
    bool hadPlayer = false;

    for (int i = 0; i < PLAYER_MAX; i++)
    {
        if (Shared::clients()[i].clientID == 0)
            continue;

        hadPlayer = true;
        std::cout << Shared::clients()[i] << std::endl;
    }

    if (!hadPlayer)
        std::cout << "no player listed" << std::endl;
}

void handleInput(const string& cmd, const std::vector<string>& ins)
{
    if (cmd == "start")
        thread(gmodThread).detach();
    else if (cmd == "stop")
        breakGmodThread = true;
    else if (cmd == "status")
        printStatus();
    else if (cmd == "players")
        printPlayers();
    else if (cmd == "rsettings")
        sendRadioSettings(ins);
    else if (cmd == "wsettings")
        sendWaterSettings(ins);
    else if (cmd == "connect")
        forcemove(ins);
    else if (cmd == "kick")
        forcekick();
    else if (cmd == "rename")
        rename(ins);
    else if (cmd == "send")
        sendPlayer(ins);
    else if (cmd == "remove")
        removePlayer(ins);
    else if (cmd == "clear")
        clearPlayers();
    else if (cmd == "local")
        sendLocalPlayer(ins);
    else
        std::cout << "unknown command " << cmd << std::endl;
}

int main()
{
    HMAP_RESULT result = Shared::openStatus();
    if (result != HMAP_RESULT::SUCCESS)
    {
        std::cout << "status map error " << (int)result << std::endl;
        return 1;
    }

    Shared::status()->tslibV = 3000;

    std::cout << "Gmod Simulator started " << Shared::status()->gspeakV << " " << Shared::status()->tslibV << std::endl;

    thread(gmodThread).detach();

    std::string in;
    while (true)
    {
        getline(std::cin, in);

        std::vector<string> ins = split(in, ' ');
        if (ins.size() > 0 && ins[ins.size() - 1].empty())
            ins.pop_back();

        string cmd = "";
        if (ins.size() > 0)
        {
            cmd = ins[0];
            ins.erase(ins.begin());
        }

        //cout << cmd << " " << ins.size();

        if (cmd.empty())
            continue;

        if (cmd == "q" || cmd == "quit" || cmd == "exit")
            break;

        try
        {
            handleInput(cmd, ins);
        }
        catch (const std::exception& e)
        {
            std::cout << e.what() << std::endl;
        }
    }

    breakGmodThread = true;
    while (gmodThreadRunning)
        this_thread::sleep_for(chrono::milliseconds(THREAD_DELAY));

    Shared::status()->tslibV = 0;

    Shared::closeStatus();

    return 0;
}