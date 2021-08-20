//
//  MFDeluxe.h
//
//  Created by Rodolphe Pineau on 2012/08/10.
//  JMI MotoFocus Deluxe X2 plugin

#ifndef __MFDeluxe__
#define __MFDeluxe__
#include <math.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <ctype.h>
#include <memory.h>
#ifdef SB_MAC_BUILD
#include <unistd.h>
#endif
#ifdef SB_WIN_BUILD
#include <time.h>
#endif

#include <string>
#include <vector>
#include <sstream>
#include <iostream>
#include <exception>
#include <typeinfo>
#include <stdexcept>

#include "../../licensedinterfaces/sberrorx.h"
#include "../../licensedinterfaces/serxinterface.h"
#include "../../licensedinterfaces/loggerinterface.h"
#include "../../licensedinterfaces/sleeperinterface.h"

// #define MFDeluxe_DEBUG 2

#define PLUGIN_VERSION 1.0
// different timeouts in ms
#define HALT_TIMEOUT                3000
#define MAX_TIMEOUT_FOC             2500
#define SHORT_TIMEOUT                250
#define MEDIUM_TIMEOUT              1000
#define MAX_READ_WAIT_TIMEOUT_FOC     25

#define SERIAL_BUFFER_SIZE_FOC 1024
#define LOG_BUFFER_SIZE         256

enum MFDeluxe_Errors    {MFDeluxe_OK = 0, NOT_CONNECTED, ND_CANT_CONNECT, MFDeluxe_BAD_CMD_RESPONSE, COMMAND_FAILED, COMMAND_NORMAL_TIMEOUT_FOC, PARSE_FAILED};
enum MotorDir       {NORMAL = 0 , REVERSE};
enum MotorStatus    {IDLE = 0, MOVING};


class CMFDeluxeController
{
public:
    CMFDeluxeController();
    ~CMFDeluxeController();

    int         Connect(const char *pszPort);
    void        Disconnect(void);
    bool        IsConnected(void) { return m_bIsConnected; };

    void        SetSerxPointer(SerXInterface *p) { m_pSerx = p; };
    void        setSleeper(SleeperInterface *pSleeper) { m_pSleeper = pSleeper; };

    int         getDeviceData();
    void        getFirmwareString(std::string &sFirmawre);
    void        getDeviceTypeString(std::string &sName);

    int         haltFocuser();
    int         gotoPosition(int nPos);
    int         moveRelativeToPosision(int nSteps);

    // command complete functions
    int         isGoToComplete(bool &bComplete);
    int         isMotorMoving(bool &bMoving);

    // getter and setter
    int         getPosition(int &nPosition);
    int         syncMotorPosition(int nPos);
    int         getPosLimit(void);
    void        setPosLimit(int nLimit);
    bool        isPosLimitEnabled(void);
    void        enablePosLimit(bool bEnable);

    void        getMotorType(int &nType);
    int         setMotorType(int nType);
    int         factoryReset();

    int         setCurPosAsZero();

protected:

    int             MFDeluxeCommand(const char *pszCmd, std::string &sResp, bool bExpectResponse = true, int nTimeout = MAX_TIMEOUT_FOC);
    int             readResponse(std::string &sResp, int nTimeout = MAX_TIMEOUT_FOC);

    int             parseFields(const std::string sResp, std::vector<std::string> &svFields, char cSeparator);
    std::string&    trim(std::string &str, const std::string &filter );
    std::string&    ltrim(std::string &str, const std::string &filter);
    std::string&    rtrim(std::string &str, const std::string &filter);
    std::string     findField(std::vector<std::string> &svFields, const std::string& token);

    SerXInterface   *m_pSerx;
    SleeperInterface    *m_pSleeper;

    bool            m_bDebugLog;
    bool            m_bIsConnected;

    int             m_nCurPos;
    int             m_nTargetPos;
    int             m_nPosLimit;
    bool            m_bPosLimitEnabled;
    bool            m_bMoving;

    std::string     m_sMotorType;
    std::string     m_sFirmware;
    std::string     m_sDeviceType;

#ifdef MFDeluxe_DEBUG
    std::string m_sLogfilePath;
    // timestamp for logs
    char *timestamp;
    time_t ltime;
    FILE *Logfile;      // LogFile
#endif

};

#endif //__MFDeluxe__
