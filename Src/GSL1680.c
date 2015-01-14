// driver for the GSL1680 touch panel
// Information gleaned from https://github.com/rastersoft/gsl1680.git and various other sources
// firmware for the specific panel was found here:- http://www.buydisplay.com/default/5-inch-tft-lcd-module-800x480-display-w-controller-i2c-serial-spi
// As was some test code.
// This is for that 800X480 display and the 480x272 from buydisplay.com

/*
Pin outs
the FPC on the touch panel is six pins, pin 1 is to the left pin 6 to the right with the display facing up

pin | function  | Arduino Uno
-----------------------------
1   | SCL       | B6
2   | SDA       | B7
3   | VDD (3v3) | 3v3
4   | Wake      | A2
5   | Int       | A1
6   | Gnd       | gnd
*/
#include "stm32l1xx_hal.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

// TODO define for other resolution
#include "gslX680firmware.h"
#include "GSL1680.h"

// Pins
#define WAKE_PIN      GPIO_PIN_2
#define WAKE_PORT     GPIOA
#define INTRPT_PIN    GPIO_PIN_1
#define INTRPT_PORT   GPIOA

#define LED3_PIN       GPIO_PIN_9
#define LED3_GPIO_PORT GPIOC

#define LED4_PIN       GPIO_PIN_8
#define LED4_GPIO_PORT GPIOC

#define SCREEN_MAX_X        800
#define SCREEN_MAX_Y        480

#define GSLX680_I2C_ADDR    (0x40<<1) // STM HAL does not right shift the address like everyone else does

#define GSL_DATA_REG        0x80
#define GSL_STATUS_REG      0xe0
#define GSL_PAGE_REG        0xf0

#define delay(n) HAL_Delay(n)

typedef uint8_t bool;

struct _ts_event ts_event;

extern I2C_HandleTypeDef hi2c1;
bool i2c_write(uint8_t reg, uint8_t *buf, int cnt)
{
#if 0
    printf("i2c write: %02X-", reg);
    for(int i = 0; i < cnt; i++) {
        printf("%02X, ", buf[i]);
    }
    printf("\r\n");
#endif

    uint8_t pData[cnt + 1];
    pData[0] = reg;
    memcpy(&pData[1], buf, cnt);
    HAL_StatusTypeDef r = HAL_I2C_Master_Transmit(&hi2c1, GSLX680_I2C_ADDR, pData, cnt + 1, 1000);
    if(r != HAL_OK) {
        printf("i2c write error: %d %02X\r\n", r, reg);
    }
    return r == HAL_OK;
}

int i2c_read(uint8_t reg, uint8_t *buf, int cnt)
{
    HAL_StatusTypeDef r;
    r = HAL_I2C_Master_Transmit(&hi2c1, GSLX680_I2C_ADDR, &reg, 1, 1000);
    if(r != 0) {
        printf("i2c read1 error: %d %02X\r\n", r, reg);
    }

    r = HAL_I2C_Master_Receive(&hi2c1, GSLX680_I2C_ADDR, buf, cnt, 1000);
    if(r != HAL_OK) {
        printf("i2c read2 error: %d %02X\r\n", r, reg);
    }
    return cnt;
}

void clr_reg(void)
{
    uint8_t buf[4];

    buf[0] = 0x88;
    i2c_write(0xe0, buf, 1);
    delay(20);

    buf[0] = 0x01;
    i2c_write(0x80, buf, 1);
    delay(5);

    buf[0] = 0x04;
    i2c_write(0xe4, buf, 1);
    delay(5);

    buf[0] = 0x00;
    i2c_write(0xe0, buf, 1);
    delay(20);
}

void reset_chip()
{
    uint8_t buf[4];

    buf[0] = 0x88;
    i2c_write(GSL_STATUS_REG, buf, 1);
    delay(20);

    buf[0] = 0x04;
    i2c_write(0xe4, buf, 1);
    delay(10);

    buf[0] = 0x00;
    buf[1] = 0x00;
    buf[2] = 0x00;
    buf[3] = 0x00;
    i2c_write(0xbc, buf, 4);
    delay(10);
}

static void dat2buf(uint32_t val, uint8_t *buf)
{
    buf[0] = (char)(val & 0x000000ff);
    buf[1] = (char)((val & 0x0000ff00) >> 8);
    buf[2] = (char)((val & 0x00ff0000) >> 16);
    buf[3] = (char)((val & 0xff000000) >> 24);
}

void load_fw(void)
{
    size_t len = sizeof(GSLX680_FW);
    printf("  Firmware length: %u bytes\r\n", len);

    uint8_t addr, faddr;
    uint8_t Wrbuf[32];
    uint source_line = 0;
    uint source_len = sizeof(GSLX680_FW) / sizeof(struct fw_data);

    // write in blocks of 32 bytes to speed up download
    int i = 0;
    for (source_line = 0; source_line < source_len; source_line++) {
        addr = GSLX680_FW[source_line].offset;
        if(addr == GSL_PAGE_REG) {
            dat2buf(GSLX680_FW[source_line].val, Wrbuf);
            i2c_write(addr, Wrbuf, 4);
            i = 0;
            continue;
        }

        if(i == 0) {
            faddr = addr;
        }

        dat2buf(GSLX680_FW[source_line].val, &Wrbuf[i]);
        i += 4;
        if(i >= sizeof(Wrbuf)) {
            i2c_write(faddr, Wrbuf, i);
            i = 0;
        }
    }
}

void startup_chip(void)
{
    uint8_t buf[4];

    buf[0] = 0x00;
    i2c_write(0xe0, buf, 1);
}

void init_chip()
{
#if 1
    printf("  Toggle Wake\r\n");
    HAL_GPIO_WritePin(WAKE_PORT, WAKE_PIN, 1);
    delay(50);
    HAL_GPIO_WritePin(WAKE_PORT, WAKE_PIN, 0);
    delay(50);
    HAL_GPIO_WritePin(WAKE_PORT, WAKE_PIN, 1);
    delay(50);

    // CTP startup sequence
    printf("  clr reg\r\n");
    clr_reg();
    printf("  reset_chip\r\n");
    reset_chip();
    printf("  load_fw: %lu\r\n", HAL_GetTick());
    load_fw();
    //startup_chip();
    printf("  reset_chip2: %lu\r\n", HAL_GetTick());
    reset_chip();
    printf("  startup_chip\r\n");
    startup_chip();
    printf("  init done\r\n");

#else
    // rastersoft int sequence
    reset_chip();
    load_fw();
    startup_chip();
    reset_chip();

    digitalWrite(WAKE, LOW);
    delay(50);
    digitalWrite(WAKE, HIGH);
    delay(30);
    digitalWrite(WAKE, LOW);
    delay(5);
    digitalWrite(WAKE, HIGH);
    delay(20);

    reset_chip();
    startup_chip();
#endif
}

int read_data(void)
{

    //printf("reading data...\r\n");
    uint8_t touch_data[24] = {0};
    int n = i2c_read(GSL_DATA_REG, touch_data, 24);
    //printf("read: %d\r\n", n);
    if(n != 24) return 0;
    ts_event.n_fingers = touch_data[0];
    for(int i = 0; i < ts_event.n_fingers; i++) {
        ts_event.coords[i].x = ( (((uint32_t)touch_data[(i * 4) + 5]) << 8) | (uint32_t)touch_data[(i * 4) + 4] ) & 0x00000FFF; // 12 bits of X coord
        ts_event.coords[i].y = ( (((uint32_t)touch_data[(i * 4) + 7]) << 8) | (uint32_t)touch_data[(i * 4) + 6] ) & 0x00000FFF;
        ts_event.coords[i].finger = (uint32_t)touch_data[(i * 4) + 7] >> 4; // finger that did the touch
    }

    return ts_event.n_fingers;
}

void setup()
{
    delay(200);
    printf("Starting touch screen...\r\n");
    HAL_GPIO_WritePin(WAKE_PORT, WAKE_PIN, 0);
    delay(100);
    init_chip();

#if 0
    uint8_t buf[4];
    i2c_read(0xB0, buf, 4);
    printf("%02X, %02X, %02X,%02X\r\n", buf[0], buf[1], buf[2], buf[3]);
#endif
}

void loop()
{
    // // poll the interrupt TODO should be an actual interrupt
    // GPIO_PinState s = HAL_GPIO_ReadPin(INTRPT_PORT, INTRPT_PIN);
    // if(s == 1) {
    //     HAL_GPIO_TogglePin(LED3_GPIO_PORT, LED3_PIN);
    //     int n = read_data();
    //     // for(int i = 0; i < n; i++) {
    //     //     printf("%d %lu %lu\r\n", ts_event.coords[i].finger, ts_event.coords[i].x, ts_event.coords[i].y);
    //     // }
    //     // printf("---\r\n");
    //     if(n > 0) ts_event.touch = 1;
    // }
}

/**
  * @brief EXTI line detection callbacks
  * @param GPIO_Pin: Specifies the pins connected EXTI line
  * @retval None
  */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
    if (GPIO_Pin == INTRPT_PIN) {
        HAL_GPIO_TogglePin(LED3_GPIO_PORT, LED3_PIN);
        int n = read_data();
        // for(int i = 0; i < n; i++) {
        //     printf("%d %lu %lu\r\n", ts_event.coords[i].finger, ts_event.coords[i].x, ts_event.coords[i].y);
        // }
        // printf("---\r\n");
        if(n > 0) ts_event.touch = 1;

    }else{
        printf("Unknown interrupt pin: %d\r\n", GPIO_Pin);
    }
}
