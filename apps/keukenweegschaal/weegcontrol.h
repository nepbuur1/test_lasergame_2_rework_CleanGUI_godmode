
class WeegControl{
    private:
        Timer timer_half_sec;
        Queue<string,5> queueKnopIngedrukt;
        Queue<string,5> queueKnopLosgelaten;
        Flag flagGewichtGemeten;
        Flag flagDrieSecKnop;

    public:
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
        enum State {WACHT_OP_AANKNOP_3SEC_INGEDRUKT, DISPLAY};
        State state = State::WACHT_OP_AANKNOP_3SEC_INGEDRUKT;

        string strEenheid = persistentMemory.getvalue("eenheid");
        if(strEenheid == "")
        {
            strEenheid = "gram";
            persistentMemory.setValue("eenheid", strEenheid);
        }

        bool nextTimeDoEntryEvent = true;
        bool thisTimeDoEntryEvent = true;

        while(true)
        {
            nextTimeDoEntryEvent = false;
            switch(state):
            {
                case State::WACHT_OP_AANKNOP_3SEC_INGEDRUKT:
                    wait(flagDrieSecondenKnop);
                    display.enable();
                    tarra = poolGewicht.read();

                    nextTimeDoEntryEvent = true;
                    state = State::DISPLAY;
                break;

                case State::DISPLAY:
                    if (thisTimeDoEntryEvent)
                    {
                        display.showString(0,0,determineWeightString
                                (strEenheid,poolGewicht.read()-tarra));
                        timer_half_sec.start(500000);
                    }
                    wait_any(timer_half_sec + flagDrieSecondenKnop 
                             + queueKnopIngedrukt);
                    if(HasFired(timer_half_sec))
                    {
                        nextTimeDoEntryEvent = true;
                        state = State::DISPLAY; // terug naar dezelfde toestand
                    }
                    else if (HasFired(flagDrieSecondenKnop))
                    {
                        display.disable();

                        nextTimeDoEntryEvent = true;
                        state = State::WACHT_OP_AANKNOP_3SEC_INGEDRUKT;
                    }
                    else if (HasFired(queueKnopIngedrukt))
                    {
                        knopIngedruktNaam = queueKnopIngedrukt.read();
                        if(knopIngedruktNaam=="tarraKnop")
                        {
                            tarra = poolGewicht.read();
                            nextTimeDoEntryEvent = true;
                            state = State::DISPLAY; // terug naar dezelfde toestand
                        }
                        else if (knopIngedruktNaam=="eenheidKnop")
                        {
                           strEenheid = toggleEenheid(strEenheid);
                           persistentMemory.setValue("eenheid", strEenheid);
                           nextTimeDoEntryEvent = true;
                           state = State::DISPLAY; // terug naar dezelfde toestand
                        }
                        else
                        {
                            nextTimeDoEntryEvent = false;
                            state = State::DISPLAY; // blijf in dezelfde toestand
                        }
                    }
                    else
                    {
                        assert(0); // ben je een if statement vergeten?
                    }
                break;

                default:
                    assert(0);
                break;
            }

            thisTimeDoEntryEvent = nextTimeDoEntryEvent;
        }
    }
}