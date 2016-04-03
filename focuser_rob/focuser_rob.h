/* This file implements the indi interfaces for
 * Rob's focuser, which uses one of these boards:
 *
 * https://www.pololu.com/product/713
 *
 * ...containing one of these chips:
 *
 * http://toshiba.semicon-storage.com/info/docget.jsp?did=10660&prodName=TB6612FNG
 *
 * ...to drive a High Res Stepper Motor kits (Model STM) from Moonlight Focusers.
 */

#ifndef FOCUSER_ROB_H
#define FOCUSER_ROB_H

#include "indifocuser.h"

class FocuserRob : public INDI::Focuser
{
    private:
    protected:

        uint32_t gTicksElapsed;
        uint16_t gPollTimerMs;
        uint32_t gTicksRequired;
        bool gDirectionIsOutward;

        void oneTick(uint16_t highTimeMs);
        void setDirection(bool isOutward);
        void setShortBreak();
        void setStop();
        void setStandby(bool isOn);
        void setVariablesAfterMove(int32_t relativeTicks);
		IPState move(int32_t ticks);

    public:
        FocuserRob();
        virtual ~FocuserRob();

        const char *getDefaultName();

        bool initProperties();
        bool updateProperties();

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual bool SetFocuserSpeed(int speed);
        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
        virtual IPState MoveAbsFocuser(uint32_t ticks);
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
        virtual bool AbortFocuser();
};

#endif