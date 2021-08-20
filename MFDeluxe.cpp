// 
//  MFDeluxe.cpp
//
//  Created by Rodolphe Pineau on 2012/08/10.
//  JMI MotoFocus Deluxe X2 plugin

#include "MFDeluxe.h"

CMFDeluxeController::CMFDeluxeController()
{

    m_pSerx = NULL;

    m_bDebugLog = false;
    m_bIsConnected = false;

    m_nCurPos = 0;
    m_nTargetPos = 0;
    m_nPosLimit = 0;
    m_bPosLimitEnabled = 0;
    m_bMoving = false;


#ifdef MFDeluxe_DEBUG
#if defined(SB_WIN_BUILD)
    m_sLogfilePath = getenv("HOMEDRIVE");
    m_sLogfilePath += getenv("HOMEPATH");
    m_sLogfilePath += "\\MFDLog.txt";
#elif defined(SB_LINUX_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/MFDLog.txt";
#elif defined(SB_MAC_BUILD)
    m_sLogfilePath = getenv("HOME");
    m_sLogfilePath += "/MFDLog.txt";
#endif
    Logfile = fopen(m_sLogfilePath.c_str(), "w");
#endif

#if MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CMFDeluxeController] Version %3.2f build 2021_08_17_0940.\n", timestamp, PLUGIN_VERSION);
    fprintf(Logfile, "[%s] [CMFDeluxeController] Constructor Called.\n", timestamp);
    fflush(Logfile);
#endif

}

CMFDeluxeController::~CMFDeluxeController()
{
#ifdef	MFDeluxe_DEBUG
    // Close LogFile
    if (Logfile) fclose(Logfile);
#endif
}

int CMFDeluxeController::Connect(const char *pszPort)
{
    int nErr = MFDeluxe_OK;

    if(!m_pSerx)
        return ERR_COMMNOLINK;

#ifdef MFDeluxe_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CMFDeluxeController::Connect Called %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    // 9600 8N1
    nErr = m_pSerx->open(pszPort, 19200, SerXInterface::B_NOPARITY, "-DTR_CONTROL 1");
    if( nErr == 0)
        m_bIsConnected = true;
    else
        m_bIsConnected = false;

    if(!m_bIsConnected)
        return nErr;

#ifdef MFDeluxe_DEBUG
	ltime = time(NULL);
	timestamp = asctime(localtime(&ltime));
	timestamp[strlen(timestamp) - 1] = 0;
	fprintf(Logfile, "[%s] CMFDeluxeController::Connect connected to %s\n", timestamp, pszPort);
	fflush(Logfile);
#endif

    getDeviceData();

    return nErr;
}

void CMFDeluxeController::Disconnect()
{
    if(m_bIsConnected && m_pSerx)
        m_pSerx->close();

	m_bIsConnected = false;
}

#pragma mark - Device info

int CMFDeluxeController::getDeviceData()
{
    int nErr;
    std::string sResp;
    std::vector<std::string> svFields;
    std::vector<std::string> svDataFields;
    std::string sResult;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = MFDeluxeCommand("$$\n", sResp, true, SHORT_TIMEOUT);
    if(nErr)
        return nErr;

    nErr = parseFields(sResp, svFields, '\r');
    if(nErr)
        return nErr;
    sResult = findField(svFields, "Firmware Version");
    if(!sResult.empty()) {
        nErr = parseFields(sResult, svDataFields, ':');
        if(!nErr) {
            m_sFirmware.assign(svDataFields[1]);
        }
    }

    sResult = findField(svFields, "Device ID");
    if(!sResult.empty()) {
        nErr = parseFields(sResult, svDataFields, ':');
        if(!nErr) {
            m_sDeviceType.assign(svDataFields[1]);
        }
    }

    sResult = findField(svFields, "Hardare Version");
    if(!sResult.empty()) {
        nErr = parseFields(sResult, svDataFields, ':');
        if(!nErr) {
            m_sDeviceType.append(" ");
            m_sDeviceType.append(svDataFields[1]);
        }
    }

//    m_nMotorType "Motor Type"
    sResult = findField(svFields, "Motor Type");
    if(!sResult.empty()) {
        nErr = parseFields(sResult, svDataFields, ':');
        if(!nErr) {
            m_sMotorType.append(svDataFields[1]);
        }
    }

#ifdef MFDeluxe_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CMFDeluxeController::getDeviceData] m_sFirmware   : %s\n", timestamp, m_sFirmware.c_str());
    fprintf(Logfile, "[%s] [CMFDeluxeController::getDeviceData] m_sDeviceType : %s\n", timestamp, m_sDeviceType.c_str());
    fprintf(Logfile, "[%s] [CMFDeluxeController::getDeviceData] m_sMotorType  : %s\n", timestamp, m_sMotorType.c_str());
    fflush(Logfile);
#endif


    return nErr;
}

void CMFDeluxeController::getFirmwareString(std::string &sFirmware)
{
    sFirmware.assign(m_sFirmware);

}

void CMFDeluxeController::getDeviceTypeString(std::string &sName)
{
    sName.assign(m_sDeviceType);
}

void CMFDeluxeController::getMotorType(int &nType)
{
    std::string sType;

    sType = trim(m_sMotorType,"vV");
    nType = std::stoi(sType);
}

int CMFDeluxeController::setMotorType(int nType)
{
    int nErr = MFDeluxe_OK;
    char szCmd[SERIAL_BUFFER_SIZE_FOC];
    std::string sResp;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE_FOC, "$SM%d\n", nType);
    nErr = MFDeluxeCommand(szCmd, sResp, false);
    if(nErr)
        return nErr;

    m_pSleeper->sleep(MEDIUM_TIMEOUT);
    m_sMotorType = std::to_string(nType);

    return nErr;
}


#pragma mark move commands
int CMFDeluxeController::haltFocuser()
{
    int nErr = MFDeluxe_OK;
    std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = MFDeluxeCommand("S\n", sResp, false);
    m_pSleeper->sleep(HALT_TIMEOUT);

    // parse output to update m_curPos
    getPosition(m_nCurPos);
    return nErr;
}

int CMFDeluxeController::gotoPosition(int nPos)
{
    int nErr;
    char szCmd[SERIAL_BUFFER_SIZE_FOC];
    std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    if (m_bPosLimitEnabled && nPos>m_nPosLimit)
        return ERR_LIMITSEXCEEDED;

#ifdef MFDeluxe_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CMFDeluxeController::gotoPosition goto position  : %d\n", timestamp, nPos);
    fflush(Logfile);
#endif

    snprintf(szCmd, SERIAL_BUFFER_SIZE_FOC, "$GT%d\n", nPos);
    nErr = MFDeluxeCommand(szCmd, sResp, false);
    if(nErr)
        return nErr;

    // we need to wait a bit after sending the goto or the focuser doesn't move
    m_pSleeper->sleep(MEDIUM_TIMEOUT);

    m_nTargetPos = nPos;

    return nErr;
}

int CMFDeluxeController::moveRelativeToPosision(int nSteps)
{
    int nErr = MFDeluxe_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

#ifdef MFDeluxe_DEBUG
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] CMFDeluxeController::gotoPosition goto relative position  : %d\n", timestamp, nSteps);
    fflush(Logfile);
#endif

    m_nTargetPos = m_nCurPos + nSteps;
    nErr = gotoPosition(m_nTargetPos);
    return nErr;
}

#pragma mark command complete functions

int CMFDeluxeController::isGoToComplete(bool &bComplete)
{
    int nErr = MFDeluxe_OK;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    bComplete = false;
    isMotorMoving(m_bMoving);
    if(!m_bMoving)
        bComplete = true;

#if defined MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CMFDeluxeController::isGoToComplete] bComplete = %s\n", timestamp, bComplete?"True":"False");
#endif

    return nErr;
}

int CMFDeluxeController::isMotorMoving(bool &bMoving)
{
    int nErr = MFDeluxe_OK;
    std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    bMoving = false;

    nErr = MFDeluxeCommand("$M\n", sResp, true, SHORT_TIMEOUT);
    if(nErr)
        return nErr;

    if(sResp.find("0")!=-1) {
        bMoving = false;
    }
    else if(sResp.find("1")!=-1) {
        bMoving = true;
    }
    else
        nErr = ERR_CMDFAILED;

    return nErr;
}

#pragma mark getters and setters

int CMFDeluxeController::getPosition(int &nPosition)
{
    int nErr = MFDeluxe_OK;
    std::string sResp;

    if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    nErr = MFDeluxeCommand("$P\n", sResp, true, SHORT_TIMEOUT);
    if(nErr)
        return nErr;

    // convert response
    try {
        nPosition = std::stoi(sResp);
        m_nCurPos = nPosition;
    }
    catch(const std::exception& e) {
#if defined MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CMFDeluxeController::getPosition] convertsion exception = %s\n", timestamp, e.what());
#endif
    }

    return nErr;
}


int CMFDeluxeController::syncMotorPosition(int nPos)
{
    int nErr = MFDeluxe_OK;
    char szCmd[SERIAL_BUFFER_SIZE_FOC];
    std::string sResp;

	if(!m_bIsConnected)
		return ERR_COMMNOLINK;

    snprintf(szCmd, SERIAL_BUFFER_SIZE_FOC, "$SP%d\n", nPos);
    nErr = MFDeluxeCommand(szCmd, sResp, false);
    if(nErr)
        return nErr;

    m_pSleeper->sleep(MEDIUM_TIMEOUT);
    m_nCurPos = nPos;
    return nErr;
}

int CMFDeluxeController::getPosLimit()
{
    return m_nPosLimit;
}

void CMFDeluxeController::setPosLimit(int nLimit)
{
    m_nPosLimit = nLimit;
}

bool CMFDeluxeController::isPosLimitEnabled()
{
    return m_bPosLimitEnabled;
}

void CMFDeluxeController::enablePosLimit(bool bEnable)
{
    m_bPosLimitEnabled = bEnable;
}


int CMFDeluxeController::factoryReset()
{
    int nErr = MFDeluxe_OK;
    std::string sResp;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = MFDeluxeCommand("$FR\n", sResp, false);
    m_pSleeper->sleep(MEDIUM_TIMEOUT);
    return nErr;
}

int CMFDeluxeController::setCurPosAsZero()
{
    int nErr = MFDeluxe_OK;
    std::string sResp;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    nErr = MFDeluxeCommand("$H0\n", sResp, false);
    m_pSleeper->sleep(MEDIUM_TIMEOUT);

    return nErr;
}

#pragma mark command and response functions

int CMFDeluxeController::MFDeluxeCommand(const char *pszCmd, std::string &sResp, bool bExpectResponse, int nTimeout)
{
    int nErr = MFDeluxe_OK;
    unsigned long  ulBytesWrite;

    if(!m_bIsConnected)
        return ERR_COMMNOLINK;

    m_pSerx->purgeTxRx();
#if defined MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CMFDeluxeController::MFDeluxeCommand] Sending %s\n", timestamp, pszCmd);
    fflush(Logfile);
#endif
    nErr = m_pSerx->writeFile((void *)pszCmd, strlen(pszCmd), ulBytesWrite);
    m_pSerx->flushTx();

    // printf("Command %s sent. wrote %lu bytes\n", szCmd, ulBytesWrite);
    if(nErr){
#if defined MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 2
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CMFDeluxeController::MFDeluxeCommand] writeFile error %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        return nErr;
    }

    if(bExpectResponse) {
        // read response
        nErr = readResponse(sResp, nTimeout);
        if(nErr != COMMAND_NORMAL_TIMEOUT_FOC){
            return nErr;
        }
        else
            nErr = MFDeluxe_OK;
#if defined MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 2
    ltime = time(NULL);
    timestamp = asctime(localtime(&ltime));
    timestamp[strlen(timestamp) - 1] = 0;
    fprintf(Logfile, "[%s] [CMFDeluxeController::MFDeluxeCommand] response \"%s\"\n", timestamp, sResp.c_str());
    fflush(Logfile);
#endif
    }
    return nErr;
}


int CMFDeluxeController::readResponse(std::string &sResp, int nTimeout)
{
    int nErr = MFDeluxe_OK;
    char pszBuf[SERIAL_BUFFER_SIZE_FOC];
    unsigned long ulBytesRead = 0;
    unsigned long ulTotalBytesRead = 0;
    char *pszBufPtr;
    int nBytesWaiting = 0 ;
    int nbTimeouts = 0;

    sResp.clear();
    memset(pszBuf, 0, SERIAL_BUFFER_SIZE_FOC);
    pszBufPtr = pszBuf;

    do {
        nErr = m_pSerx->bytesWaitingRx(nBytesWaiting);
#if defined MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 3
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CMFDeluxeController::readResponse] nBytesWaiting = %d\n", timestamp, nBytesWaiting);
        fprintf(Logfile, "[%s] [CMFDeluxeController::readResponse] nBytesWaiting nErr = %d\n", timestamp, nErr);
        fflush(Logfile);
#endif
        if(!nBytesWaiting) {
            nbTimeouts += MAX_READ_WAIT_TIMEOUT_FOC;
            if(nbTimeouts >= nTimeout) {
#if defined MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 3
                ltime = time(NULL);
                timestamp = asctime(localtime(&ltime));
                timestamp[strlen(timestamp) - 1] = 0;
                fprintf(Logfile, "[%s] [CMFDeluxeController::readResponse] bytesWaitingRx timeout, no data for %d ms\n", timestamp, nbTimeouts);
                fflush(Logfile);
#endif
                nErr = COMMAND_NORMAL_TIMEOUT_FOC;
                break;
            }
            m_pSleeper->sleep(MAX_READ_WAIT_TIMEOUT_FOC);
            continue;
        }
        nbTimeouts = 0;
        if(ulTotalBytesRead + nBytesWaiting <= SERIAL_BUFFER_SIZE_FOC)
            nErr = m_pSerx->readFile(pszBufPtr, nBytesWaiting, ulBytesRead, nTimeout);
        else {
            nErr = ERR_RXTIMEOUT;
            break; // buffer is full.. there is a problem !!
        }
        if(nErr) {
#if defined MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CMFDeluxeController::readResponse] readFile error.\n", timestamp);
            fflush(Logfile);
#endif
            return nErr;
        }

        if (ulBytesRead != nBytesWaiting) { // timeout
#if defined MFDeluxe_DEBUG && MFDeluxe_DEBUG >= 2
            ltime = time(NULL);
            timestamp = asctime(localtime(&ltime));
            timestamp[strlen(timestamp) - 1] = 0;
            fprintf(Logfile, "[%s] [CMFDeluxeController::readResponse] readFile Timeout Error\n", timestamp);
            fprintf(Logfile, "[%s] [CMFDeluxeController::readResponse] readFile nBytesWaiting = %d\n", timestamp, nBytesWaiting);
            fprintf(Logfile, "[%s] [CMFDeluxeController::readResponse] readFile ulBytesRead = %lu\n", timestamp, ulBytesRead);
            fflush(Logfile);
#endif
        }

        ulTotalBytesRead += ulBytesRead;
        pszBufPtr+=ulBytesRead;
    } while (ulTotalBytesRead < SERIAL_BUFFER_SIZE_FOC);

    if(!ulTotalBytesRead)
        nErr = ERR_RXTIMEOUT; // the MFD doesn't have a terminating char so it's timeout based.

    sResp.assign(pszBuf);
    return nErr;
}

int CMFDeluxeController::parseFields(const std::string sResp, std::vector<std::string> &svFields, char cSeparator)
{
    int nErr = MFDeluxe_OK;
    std::string sSegment;

    if(!sResp.size())
        return PARSE_FAILED;

    std::stringstream ssTmp(sResp);

    svFields.clear();
    // split the string into vector elements
    while(std::getline(ssTmp, sSegment, cSeparator))
    {
#if defined PLUGIN_DEBUG && PLUGIN_DEBUG >= 3
        ltime = time(NULL);
        timestamp = asctime(localtime(&ltime));
        timestamp[strlen(timestamp) - 1] = 0;
        fprintf(Logfile, "[%s] [CAAG::parseFields] sSegment : %s\n", timestamp, sSegment.c_str());
        fflush(Logfile);
#endif
        svFields.push_back(trim(sSegment, " \n\r"));
    }

    if(svFields.size()==0) {
        nErr = ERR_CMDFAILED;
    }
    return nErr;
}


std::string& CMFDeluxeController::trim(std::string &str, const std::string& filter )
{
    return ltrim(rtrim(str, filter), filter);
}

std::string& CMFDeluxeController::ltrim(std::string& str, const std::string& filter)
{
    str.erase(0, str.find_first_not_of(filter));
    return str;
}

std::string& CMFDeluxeController::rtrim(std::string& str, const std::string& filter)
{
    str.erase(str.find_last_not_of(filter) + 1);
    return str;
}

std::string CMFDeluxeController::findField(std::vector<std::string> &svFields, const std::string& token)
{
    for(int i=0; i<svFields.size(); i++){
        if(svFields[i].find(token)!= -1) {
            return svFields[i];
        }
    }
    return std::string();
}


