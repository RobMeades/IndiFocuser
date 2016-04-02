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
#include <wiringX.h>

/**************************************************************************************
 * DEFINES
 **************************************************************************************/

/* Courtesy of a Mr timglowforge at this Solid Run forum post:
 *
 * http://forum.solid-run.com/linux-kernel-and-bootloaders-on-cubox-i-and-hummin-f10/how-to-control-hummingboard-gpio-from-kernel-space-t2345.html
 *
 * ...and http://wiringx.org/, the GPIO mapping of header pin number for the
 * Hummingboard (HB#) to wiringX number (WiringX#) is as follows:
 * 
 * HB# HB Net    MicroSOM Net   i.MX6 Pad   GPIO Signal  Mode  Bank  Pin  Linux# WiringX# 
 * 1   3.2V
 * 2   5.OV
 * 3   SDA1      I2C3_SDA       EIM_D18     GPIO3_I018   ALT5   3    18   82
 * 4   5.0V
 * 5   SCL1      I2C3_SCL       EIM_D17     GPIC3_I017   ALT5   3    17   81
 * 6   GND
 * 7   GPIO_GCLK USB_OTG_ID     GPIO_1      GPIO1_IO01   ALT5   1    1    1      7
 * 8   TXD0      UART1_TX_DATA  CSIO_DAT10  GPIO5_IO28   ALT5   5    28   156
 * 9   GND
 * 10  RXD0      UART1_RX_DATA  CSIO_DAT11  GPIO5_IO29   ALT5   5    29   157
 * 11  GPIO_GEN0 DISP1_DATA00   EIM_DA9     GPIO3_IO09   ALT5   3    9    73     0
 * 12  GPIO_GEN1 DISP1_DATA01   EIM_DA8     GPIO3_IO08   ALT5   3    8    72     1
 * 13  GPIO_GEN2 DISP1_DATA02   EIM_DA7     GPIO3_IO07   ALT5   3    7    71     2
 * 14  GND
 * 15  GPIO_GEN3 DISP1_DATA03   EIM_DA6     GPIO3_IO06   ALT5   3    6    70     3
 * 16  GPIO_GEN4 SD3_CMD        SD3_CMD     GPIO7_IO02   ALT5   7    2    194    4
 * 17  3.2V
 * 18  GPIO_GEN5 SD3_CLK        SD3_CLK     GPIO7_IO03   ALT5   7    3    195    5
 * 19  SPI_MOSI  ECSPI2_MOSI    EIM_CS1     GPIO2_IO24   ALT5   2    24   56
 * 20  GND
 * 21  SPI_MOSO  ECSPI2_MOSO    EIM_OE      GPIO2_IO25   ALT5   2    25   57
 * 22  GPIO_GEN6 DISP1_DATA06   EIM_DA3     GPIO3_IO03   ALT5   3    3    67     6
 * 23  SPI_SCLK  ECSPI2_SCLK    EIM_CS0     GPIO2_IO23   ALT5   2    23   55
 * 24  SPI_CE0_N ECSPI2_SS0     EIM_RW      GPIO2_IO26   ALT5   2    26   58
 * 25  GND
 * 26  SPI_CE1_N ECSPI2_SS1     EIM_LBA     GPIO2_IO27   ALT5   2    27   59
 */

/* From the TB6612FNG documentation:
 *
 *        Input                       Output
 * IN1  IN2  PWM  STBY          OUT1  OUT2    Mode
 *  1    1   ---   1             0    0    Short brake
 *  0    1    1    1             0    1    Counter-clockwise
 *  0    1    0    1             0    0    Short brake
 *  1    0    1    1             1    0    Clockwise
 *  1    0    0    1             0    0    Short brake
 *  0    0    1    1               Off     Stop
 * ---  ---  ---   0               Off     Standby 
 */
 
#define IN1_TB6612FNG    0 /* GPIO0, header pin 11 */
#define IN2_TB6612FNG    1 /* GPIO1, header pin 12 */
#define PWM_TB6612FNG    2 /* GPIO2, header pin 13 */
#define STBY_TB6612FNG   3 /* GPIO3, header pin 15 */

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
	
	/* Set up wiringX and pins */
    wiringXSetup();
    pinMode(IN1_TB6612FNG, OUTPUT);
    pinMode(IN2_TB6612FNG, OUTPUT);
    pinMode(PWM_TB6612FNG, OUTPUT);
    pinMode(STBY_TB6612FNG, OUTPUT);

    /* Put the driver chip into standby */
	digitalWrite(STBY_TB6612FNG, LOW);
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
