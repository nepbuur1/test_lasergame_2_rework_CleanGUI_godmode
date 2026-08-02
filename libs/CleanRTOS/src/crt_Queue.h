// by Marius Versteegen, 2023

// A Queue is a waitable. It is meant for inter task communications.
// The task that owns the queue should wait for another task to put something into it.
// (see the Queue example in the examples folder)

#pragma once
#include "internals/crt_FreeRTOS.h"
#include "crt_Waitable.h"
#include "crt_Task.h"
#include "crt_ILogger.h"
namespace crt
{
    //extern ILogger& logger;

	template<typename TYPE, uint32_t COUNT> class Queue : public Waitable
	{
	private:
		QueueHandle_t qh;
        Task* pTask;
        TickType_t writeDelay;
		TYPE dummy;

	public:
		Queue(Task* pTask,bool bWriteWaitIfQueueFull=false):Waitable(WaitableType::wt_Queue),pTask(pTask),
            writeDelay(bWriteWaitIfQueueFull ? portMAX_DELAY : 0)
		{
            Waitable::init(pTask->queryBitNumber(this));
			qh = xQueueCreate(COUNT, sizeof(TYPE));
		}
		
		///*override*/ bool operator==(const Waitable& other) const { return this == &other; };

		// Blokkeert tot er iets in de queue staat. Prima als de taak niets anders
		// te doen heeft dan wachten op deze queue.
		//
		// LET OP: gebruik deze functie NIET na een waitAny()/hasFired() op deze
		// queue - gebruik dan tryRead(). Het event-bit is namelijk een belletje
		// dat los van de queue wordt bijgehouden, en het kan aan blijven staan
		// terwijl de queue al leeg is (zie write()). Wie dat belletje gelooft en
		// hier binnenloopt, blijft voorgoed staan wachten op iets dat er niet
		// meer is - en dat ziet er van buitenaf uit als een taak die zomaar dood
		// is gegaan.
		void read(TYPE& returnVariable)
		{
			BaseType_t rc = xQueueReceive(qh, &returnVariable, portMAX_DELAY);
			syncEventBit();
			assert(rc == pdPASS);
		}

		// Leest zonder ooit te blokkeren; geeft false als de queue leeg was.
		// Dit is de veilige tegenhanger van read() voor een taak die met
		// waitAny() op meerdere dingen tegelijk wacht: het event-bit is dan een
		// aanwijzing, geen belofte, en de queue zelf is de enige waarheid.
		bool tryRead(TYPE& returnVariable)
		{
			bool bGotOne = (xQueueReceive(qh, &returnVariable, 0) == pdPASS);
			syncEventBit();		// ook als er niets stond: dan stond het bit ten onrechte aan.
			return bGotOne;
		}

		int getNofMessagesWaiting()
		{
			return uxQueueMessagesWaiting(qh);
		}

		bool write(TYPE& variableToCopy)
		{
			BaseType_t rc = xQueueSend(qh, &variableToCopy, writeDelay);
            if (rc != pdPASS)
            {
                // The queue got full. Note: that cannot happen if bWriteWaitIfQueueFull==true.
                return false;
            }
#ifdef CRT_QUEUE_WIDEN_DOORBELL_RACE
            // TIJDELIJK - diagnose, geen fix. Zet het venster open dat hieronder
            // sowieso al bestaat: het item staat na xQueueSend al in de queue,
            // maar de lezer weet dat pas na setEventBits. Wordt de schrijver daar
            // precies tussenin weggedrukt en leegt de lezer de queue intussen
            // helemaal, dan zet de regel hieronder alsnog een bit waar niets meer
            // achter zit - en blijft de lezer in read() op een lege queue hangen.
            //
            // Deze taskYIELD maakt dat venster expres groot. Klapt het systeem
            // daarmee vrijwel meteen om, dan is het mechanisme daarmee bewezen.
            taskYIELD();
#endif
            pTask->setEventBits(Waitable::getBitMask());
            return true;
		}

		void clear()
		{
			while (uxQueueMessagesWaiting(qh) > 0)
			{
				xQueueReceive(qh, &dummy, portMAX_DELAY);
			}
			
			pTask->clearEventBits(Waitable::getBitMask());
		}

	private:
		// Brengt het event-bit in overeenstemming met wat er werkelijk in de
		// queue staat. Alleen de eigenaar-taak roept dit aan, vanuit read() /
		// tryRead().
		//
		// De dubbele controle is geen overdaad. Tussen het vaststellen dat de
		// queue leeg is en het wissen van het bit kan een schrijver er alsnog
		// iets in zetten. Die schrijver zet daarna zelf ook het bit, maar dat kan
		// net voor ons wissen gebeurd zijn - dan zouden we zijn belletje
		// wegpoetsen en zou zijn commando blijven liggen tot er toevallig een
		// volgende schrijver langskomt. Na het wissen dus nog een keer kijken:
		// wat er dan staat, is van na het wissen, en daar hoort het bit bij.
		void syncEventBit()
		{
			if (uxQueueMessagesWaiting(qh) > 0)
			{
				pTask->setEventBits(Waitable::getBitMask());
				return;
			}

			pTask->clearEventBits(Waitable::getBitMask());

			if (uxQueueMessagesWaiting(qh) > 0)
			{
				pTask->setEventBits(Waitable::getBitMask());
			}
		}
	};
};

