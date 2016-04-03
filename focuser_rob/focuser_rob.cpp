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

#define SPEED_MAX_TICKS_PER_SECOND 100

#define MIN_POLL_TIMER_MS    10
#define MIN_PWM_HIGH_TIME_MS 1

/**************************************************************************************
 * PRIVATE VARIABLES
 **************************************************************************************/

std::unique_ptr <FocuserRob>  focuserRob(new FocuserRob());

/**************************************************************************************
 * PRIVATE FUNCTIONS OF FOCUSER
 **************************************************************************************/

/* Move by one tick, keeping the PWM output high for the given time. */
void FocuserRob::oneTick(uint16_t highTimeMs)
{
    digitalWrite(PWM_TB6612FNG, HIGH);
    usleep(highTimeMs);
    digitalWrite(PWM_TB6612FNG, LOW);
}
 
/* Set the motor in the correct direction. */
void FocuserRob::setDirection(bool isOutward)
{
    gDirectionIsOutward = false;
    if (isOutward)
    {
        gDirectionIsOutward = true;
        /* Counter clockwise */
        digitalWrite(IN1_TB6612FNG, LOW);
        digitalWrite(IN2_TB6612FNG, HIGH);	
    }
    else
    {
        /* Clockwise */
        digitalWrite(IN1_TB6612FNG, HIGH);
        digitalWrite(IN2_TB6612FNG, LOW);	
    }
}

/* Set a short break.
   Direction must be set up again after doing this. */
void FocuserRob::setShortBreak()
{
    digitalWrite(IN1_TB6612FNG, HIGH);
    digitalWrite(IN2_TB6612FNG, HIGH);	
}

/* Set the motor into stop mode.
   Direction must be set up again after doing this. */
void FocuserRob::setStop()
{
    digitalWrite(PWM_TB6612FNG, LOW);
    digitalWrite(IN1_TB6612FNG, LOW);
    digitalWrite(IN2_TB6612FNG, LOW);	
    digitalWrite(PWM_TB6612FNG, HIGH);
}

/* Set the motor into standby, or take it out.
   Set the motor to "stop" before calling this with
   true and set the direction first before calling
   this with false. */
void FocuserRob::setStandby(bool isOn)
{
    if (isOn)
    {
        digitalWrite(STBY_TB6612FNG, LOW);
    }
    else
    {
        digitalWrite(STBY_TB6612FNG, HIGH);
    }
}

/* Calculate the new position after a move. */
void FocuserRob::setVariablesAfterMove(int32_t relativeTicks)
{
    if (gDirectionIsOutward)
    {
        relativeTicks = -relativeTicks;
    }
    
    FocusAbsPosN[0].value += relativeTicks;
    FocusRelPosN[0].value = relativeTicks;
    IDSetNumber(&FocusAbsPosNP, NULL);
    IDSetNumber(&FocusRelPosNP, NULL);
}

/* Move the focuser by a given number of ticks.
   A positive number indicates inward focus. */
IPState FocuserRob::move(int32_t relativeTicks)
{
    IPState returnState = IPS_ALERT;
    uint16_t delayMs = 1000 / FocusSpeedN[0].value;
	
    if (gTicksRequired != 0)
    {
        /* If we're already doing stuff, stop first */
        AbortFocuser();
    }
    
    IDMessage(getDeviceName(), "Moving to requested position...");

    /* Set the right direction */
    setDirection(relativeTicks < 0);
    
    /* Make relativeTicks absolute */
    if (gDirectionIsOutward)
    {
        relativeTicks = -relativeTicks;
    }
    
    /* Take the driver chip out of standby */
    setStandby(false);
    
    if (delayMs < MIN_POLL_TIMER_MS)
    {
        /* The speed is too high to do on a timer, do the move here */
        for (int32_t x = 0; x < relativeTicks; x++)
        {
            oneTick(MIN_PWM_HIGH_TIME_MS);
            usleep(MIN_POLL_TIMER_MS - MIN_PWM_HIGH_TIME_MS);
        }
        
        setStop();
        setStandby(true);
        
        /* Set things straight after the move */
        setVariablesAfterMove(relativeTicks);
        
        returnState = IPS_OK;
    }
    else
    {
        /* The poll timer will do the move */
        gPollTimerMs = delayMs;
        gTicksRequired = (uint32_t) relativeTicks;
        SetTimer(gPollTimerMs);
        oneTick(MIN_PWM_HIGH_TIME_MS);
        gTicksElapsed = 1;
        
        returnState = IPS_BUSY;
    }
    
    return returnState;
}

/**************************************************************************************
 * PUBLIC FUNCTIONS OF BASE DEVICE
 **************************************************************************************/

/* Return properties of focuser. */
void ISGetProperties (const char *dev)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISGetProperties(dev);
}

/* Process new switch from client. */
void ISNewSwitch (const char *dev, const char *name, ISState *states, char *names[], int n)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISNewSwitch(dev, name, states, names, n);
}

/* Process new text from client. */
void ISNewText (const char *dev, const char *name, char *texts[], char *names[], int n)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISNewText(dev, name, texts, names, n);
}

/* Process new number from client. */
void ISNewNumber (const char *dev, const char *name, double values[], char *names[], int n)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISNewNumber(dev, name, values, names, n);
}

/* Process new blob from client: not used in any focuser, so just stub it. */
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

/* Process snooped property from another driver. */
void ISSnoopDevice (XMLEle *root)
{
    /* This will be handled by the inherited focuser class */
    focuserRob->ISSnoopDevice(root);
}

/**************************************************************************************
 * PUBLIC FUNCTIONS OF FOCUSER
 **************************************************************************************/

/* Constructor. */
FocuserRob::FocuserRob()
{
    gTicksElapsed = 0;
    gTicksRequired = 0;
    gDirectionIsOutward = false;
    SetFocuserCapability(FOCUSER_CAN_ABS_MOVE | FOCUSER_CAN_REL_MOVE | FOCUSER_CAN_ABORT | FOCUSER_HAS_VARIABLE_SPEED);
	
	/* Set up wiringX and pins */
    wiringXSetup();
    pinMode(IN1_TB6612FNG, OUTPUT);
    pinMode(IN2_TB6612FNG, OUTPUT);
    pinMode(PWM_TB6612FNG, OUTPUT);
    pinMode(STBY_TB6612FNG, OUTPUT);

    /* Put the driver chip into standby */
    setStop();
    setStandby(true);
}

/* Destructor. */
FocuserRob::~FocuserRob()
{
}

/* Client is asking us to establish connection to the focuser. */
bool FocuserRob::Connect()
{
    IDMessage(getDeviceName(), "Connected.");
    return true;
}

/* Client is asking us to terminate connection to the focuser. */
bool FocuserRob::Disconnect()
{
    AbortFocuser();
    IDMessage(getDeviceName(), "Disconnected.");
    return true;
}

/* INDI is asking us for our default device name. */
const char * FocuserRob::getDefaultName()
{
	/* "Rob Focuser" rather than "Focuser Rob" as the latter gets confused with "Focuser Simulator" */
    return "Rob Focuser";
}

/* Initialise properties. */
bool FocuserRob::initProperties()
{
    INDI::Focuser::initProperties();

    /* Speed is in ticks per second */
	FocusSpeedN[0].min = 1;
    FocusSpeedN[0].max = 255;
    FocusSpeedN[0].value = 100;    

    /* Position is in ticks */
    FocusAbsPosN[0].min = 0.0;
    FocusAbsPosN[0].max = 60000.0;
    FocusAbsPosN[0].value = (FocusAbsPosN[0].min + FocusAbsPosN[0].max) / 2;
    FocusAbsPosN[0].step = 1;
	
    FocusRelPosN[0].min = 0.0;
    FocusRelPosN[0].max = (FocusAbsPosN[0].min + FocusAbsPosN[0].max) / 2;
    FocusRelPosN[0].value = 0;
    FocusRelPosN[0].step = 1;

    return true;
}

/* Update properties. */
bool FocuserRob::updateProperties()
{
    INDI::Focuser::updateProperties();
    return true;
}

/* Set speed in ticks per millisecond. */
bool FocuserRob::SetFocuserSpeed(int speed)
{
	bool success = false;
	
    if (speed != (int) FocusSpeedN[0].value)
    {
        if ((FocusTimerNP.s == IPS_BUSY) || (FocusAbsPosNP.s == IPS_BUSY) || (FocusRelPosNP.s == IPS_BUSY))
        {
            IDMessage(getDeviceName(), "Can't set the speed while the motor is running.");
        }
        else
        {
            if ((speed < FocusSpeedN[0].min) || (speed > FocusSpeedN[0].max))
            {
                IDMessage(getDeviceName(), "Error, requested speed is out of range.");
            }
            else
            {
                FocusSpeedN[0].value = speed;
                FocusSpeedNP.s = IPS_OK;
                IDSetNumber(&FocusSpeedNP, "Speed set.");
                success = true;
            }
        }
    }
    else
    {
        success = true;
    }
    
    return success;
}

/* Handle timer expiry - used to tick along the motor if the
   speed is low enough. */
void FocuserRob::TimerHit()
{
    if (isConnected())
    {
        /* Only do stuff if we're connected */        
        if (gTicksRequired > 0)
        {
            if (gTicksElapsed < gTicksRequired)
            {
                /* Set the next timer and do a tick */
                SetTimer(gPollTimerMs);
                oneTick(MIN_PWM_HIGH_TIME_MS);
                gTicksElapsed++;
            }
            else
            {
                /* Done enough, abort now */
                AbortFocuser();
            }
        }        
    }
}

/* Handle a request to move the focuser at a given speed (in ticks per second)
   for a given duration (in ms). */
IPState FocuserRob::MoveFocuser(FocusDirection dir, int speed, uint16_t duration)
{
    IPState returnState = IPS_ALERT;

    if (SetFocuserSpeed(speed))
	{
        int32_t relativeTicks = (speed * duration) / 1000; /* 1000 'cos duration is in milliseconds */
		
        if (dir == FOCUS_OUTWARD)
        {
            relativeTicks -= relativeTicks;
        }
        
        double plannedAbsPos = FocusAbsPosN[0].value + relativeTicks;
	
        if ((plannedAbsPos < FocusAbsPosN[0].min) || (plannedAbsPos > FocusAbsPosN[0].max))
        {
            IDMessage(getDeviceName(), "Error, requested position is out of range.");
        }
        else
        {
            /* Move */
            returnState = move(relativeTicks);
        }
    }

    return returnState;
}

/* Handle a request to move the focuser to a given tick count. */
IPState FocuserRob::MoveAbsFocuser(uint32_t targetTicks)
{
    IPState returnState = IPS_ALERT;
	
    if (targetTicks != FocusAbsPosN[0].value)
    {
        if ((targetTicks < FocusAbsPosN[0].min) || (targetTicks > FocusAbsPosN[0].max))
        {
            IDMessage(getDeviceName(), "Error, requested position is out of range.");
        }
        else
        {
            int32_t relativeTicks = targetTicks - FocusAbsPosN[0].value;
            
            /* Move */
            returnState = move(relativeTicks);
        }
    }
	
    return returnState;
}

/* Handle a request to move the focuser by a number of ticks. */
IPState FocuserRob::MoveRelFocuser(FocusDirection dir, uint32_t ticks)
{
    uint32_t targetTicks = FocusAbsPosN[0].value + (ticks * (dir == FOCUS_INWARD ? -1 : 1));
    
    /* Let the absolute move function do the work */
    return MoveAbsFocuser(targetTicks);
}

/* Stop the focuser. */
bool FocuserRob::AbortFocuser()
{
    /* Stop the motor */
    setStop();
    setStandby(true);

    /* If we were moving, setup the variables for how far we got */
    if (gTicksRequired > 0)
    {
        setVariablesAfterMove(gTicksRequired - gTicksElapsed);
        gTicksRequired = 0;
    }
	
	FocusTimerNP.s = IPS_IDLE;
	FocusAbsPosNP.s = IPS_IDLE;
	FocusRelPosNP.s = IPS_IDLE;
	IDSetNumber(&FocusTimerNP, NULL);
	IDSetNumber(&FocusAbsPosNP, NULL);
	IDSetNumber(&FocusRelPosNP, NULL);
	
    return true;
}

/* End of file */
