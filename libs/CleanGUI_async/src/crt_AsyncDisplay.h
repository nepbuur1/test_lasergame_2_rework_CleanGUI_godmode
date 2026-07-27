// by Marius Versteegen, 2023
#pragma once
#include <crt_CleanRTOS.h>
#include "crt_typesAndConstants.h"
#include <crt_Vec2.h>
#include <crt_IDisplay.h>
#include <crt_DisplayCommand.h>
#include <crt_PrintContext.h>

namespace crt
{
	extern ILogger& logger;

	// Via AsyncDisplay, multiple threads can asynchronously send commands to a (shared) display.

	template <size_t CommandBufferSize, size_t MaxNofClientTasks>
	class AsyncDisplay : public Task, public IDisplay
	{
	private:
		IDisplay& display;
		PrintContext arPrintContext[MaxNofClientTasks];
		int nofPrintContexts;
		Queue<DisplayCommand, CommandBufferSize> queueCommands;
		Timer periodicTimer;
		Flag flagTouchCalibrate;
		DisplayCommand curCommand;

		// Hoeveel tekencommando's per waitAny-ronde worden afgehandeld. Een op
		// een is verspilling zodra er een reeks klaarstaat: de waitAny-cyclus
		// kost dan meer dan het commando zelf. Gemeten op een volledige
		// keyboard-hertekening: 95 -> 85 ms, dus ruim 10%. Begrensd, zodat
		// pollTouch niet te lang moet wachten.
		static const int MaxNofCommandsPerCycle = 10;
		uint32_t nofQueueFullEvents;	// alleen ter diagnose; er gaat niets meer verloren
		uint8_t rotation;
		uint8_t fontTouchCalibration;

	public:
		AsyncDisplay(const char* taskName, unsigned int taskPriority, unsigned int taskSizeBytes, unsigned int taskCoreNumber, 
			IDisplay& display, uint8_t rotation, uint8_t fontTouchCalibration) :
			Task(taskName, taskPriority, taskSizeBytes, taskCoreNumber), display(display), nofPrintContexts(0), queueCommands(this, true /*bWriteWaitIfQueueFull*/), 
			periodicTimer(this), flagTouchCalibrate(this), nofQueueFullEvents(0), rotation(rotation), fontTouchCalibration(fontTouchCalibration)
		{
			// Meteen doorgeven, niet pas in main(): PageRoot vraagt de schermmaat
			// op vanuit een andere taak en zou anders de portretmaat krijgen.
			// Dat was precies de race waar de layout op leunde.
			display.setRotation(rotation);

			start(); // For simplicity, the task is started right away in it's constructor.
		}

	private:
		void main()
		{
			vTaskDelay(1000); // wait for other threads to have started up as well.
			periodicTimer.start_periodic(50000); // 50ms Polling period for touch handling.

			PrintContext* pPrintContext = nullptr;

			display.touchCalibrate(rotation, fontTouchCalibration);

			while (true)
			{
				dumpStackHighWaterMarkIfIncreased(); 		// This function call takes about 0.25ms! It should be called while debugging only.

				waitAny(queueCommands+periodicTimer+flagTouchCalibrate);
				
				if (hasFired(queueCommands))
				{
				  for (int nofHandled = 0; nofHandled < MaxNofCommandsPerCycle; nofHandled++)
				  {
					queueCommands.read(curCommand);

					// curCommand.dumpWithESP_LOGI(); vTaskDelay(1); // for debugging purposes.

					pPrintContext = queryPrintContext(curCommand.getTask());
					assert(pPrintContext != nullptr);
#ifdef CRT_ASYNCDISPLAY_COMMAND_TIMING
					// Diagnostic, off unless CRT_ASYNCDISPLAY_COMMAND_TIMING is defined
					// before including this header: logs every executed drawing
					// command with its duration, which makes the sequence per redraw
					// visible. Beware that the logging itself costs bus time (~9 ms
					// per line at 115200 baud), so it distorts what it measures if
					// commands come in fast.
					int64_t timeBeforeUs = esp_timer_get_time();
					curCommand.execute(display, *pPrintContext);
					int64_t timeAfterUs = esp_timer_get_time();
					ESP_LOGI("cmd", "%-20s %6d us  (t=%d)",
						commandTypeName(curCommand.getCommandType()),
						(int)(timeAfterUs - timeBeforeUs), (int)(timeAfterUs / 1000));
#else
					curCommand.execute(display, *pPrintContext);
#endif
					if (queueCommands.getNofMessagesWaiting() == 0)
					{
						break;		// niets meer klaar; terug naar waitAny.
					}
				  }
				}

				if (hasFired(periodicTimer))
				{
					display.pollTouch(this);
				}
			}
		}

#ifdef CRT_ASYNCDISPLAY_COMMAND_TIMING
	private:
		static const char* commandTypeName(DisplayCommandType type)
		{
			switch (type)
			{
			case DisplayCommandType::fillScreen:          return "fillScreen";
			case DisplayCommandType::drawRect:            return "drawRect";
			case DisplayCommandType::fillRect:            return "fillRect";
			case DisplayCommandType::drawRoundRect:       return "drawRoundRect";
			case DisplayCommandType::fillRoundRect:       return "fillRoundRect";
			case DisplayCommandType::fillSmoothRoundRect: return "fillSmoothRoundRect";
			case DisplayCommandType::drawString:          return "drawString";
			case DisplayCommandType::drawStringOpaque:    return "drawStringOpaque";
			case DisplayCommandType::fillBeveledRect:     return "fillBeveledRect";
			case DisplayCommandType::setClipRect:         return "setClipRect";
			case DisplayCommandType::clearClipRect:       return "clearClipRect";
			case DisplayCommandType::drawImage:           return "drawImage";
			case DisplayCommandType::print:               return "print";
			case DisplayCommandType::println:             return "println";
			default:                                      return "(other)";
			}
		}
#endif

	private:
		// Queue::write blokkeert niet als de queue vol is - hij geeft false terug
		// en het commando is weg. Dat mag hier niet gebeuren: sneuvelt er een
		// clearClipRect, dan blijft de clip staan en verandert er daarna niets
		// meer op het scherm. Dat ziet eruit als vastlopen, terwijl alles nog
		// gewoon draait. Wachten tot er plaats is, is het juiste gedrag: het
		// remt de schrijver af tot de tekensnelheid.
		void writeCommand(DisplayCommand& displayCommand)
		{
			if (xTaskGetCurrentTaskHandle() == Task::taskHandle)
			{
				// Wij zijn zelf de taak die de queue leegmaakt. Blokkerend
				// schrijven zou een impasse met onszelf zijn, dus maken we eerst
				// zelf plaats. Dit gebeurt als touch-afhandeling (pollTouch
				// draait in deze taak) een widget laat hertekenen.
				while (queueCommands.getNofMessagesWaiting() >= (int)CommandBufferSize)
				{
					nofQueueFullEvents++;
					drainOneCommand();
				}
			}
			else if (queueCommands.getNofMessagesWaiting() >= (int)CommandBufferSize)
			{
				nofQueueFullEvents++;
			}

			// Blokkeert tot er plaats is (bWriteWaitIfQueueFull). Dat remt de
			// schrijver af tot de tekensnelheid, wat precies goed is - en het is
			// veel netter dan zelf pollen met vTaskDelay: de schrijver wordt
			// direct gewekt zodra er ruimte vrijkomt, in plaats van pas bij de
			// volgende tick.
			queueCommands.write(displayCommand);
		}

		// Voert precies een commando uit de queue uit. Gebruikt een eigen
		// DisplayCommand, niet curCommand, zodat dit ook veilig is als het vanuit
		// de hoofdlus wordt aangeroepen.
		void drainOneCommand()
		{
			DisplayCommand displayCommand;
			queueCommands.read(displayCommand);		// blokkeert alleen als leeg, en dat is hij hier niet
			PrintContext* pPrintContext = queryPrintContext(displayCommand.getTask());
			assert(pPrintContext != nullptr);
			displayCommand.execute(display, *pPrintContext);
		}

		PrintContext* queryPrintContext(const Task* pTask)
		{
			PrintContext* pPrintContext = nullptr;
			int i = 0;
			for (i = 0; i < nofPrintContexts; i++)
			{
				pPrintContext = &(arPrintContext[i]);
				if (pPrintContext->pTask == pTask)
				{
					break;
				}
			}
			if (i == nofPrintContexts)
			{
				// There was no print context yet for this task.
				if (nofPrintContexts < MaxNofClientTasks)
				{
					pPrintContext = &(arPrintContext[nofPrintContexts++]);
					pPrintContext->pTask = pTask;
				}
				else
				{
					// Graceful degradation. Just use the last printContext.
					ESP_LOGI("AsyncDisplay", "MaxNofClientTasks constant is set too low.");
				}
			}
			return  pPrintContext;
		}

		public:
		// Hoe vaak de queue vol zat. Nul betekent dat de tekenlast binnen
		// CommandBufferSize past; een hoog getal betekent dat de schrijvende taak
		// wordt afgeremd door de tekensnelheid.
		// Hoeveel commando's nog op uitvoering wachten. Handig om te meten hoe lang
		// een hertekening werkelijk duurt: zonder dit meet je alleen hoe snel de
		// commando's in de queue zijn gezet.
		int getNofQueuedCommands()
		{
			return queueCommands.getNofMessagesWaiting();
		}

		uint32_t getNofQueueFullEvents()
		{
			return nofQueueFullEvents;
		}

	// The interface functions:
	public:
		/*override*/ void pollTouch(void* pCallingTask)
		{
			// no implementation needed for AsyncDisplay.
			// This Display Control object itself calls the function 
			// in the main-loop.
		}

		/*override*/ void touchCalibrate(uint8_t rotation, uint8_t font)
		{
			ESP_LOGI("AsyncDisplay", "Stub - don't call this function directly.");
			ESP_LOGI("AsyncDisplay", "AsyncDisplay automatically handles this at startup");
		}

		/*override*/ void addTouchListener(ITouchListener* pTouchListener)
		{
			display.addTouchListener(pTouchListener);
		}

		/*override*/ void setRotation(uint8_t rotation)
		{
			display.setRotation(rotation);
		}

		/* override */ Vec2 getScreenSize()
		{
			return display.getScreenSize();
		}

		/*override*/ int16_t getTextWidth(const char* str, uint8_t font)
		{
			return display.getTextWidth(str, font);
		}

		/*override*/ int16_t getFontHeight(uint8_t font)
		{
			return display.getFontHeight(font);
		}

		/*override*/ Vec2 getPrintCursor()
		{
			return display.getPrintCursor();
		}

		///*override*/ void setFontScale(uint8_t fontScale)
		//{
		//	ESP_LOGI("AsyncDisplay", "You should not directly set the font scale because it could interfere with");
		//	ESP_LOGI("AsyncDisplay", "the asynchronous command of another task.");
		//	ESP_LOGI("AsyncDisplay", "Instead, pass the font scale along with a drawText call.");
		//	display.setFontScale(fontScale);
		//}

		/* override */ void setClipRect(const Vec2& topLeftPix, const Vec2& sizePix)
		{
			DisplayCommand displayCommand;
			displayCommand.setClipRect(topLeftPix, sizePix);
			writeCommand(displayCommand);
		}

		/* override */ void clearClipRect()
		{
			DisplayCommand displayCommand;
			displayCommand.clearClipRect();
			writeCommand(displayCommand);
		}

		/* override */ void fillScreen(uint32_t color)
		{
			DisplayCommand displayCommand;
			displayCommand.fillScreen(color);
			writeCommand(displayCommand);
		}

		/* override */ void drawRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t color)
		{
			DisplayCommand displayCommand;
			displayCommand.drawRect(topLeftPix, sizePix, color);
			writeCommand(displayCommand);
		}

		/* override */ void fillRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t color)
		{
			DisplayCommand displayCommand;
			displayCommand.fillRect(topLeftPix, sizePix, color);
			writeCommand(displayCommand);
		}

		/* override */ void drawRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color)
		{
			DisplayCommand displayCommand;
			displayCommand.drawRoundRect(topLeftPix, sizePix, radius, color);
			writeCommand(displayCommand);
		}

		/* override */ void fillRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t color)
		{
			DisplayCommand displayCommand;
			displayCommand.fillRoundRect(topLeftPix, sizePix, radius, color);
			writeCommand(displayCommand);
		}

		/* override */ void fillSmoothRoundRect(const Vec2& topLeftPix, const Vec2& sizePix, int32_t radius, uint32_t colFg, uint32_t colBg)
		{
			DisplayCommand displayCommand;
			displayCommand.fillSmoothRoundRect(topLeftPix, sizePix, radius, colFg, colBg);
			writeCommand(displayCommand);
		}

		/* override */ void fillBeveledRect(const Vec2& topLeftPix, const Vec2& sizePix, uint32_t colFg, uint32_t colHighlight, uint32_t colShadow, BevelStyle style)
		{
			DisplayCommand displayCommand;
			displayCommand.fillBeveledRect(topLeftPix, sizePix, colFg, colHighlight, colShadow, style);
			writeCommand(displayCommand);
		}

		/*override*/ void drawString(const char* str, const Vec2& bottomLeftPix, Alignment alignment, uint8_t font, uint8_t scale, uint32_t color)
		{
			DisplayCommand displayCommand;
			displayCommand.drawString(str, bottomLeftPix, alignment, font, scale, color);
			writeCommand(displayCommand);
		}

		/*override*/ void drawStringOpaque(const char* str, const Vec2& bottomLeftPix, Alignment alignment, uint8_t font, uint8_t scale, uint32_t colFg, uint32_t colBg, const Vec2& paddingSizePix)
		{
			DisplayCommand displayCommand;
			displayCommand.drawStringOpaque(str, bottomLeftPix, alignment, font, scale, colFg, colBg, paddingSizePix);
			writeCommand(displayCommand);
		}

		/*override*/ void drawImage(const Vec2& topLeftPix, const Vec2& sizePix, const Image& image, Alignment alignment,
		                            int32_t frameRow, int32_t frameCol)
		{
			DisplayCommand displayCommand;
			displayCommand.drawImage(topLeftPix, sizePix, image, alignment, frameRow, frameCol);
			writeCommand(displayCommand);
		}

		/*override*/ void setPrintColor(void* pTask, uint32_t color)
		{
			Task* pTask2 = (Task*)pTask;
			PrintContext* printContext = queryPrintContext(pTask2);
			printContext->color = color;
		}

		/*override*/ void setPrintFont(void* pTask, uint8_t font)
		{
			Task* pTask2 = (Task*)pTask;
			PrintContext* printContext = queryPrintContext(pTask2);
			printContext->font = font;
		}

		/*override*/ void setPrintScale(void* pTask, uint8_t fontScale)
		{
			Task* pTask2 = (Task*)pTask;
			PrintContext* printContext = queryPrintContext(pTask2);
			printContext->fontScale = fontScale;
		}

		/*override*/ void setPrintCursor(void* pTask, const Vec2& cursorPos)
		{
			Task* pTask2 = (Task*)pTask;
			PrintContext* printContext = queryPrintContext(pTask2);
			printContext->cursorPos = cursorPos;
		}

		/*override*/ void setPrintWrap(void* pTask, bool bWrapX)
		{
			Task* pTask2 = (Task*)pTask;
			PrintContext* printContext = queryPrintContext(pTask2);
			printContext->bWrapX = bWrapX;
		}

		/*override*/ void print(void* pTask, const char* str)
		{
			Task* pTask2 = (Task*)pTask;
			DisplayCommand displayCommand;
			displayCommand.print(pTask2, str);
			writeCommand(displayCommand);
		}

		/*override*/ void println(void* pTask, const char* str)
		{
			Task* pTask2 = (Task*)pTask;
			DisplayCommand displayCommand;
			displayCommand.println(pTask2, str);
			writeCommand(displayCommand);
		}

	}; // end class AsyncDisplay
};// end namespace crt
