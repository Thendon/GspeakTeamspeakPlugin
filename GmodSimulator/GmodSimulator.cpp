#define THREAD_DELAY 100

#include "../source/shared.h"
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

void sendRadioSettings(const std::vector<string>& ins)
{
    if (!validateParameterCount(ins, 4))
        return;

    short radioDownsampler = std::stoi(ins[0]);
    short radioDistortion = std::stoi(ins[1]);
    float radioVolume = std::stof(ins[2]);
    float radioNoise = std::stof(ins[3]);

    Shared::status()->radio_downsampler = radioDownsampler;
    Shared::status()->radio_distortion = radioDistortion;
    Shared::status()->radio_volume = radioVolume;
    Shared::status()->radio_volume_noise = radioNoise;
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
    bool isRadio = std::stoi(ins[5]);


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
        Shared::clients()[index].radio = isRadio;
    }

    Shared::clients()[index].volume_gm = volume;
    Shared::clients()[index].pos[0] = x;
    Shared::clients()[index].pos[1] = z;
    Shared::clients()[index].pos[2] = y;

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
    if (!validateParameterCount(ins, 1))
        return;

    string password = ins[0];

    //dont write this into status, maybe add command params
    strcpy_s(Shared::status()->password, PASS_BUF * sizeof(char), password.c_str());
    Shared::status()->command = CMD_FORCEMOVE;
}

void rename(const std::vector<string>& ins)
{
    if (!validateParameterCount(ins, 1))
        return;

    string name = ins[0];

    //dont write this into status, maybe add command params
    strcpy_s(Shared::status()->name, NAME_BUF * sizeof(char), name.c_str());
    Shared::status()->command = CMD_RENAME;
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
    else if (cmd == "connect")
        forcemove(ins);
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
        std::cout << "status map error " << result << std::endl;
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