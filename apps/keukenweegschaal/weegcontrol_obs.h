


class WeegControl : public Task, public IKnopListener
{
    Pool<int> poolGewicht;
-   Flag flagGewichtGemeten;
-   Flag flagDrieSecKnop;
-   Queue<string, 5> queueKnopIngedrukt;
-   Queue<string, 5> queueKnopLosgelaten;
    Timer timer_halve_seconde;
-   Display display;
    int tarra;

    public:
        WeegControl() : flagGewichtGemeten(this), flagDrieSecKnop(this), 
        queueKnopIngedrukt(this), queueKnopLosgelaten(this), 
        timer_halve_seconde(this), display(this)
        {
            display.disable();
        }

        void ingedrukt(string knopNaam)
        {
            queueKnopIngedrukt.write(knopNaam);
        } 
        
        void losgelaten(knopNaam:string)
        {
            queueKnopLosgelaten.write(knopNaam);
        }

        void gewichtGemeten(sensorNaam:string, gewicht:int)
        {
            flagGewichtGemeten.set(); 
            poolGewicht.write(gewicht);
        }

        void knopIngedruktGedurendeDrieSeconden(knopNaam:string)
        {
            flagDrieSecKnop.set();
        }

private:
    void main()
    {
        enum State {WACHT_OP_AANKNOP_3SEC_INGEDRUKT,DISPLAY};
        State state = WACHT_OP_AANKNOP_3SEC_INGEDRUKT;
        State prevState = state;

        string strEenheid = persistentMemory.getValue("eenheid")
        int tarra = 0;
        if(strEenheid == "")
        {
            strEenheid = "gram";
            persistentMemory.setValue("eenheid", strEenheid);
        }

        bool nextTimeDoEntryEvent = false;
        bool thisTimeDoEntryEvent = true;

        while(true)
        {
            nextTimeDoEntryEvent = false;

            switch(state)
            {
                case State::WACHT_OP_AANKNOP_3SEC_INGEDRUKT:
                {
                    wait(flagDrieSecKnop);
                    display.enable();
                    tarra = poolGewicht.read();

                    nextTimeDoEntryEvent = true;
                    state = State::DISPLAY;
                    break;
                }

                case State::DISPLAY:
                {
                    if(thisTimeDoEntryEvent)
                    {
                        display.showString(0,0,determineWeightString(strEenheid,poolGewicht.read()-tarra))
                        timer_halve_seconde.start(500000);
                    }
                    wait_any(timer_halve_seconde, queueKnopIngedrukt, flagDrieSecKnop);
                    if(HasFired(time_halve_seconde))
                    {
                        // doe niets, ga terug naar de start van de toestand.
                    }
                    else if (HasFired(queueKnopIngedrukt))
                    {
                        string knopIngedruktNaam=queueKnopIngedrukt.read();
                        if(knopIngedruktNaam == "tarraKnop")
                        {
                            tarra = poolGewicht.read();
                            // doe verder niets, ga terug naar de start van de toestand.
                        }
                        else if(knopIngedruktNaam == "eenheidKnop")
                        {
                            assert(knopIngedruktNaam == "eenheidKnop");
                            strEenheid = toggleEenheid(strEenheid);
                            persistentMemory.setValue("eenheid", strEenheid);
                        }
                        else
                        {
                            // andere knop ingedrukt: blijf in de toestand
                            nextTimeDoEntryEvent = false
                        }
                    }
                    else if (HasFired(flagDrieSecKnop))
                    {
                        display.disable()
                        nextTimeDoEntryEvent = true;
                        state = State::WACHT_OP_AANKNOP_3SEC_INGEDRUKT;
                    }
                    break;
                }

                default:
                {
                    assert(false);
                }
            }
            thisTimeDoEntryEvent = nextTimeDoEntryEvent;
        }
    }
}