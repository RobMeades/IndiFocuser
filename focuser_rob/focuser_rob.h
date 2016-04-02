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
 *
 * The code is based on:
 *
 * libindi/drivers/focuser/focus_simulator.h
 *
 */

#ifndef FOCUSER_ROB_H
#define FOCUSER_ROB_H

#include "indifocuser.h"

class FocuserRob : public INDI::Focuser
{
    protected:
    private:

        double ticks;
        double initTicks;

    public:
        FocuserRob();
        virtual ~FocuserRob();

        const char *getDefaultName();

        bool initProperties();
        bool updateProperties();

        bool Connect();
        bool Disconnect();

        void TimerHit();

        virtual IPState MoveFocuser(FocusDirection dir, int speed, uint16_t duration);
        virtual IPState MoveAbsFocuser(uint32_t ticks);
        virtual IPState MoveRelFocuser(FocusDirection dir, uint32_t ticks);
};

#endif