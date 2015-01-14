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



// 15 lines x 49 characters
void testLcd()
{
	// display a circle under each finger
	if(ts_event.touch && ts_event.n_fingers > 0) {
		ts_event.touch= 0;
	    for(int i = 0; i < ts_event.n_fingers; i++) {
            uint8_t f= ts_event.coords[i].finger;
            uint32_t x= ts_event.coords[i].x;
            uint32_t y= ts_event.coords[i].y;
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

    }else{
    	ts_event.touch= 0;
    }

    // User button is clear screen
	GPIO_PinState s= HAL_GPIO_ReadPin(GPIOA, GPIO_PIN_0);
	if(s == GPIO_PIN_SET) {
		tft->fillScreen(RA8875_BLACK);
	}
}
