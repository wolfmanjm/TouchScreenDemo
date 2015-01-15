// main.cpp

#include "panel/RA8875.h"
#include "stm32l1xx_hal.h"

#include "GSL1680.h"

#include <stdio.h>

static RA8875 *tft;
extern "C" void setupcpp();
extern "C" void loopcpp();

extern SPI_HandleTypeDef hspi1;

#define delay(n) HAL_Delay(n)

void setupLcd();
void setupcpp()
{
    setupLcd();
}

void testLcd();
void loopcpp()
{
    testLcd();
}

void setupLcd()
{
	delay(500);
    printf("RA8875 start\r\n");
    tft = new RA8875(&hspi1, GPIOA, GPIO_PIN_4);
    //initialization routine
    tft->begin(RA8875_800x480);

    //following it's already by begin function but
    //if you like another background color....
    tft->fillScreen(RA8875_BLACK);//fill screen black

    tft->setTextColor(RA8875_GREEN);
    tft->setFontScale(1);//font x2
    tft->printf("RA8875 is alive with %dx%d\n", 800, 600);
}


#include "RingBuffer.h"
RingBuffer<struct _ts_event, 16> touch_events;

extern "C" void add_touch_event(struct _ts_event *e)
{
	touch_events.push_back(*e);
}

extern "C" uint32_t i2c_read_errors;

uint32_t fc= 0, time= 0;
int max_depth= 0;
// 15 lines x 49 characters
void testLcd()
{
	int cnt= 0;
	while(!touch_events.empty()) {
		struct _ts_event tse;
		touch_events.pop_front(tse);

		// display a circle under each finger
		if(tse.n_fingers > 0) {
		    for(int i = 0; i < tse.n_fingers; i++) {
	            uint8_t f= tse.coords[i].finger;
	            uint32_t x= tse.coords[i].x;
	            uint32_t y= tse.coords[i].y;
				uint16_t col;
				switch(f) {
					case 1: col= RA8875_RED; break;
					case 2: col= RA8875_GREEN; break;
					case 3: col= RA8875_BLUE; break;
					case 4: col= RA8875_YELLOW; break;
					case 5: col= RA8875_WHITE; break;
					default: col= RA8875_CYAN;
				}
				tft->fillCircle(x, y, 20, col);
	        }
	    }
	    cnt++;
	}
	if(cnt > max_depth) max_depth= cnt;

    // User button is clear screen
	GPIO_PinState s= HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
	if(s == GPIO_PIN_SET) {
		tft->fillScreen(RA8875_BLACK);
	}

	// display stats every 2 seconds
	if(HAL_GetTick() > fc) {
		// (was 59ms) (20ms with block transfer) 11ms with write multiple registers
		uint32_t s= HAL_GetTick();
		fc= s + 2000;
		time+=2;
	    tft->setTextColor(RA8875_GREEN, RA8875_BLACK);
		tft->setCursor(0, 0);
		tft->printf("overflow: %10d\n", touch_events.get_overflow());
		tft->printf("max_depth: %10d\n", max_depth);
		uint32_t e= HAL_GetTick();
		uint32_t d=  e-s;
		tft->printf("time: %6lu secs, %6lu ms, i2c errors: %6lu\n", time, d, i2c_read_errors);
	}
}
