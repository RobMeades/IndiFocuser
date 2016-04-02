/* This file implements Rob's focuser for Indi
 * It uses one of these boards:
 *
 * https://www.pololu.com/product/713
 *
 * ...containing one of these chips:
 *
 * http://toshiba.semicon-storage.com/info/docget.jsp?did=10660&prodName=TB6612FNG
 *
 * ...to drive a High Res Stepper Motor kits (Model STM) from Moonlight Focusers.
 *
 * This code is based on the Indi focuser example:
 *
 * libindi/drivers/focuser/focus_simulator.cpp
 */

#include "focuser_rob.h"

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <memory>
 
/**************************************************************************************
 * DEFINES
 **************************************************************************************/

/**************************************************************************************
 * PRIVATE VARIABLES
 **************************************************************************************/

std::unique_ptr<FocuserRob>  focuserRob(new FocuserRob());

/**************************************************************************************
 * FUNCTIONS OF BASE DEVICE (PUBLIC)
 **************************************************************************************/

/* Return properties of focuser */
void ISGetProperties (const char *dev)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISGetProperties(dev);
}

/* Process new switch from client */
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISNewSwitch(dev, name, states, names, n);
}

/* Process new text from client */
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISNewText(dev, name, texts, names, n);
}

/* Process new number from client */
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISNewNumber(dev, name, values, names, n);
}

/* Process new blob from client: not used in any focuser, so just stub it */
void ISNewBLOB (const char *dev, const char *name, int sizes[], int blobsizes[], char *blobs[], char *formats[], char *names[], int n)
{
    INDI_UNUSED(dev);
    INDI_UNUSED(name);
    INDI_UNUSED(sizes);
    INDI_UNUSED(blobsizes);
    INDI_UNUSED(blobs);
    INDI_UNUSED(formats);
    INDI_UNUSED(names);
    INDI_UNUSED(n);
}

/* Process snooped property from another driver */
void ISSnoopDevice (XMLEle *root)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISSnoopDevice(root);
}

/**************************************************************************************
 * FUNCTIONS OF FOCUSER (PROTECTED)
 **************************************************************************************/

/* Constructor */
FocuserRob::FocuserRob()
{
    initTicks = 0;
    SetFocuserCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE);
}

/* Destructor */
FocuserRob::~FocuserRob()
{
}

/* Client is asking us to establish connection to the focuser */
bool FocuserRob::Connect()
{
    SetTimer(1000);
    IDMessage(getDeviceName(), "Connected.");
    return true;
}

/* Client is asking us to terminate connection to the focuser */
bool FocuserRob::Disconnect()
{
    IDMessage(getDeviceName(), "Disconnected.");
    return true;
}

/* INDI is asking us for our default device name */
const char * FocuserRob::getDefaultName()
{
	/* "Rob Focuser" rather than "Focuser Rob" as the latter gets confused with "Focuser Simulator" */
    return "Rob Focuser";
}

/* Initialise properties */
bool FocuserRob::initProperties()
{
    INDI::Focuser::initProperties();

    ticks = initTicks;    
    FocusAbsPosN[0].value = FocusAbsPosN[0].max / 2;    

    return true;
}

/* Update properties */
bool FocuserRob::updateProperties()
{
    INDI::Focuser::updateProperties();
    return true;
}

/* Handle timer expiry */
void FocuserRob::TimerHit()
{
    int nextTimer = 1000;

    if (isConnected())
    {
        SetTimer(nextTimer);
    }
}

/* Handle a request to move the focuser */
IPState FocuserRob::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    IPState returnState = IPS_ALERT;

    double targetTicks = (speed * duration) / (FocusSpeedN[0].max * FocusTimerN[0].max);
    double plannedTicks = ticks;
    double plannedAbsPos = 0;

    if (dir == FOCUS_INWARD)
    {
        plannedTicks -= targetTicks;
    }
    else
    {
        plannedTicks += targetTicks;
    }
	
    if (isDebug())
    {
        IDLog("Current ticks: %g - target Ticks: %g, plannedTicks %g.\n", ticks, targetTicks, plannedTicks);
    }
	
    /* TODO: 5000? */
    plannedAbsPos = (plannedTicks - initTicks) * 5000 + (FocusAbsPosN[0].max - FocusAbsPosN[0].min) / 2;

    if (plannedAbsPos < FocusAbsPosN[0].min || plannedAbsPos > FocusAbsPosN[0].max)
    {
        IDMessage(getDeviceName(), "Error, requested position is out of range.");
    }
    else
    {
        /* TODO: actually move */
        IDMessage(getDeviceName() , "TODO MoveFocuser().");

        ticks = plannedTicks;
        if (isDebug())
        {
            IDLog("Current absolute position: %g, current ticks is %g.\n", plannedAbsPos, ticks);
        }

        FocusAbsPosN[0].value = plannedAbsPos;

        IDSetNumber(&FocusAbsPosNP, NULL);
		
	returnState = IPS_OK;
    }

    return returnState;
}

/* Handle a request to move the focuser to a given tick count */
IPState FocuserRob::MoveAbsFocuser(uint32_t targetTicks)
{
    IPState returnState = IPS_ALERT;
	
    if (targetTicks < FocusAbsPosN[0].min || targetTicks > FocusAbsPosN[0].max)
    {
        IDMessage(getDeviceName(), "Error, requested position is out of range.");
    }
    else
    {
        double mid = (FocusAbsPosN[0].max - FocusAbsPosN[0].min)/2;

        IDMessage(getDeviceName() , "Moving to requested position...");

        /* Limit to +/- 10 from initTicks */
        ticks = initTicks + (targetTicks - mid) / 5000.0;

        if (isDebug())
        {
            IDLog("Current ticks: %g\n", ticks);
        }
	
        /* TODO: actually move */
        IDMessage(getDeviceName() , "TODO MoveAbsFocuser().");

        FocusAbsPosN[0].value = targetTicks;
		
        returnState = IPS_OK;
    }
	
    return returnState;
}

/* Handle a request to move the focuser by a number of ticks */
IPState FocuserRob::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t targetTicks = FocusAbsPosN[0].value + (ticks * (dir == FOCUS_INWARD ? -1 : 1));

    FocusAbsPosNP.s = IPS_BUSY;
    IDSetNumber(&FocusAbsPosNP, NULL);

    return MoveAbsFocuser(targetTicks);
}

/* End of file */
