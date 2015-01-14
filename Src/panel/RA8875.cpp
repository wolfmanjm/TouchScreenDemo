
#include "RA8875.h"

#include "stm32l1xx_hal.h"
#include "stm32l1xx_hal_gpio.h"

#include <string.h>
#include <stdio.h>

#define delay(n) HAL_Delay(n)

// from arduino
#define bitRead(value, bit) (((value) >> (bit)) & 0x01)
#define bitSet(value, bit) ((value) |= (1UL << (bit)))
#define bitClear(value, bit) ((value) &= ~(1UL << (bit)))
#define bitWrite(value, bit, bitvalue) (bitvalue ? bitSet(value, bit) : bitClear(value, bit))

/**************************************************************************/
/*!
	Contructor
	SPI nd GPIO passed in
*/
/**************************************************************************/

RA8875::RA8875(SPI_HandleTypeDef *hspi, GPIO_TypeDef* csport, uint16_t cspin)
{
	this->hspi= hspi;
	this->cs_port= csport;
	this->cs_pin= cspin;
}


/**************************************************************************/
/*!
	Initialize library
	Parameter:
    RA8875_480x272 (4.3" displays)
    RA8875_800x480 (5" and 7" displays)
	Adafruit_480x272 (4.3" Adafruit displays)
	Adafruit_800x480 (5" and 7" Adafruit displays)
	UPDATE! Some devices ONLY in Energia IDE needs an extra parameter!
	module: sets the SPI interface (it depends from MCU). Default:0
*/
/**************************************************************************/

void RA8875::begin(const enum RA8875sizes s) {

	_size = s;
	uint8_t initIndex;

	if (_size == RA8875_320x240) {//still not supported! Wait next version
		_width = 320;
		_height = 240;
		initIndex = 0;
		_maxLayers = 2;
	} else if (_size == RA8875_480x272 || _size == Adafruit_480x272) {
		_width = 480;
		_height = 272;
		initIndex = 1;
		_maxLayers = 2;
	} else if (_size == RA8875_640x480) {//still not supported! Wait next version
		_width = 640;
		_height = 480;
		initIndex = 2;
		_maxLayers = 1;
	} else if (_size == RA8875_800x480 || _size == Adafruit_800x480) {
		_width = 800;
		_height = 480;
		initIndex = 3;
		_maxLayers = 1;
	} else {
		_width = 480;
		_height = 272;
		initIndex = 1;
		_maxLayers = 2;
	}

	_currentLayer = 0;
	_currentMode = GRAPHIC;

	_cursorX = 0; _cursorY = 0;
	_textWrap = true;
	_textSize = X16;
	_fontSpacing = 0;
	_extFontRom = false;
	_fontRomType = GT21L16T1W;
	_fontRomCoding = GB2312;
	_fontSource = INT;
	_fontFullAlig = false;
	_fontRotation = false;
	_fontInterline = 0;
	_fontFamily = STANDARD;
	_textCursorStyle = BLINK;
	_scrollXL = 0; _scrollXR = 0; _scrollYT = 0; _scrollYB = 0;
	_useMultiLayers = false;//starts with one layer only

/* Display Configuration Register	  [0x20]
	  7: (Layer Setting Control) 0:one Layer, 1:two Layers
	  6,5,4: (na)
	  3: (Horizontal Scan Direction) 0: SEG0 to SEG(n-1), 1: SEG(n-1) to SEG0
	  2: (Vertical Scan direction) 0: COM0 to COM(n-1), 1: COM(n-1) to COM0
	  1,0: (na) */
	_DPCRReg = 0b00000000;
/*	Memory Write Control Register 0
	7: 0(graphic mode), 1(textx mode)
	6: 0(font-memory cursor not visible), 1(visible)
	5: 0(normal), 1(blinking)
	4: na
	3-2: 00(LR,TB), 01(RL,TB), 10(TB,LR), 11(BT,LR)
	1: 0(Auto Increase in write), 1(no)
	0: 0(Auto Increase in read), 1(no) */
	_MWCR0Reg = 0b00000000;

/*	Font Control Register 0 [0x21]
	7: 0(CGROM font is selected), 1(CGRAM font is selected)
	6: na
	5: 0(Internal CGROM [reg 0x2F to 00]), 1(External CGROM [0x2E reg, bit6,7 to 0)
	4-2: na
	1-0: 00(ISO/IEC 8859-1), 01(ISO/IEC 8859-2), 10(ISO/IEC 8859-3), 11(ISO/IEC 8859-4)*/

	_FNCR0Reg = 0b00000000;
/*	Font Control Register 1 [0x22]
	7: 0(Full Alignment off), 1(Full Alignment on)
	6: 0(no-trasparent), 1(trasparent)
	5: na
	4: 0(normal), 1(90degrees)
	3-2: 00(x1), 01(x2), 10(x3), 11(x3) Horizontal font scale
	1-0: 00(x1), 01(x2), 10(x3), 11(x3) Vertical font scale */
	_FNCR1Reg = 0b00000000;

	/*	Font Write Type Setting Register [0x2E]
	7-6: 00(16x16,8x16,nx16), 01(24x24,12x24,nx24), 1x(32x32,16x32, nx32)
	5-0: 00...3F (font width off to 63 pixels)*/
	_FWTSETReg = 0b00000000;

	/*	Serial Font ROM Setting [0x2F]
	GT Serial Font ROM Select
	7-5: 000(GT21L16TW/GT21H16T1W),001(GT30L16U2W),010(GT30L24T3Y/GT30H24T3Y),011(GT30L24M1Z),111(GT30L32S4W/GT30H32S4W)
	FONT ROM Coding Setting
	4-2: 000(GB2312),001(GB12345/GB18030),010(BIG5),011(UNICODE),100(ASCII),101(UNI-Japanese),110(JIS0208),111(Latin/Greek/Cyrillic/Arabic)
	1-0: 00...11
		 bits	ASCII		Lat/Gr/Cyr		Arabit
		 00		normal		normal			na
		 01		Arial		var Wdth		Pres Forms A
		 10		Roman		na				Pres Forms B
		 11		Bold		na				na */
	_SFRSETReg = 0b00000000;

	/*	Interrupt Control Register1		  [0xF0]
	7,6,5: (na)
	4: KEYSCAN Interrupt Enable Bit
	3: DMA Interrupt Enable Bit
	2: TOUCH Panel Interrupt Enable Bit
	1: BTE Process Complete Interrupt Enable Bit
	0:
	When MCU-relative BTE operation is selected(*1) and BTE
	Function is Enabled(REG[50h] Bit7 = 1), this bit is used to
		Enable the BTE Interrupt for MCU R/W:
		0 : Disable BTE interrupt for MCU R/W.
		1 : Enable BTE interrupt for MCU R/W.
	When the BTE Function is Disabled, this bit is used to
		Enable the Interrupt of Font Write Function:
		0 : Disable font write interrupt.
		1 : Enable font write interrupt.
	*/
	_INTC1Reg = 0b00000000;

	endSend(); // sets CS high

	_rst= false;
	// Do reset if needed
	// if (_rst < 255){
	// 	pinMode(_rst, OUTPUT);
	// 	digitalWrite(_rst, HIGH);
	// 	delay(5);
	// 	digitalWrite(_rst, LOW);
	// 	delay(20);
	// 	digitalWrite(_rst, HIGH);
	// 	delay(150);
	// }

	//	settings = SPISettings(MAXSPISPEED, MSBFIRST, SPI_MODE0);

	initialize(initIndex);
}

/************************* Initialization *********************************/

/**************************************************************************/
/*!
	PRIVATE
      Hardware initialization of RA8875 and turn on
*/
/**************************************************************************/
static const uint8_t initStrings[4][15] = { // put in flash
	{0x0A,0x02,0x03,0x27,0x00,0x05,0x04,0x03,0xEF,0x00,0x05,0x00,0x0E,0x00,0x02},//0 -> 320x240 (to be fixed)
	{0x10,0x02,0x82,0x3B,0x00,0x01,0x00,0x05,0x0F,0x01,0x02,0x00,0x07,0x00,0x09},//1 -> 480x272 (0x0A)
	{0x0B,0x02,0x01,0x4F,0x05,0x0F,0x01,0x00,0xDF,0x10,0x0A,0x00,0x0E,0x00,0x01},//2 -> 640x480 (to be fixed)
	{0x10,0x02,0x81,0x63,0x00,0x03,0x03,0x0B,0xDF,0x01,0x1F,0x00,0x16,0x00,0x01}// 3 -> 800x480 (0x0B)(to be fixed?)
};

void RA8875::initialize(uint8_t initIndex) {
	if (!_rst) {//soft reset
		softReset();
	}

	writeReg(RA8875_PLLC1,initStrings[initIndex][0]);////PLL Control Register 1
	delay(1);
	writeReg(RA8875_PLLC2,initStrings[initIndex][1]);////PLL Control Register 2
	delay(1);

	writeReg(RA8875_PCSR,initStrings[initIndex][2]);//Pixel Clock Setting Register
	delay(1);
	writeReg(RA8875_SYSR,0x0C);//we are working ALWAYS at 65K color space!!!!
	writeReg(RA8875_HDWR,initStrings[initIndex][3]);//LCD Horizontal Display Width Register
	writeReg(RA8875_HNDFTR,initStrings[initIndex][4]);//Horizontal Non-Display Period Fine Tuning Option Register
	writeReg(RA8875_HNDR,initStrings[initIndex][5]);////LCD Horizontal Non-Display Period Register
	writeReg(RA8875_HSTR,initStrings[initIndex][6]);////HSYNC Start Position Register
	writeReg(RA8875_HPWR,initStrings[initIndex][7]);////HSYNC Pulse Width Register
	writeReg(RA8875_VDHR0,initStrings[initIndex][8]);////LCD Vertical Display Height Register0
	writeReg(RA8875_VDHR1,initStrings[initIndex][9]);////LCD Vertical Display Height Register1
	writeReg(RA8875_VNDR0,initStrings[initIndex][10]);////LCD Vertical Non-Display Period Register 0
	writeReg(RA8875_VNDR1,initStrings[initIndex][11]);////LCD Vertical Non-Display Period Register 1
	writeReg(RA8875_VSTR0,initStrings[initIndex][12]);////VSYNC Start Position Register 0
	writeReg(RA8875_VSTR1,initStrings[initIndex][13]);////VSYNC Start Position Register 1
	writeReg(RA8875_VPWR,initStrings[initIndex][14]);////VSYNC Pulse Width Register
	setActiveWindow(0,(_width-1),0,(_height-1));//set the active winsow
	clearMemory(true);//clear FULL memory
	//end of hardware initialization
	delay(10);

    //now starts the first time setting up
	displayOn(true);//turn On Display
	if (_size == Adafruit_480x272 || _size == Adafruit_800x480) GPIOX(true);//only for adafruit stuff
	PWMsetup(1,true, RA8875_PWM_CLK_DIV1024);//setup PWM ch 1 for backlight
	PWMout(1,255);//turn on PWM1
	setCursorBlinkRate(DEFAULTCURSORBLINKRATE);//set default blink rate
	if (_textCursorStyle == BLINK) showCursor(false,BLINK); //set default text cursor type and turn off
	setIntFontCoding(DEFAULTINTENCODING);//set default internal font encoding
	setFont(INT);	//set internal font use
	setTextColor(RA8875_WHITE);//since the blackground it's black...
	//now tft it's ready to go and in [Graphic mode]
}

/**************************************************************************/
/*!
      Software Reset
*/
/**************************************************************************/
void RA8875::softReset(void) {
	writeCommand(RA8875_PWRR);
	writeData(RA8875_PWRR_SOFTRESET);
	writeData(RA8875_PWRR_NORMAL);
	delay(200);
}
/**************************************************************************/
/*!
		Clear memory
	    Parameters:
		full: true(clear all memory), false(clear active window only)
*/
/**************************************************************************/
void RA8875::clearMemory(bool full){
	uint8_t temp = 0b00000000;
	if (!full) temp |= (1 << 6);
	temp |= (1 << 7);//enable start bit
	writeReg(RA8875_MCLR,temp);
	//_cursorX = _cursorY = 0;
	waitBusy(0x80);
}


/**************************************************************************/
/*!
		Set the Active Window
	    Parameters:
		XL: Horizontal Left
		XR: Horizontal Right
		YT: Vertical TOP
		YB: Vertical Bottom
*/
/**************************************************************************/
void RA8875::setActiveWindow(uint16_t XL,uint16_t XR ,uint16_t YT ,uint16_t YB){
	if (XR >= _width) XR = _width-1;
	if (YB >= _height) YB = _height-1;
    // X
	writeReg(RA8875_HSAW0,XL);
	writeReg(RA8875_HSAW1,XL >> 8);
	writeReg(RA8875_HEAW0,XR);
	writeReg(RA8875_HEAW1,XR >> 8);
    // Y
	writeReg(RA8875_VSAW0,YT);
	writeReg(RA8875_VSAW1,YT >> 8);
	writeReg(RA8875_VEAW0,YB);
	writeReg(RA8875_VEAW1,YB >> 8);
}

/**************************************************************************/
/*!
		Return the max tft width.
		Note that real size will start from 0
		so you need to subtract 1!
*/
/**************************************************************************/
uint16_t RA8875::width(void) { return _width; }

/**************************************************************************/
/*!
		Return the max tft height.
		Note that real size will start from 0
		so you need to subtract 1!
*/
/**************************************************************************/
uint16_t RA8875::height(void) { return _height; }

/************************* Text Mode ***********************************/

/**************************************************************************/
/*!
		Change the mode between graphic and text
		Parameters:
		m: can be GRAPHIC or TEXT
*/
/**************************************************************************/
void RA8875::changeMode(enum RA8875modes m) {
	writeCommand(RA8875_MWCR0);
	if (m == GRAPHIC){
		if (_currentMode == TEXT){//avoid useless consecutive calls
			 _MWCR0Reg &= ~(1 << 7);
			 _currentMode = GRAPHIC;
			writeData(_MWCR0Reg);
		}
	} else {
		if (_currentMode == GRAPHIC){//avoid useless consecutive calls
			_MWCR0Reg |= (1 << 7);
			_currentMode = TEXT;
			writeData(_MWCR0Reg);
		}
	}
}

/**************************************************************************/
/*!		Upload user custom cahr or symbol to CGRAM, max 255
		Parameters:
		symbol[]: an 8bit x 16 char in an array. Must be exact 16 bytes
		address: 0...255 the address of the CGRAM where to store the char
*/
/**************************************************************************/
void RA8875::uploadUserChar(const uint8_t symbol[],uint8_t address) {
	bool modeChanged = false;
	if (_currentMode != GRAPHIC) {//was in text!
		changeMode(GRAPHIC);
		modeChanged = true;
	}
	writeReg(RA8875_CGSR,address);
	writeTo(CGRAM);
	writeCommand(RA8875_MRWC);
	for (uint8_t i=0;i<16;i++){
		writeData(symbol[i]);
	}
	if (modeChanged) changeMode(TEXT);
}

/**************************************************************************/
/*!		Retrieve and print to screen the user custom char or symbol
		User have to store a custom char before use this function
		Parameters:
		address: 0...255 the address of the CGRAM where char it's stored
		wide:0 for single 8x16 char, if you have wider chars that use
		more than a char slot they can be showed combined (see examples)
*/
/**************************************************************************/
void RA8875::showUserChar(uint8_t symbolAddrs,uint8_t wide) {
	uint8_t oldRegState = _FNCR0Reg;
	uint8_t i;
	bitSet(oldRegState,7);//set to CGRAM
	writeReg(RA8875_FNCR0,oldRegState);
	//layers?
 	if (_useMultiLayers){
		if (_currentLayer == 0){
			writeTo(L1);
		} else {
			writeTo(L2);
		}
	} else {
		writeTo(L1);
	}
	writeCommand(RA8875_MRWC);
	writeData(symbolAddrs);
	if (wide > 0){
		for (i=1;i<=wide;i++){
			writeData(symbolAddrs+i);
		}
	}
	if (oldRegState != _FNCR0Reg) writeReg(RA8875_FNCR0,_FNCR0Reg);
}

/**************************************************************************/
/*!
		Set internal Font Encoding
		Parameters:
		f:ISO_IEC_8859_1, ISO_IEC_8859_2, ISO_IEC_8859_3, ISO_IEC_8859_4
		default:ISO_IEC_8859_1
*/
/**************************************************************************/
void RA8875::setIntFontCoding(enum RA8875fontCoding f) {
	uint8_t temp = _FNCR0Reg;
	temp &= ~((1<<1) | (1<<0));// Clear bits 1 and 0
	switch (f){
		case ISO_IEC_8859_1:
			 //do nothing
		break;
		case ISO_IEC_8859_2:
			temp |= (1 << 0);
		break;
		case ISO_IEC_8859_3:
			temp |= (1 << 1);
		break;
		case ISO_IEC_8859_4:
			temp |= ((1<<1) | (1<<0));// Set bits 1 and 0
		break;
		default:
		return;
	}
	_FNCR0Reg = temp;
	writeReg(RA8875_FNCR0,_FNCR0Reg);
}

/**************************************************************************/
/*!
		External Font Rom setup
		This will not phisically change the register but should be called before setFont(EXT)!
		You should use this values accordly Font ROM datasheet!
		Parameters:
		ert:ROM Type          (GT21L16T1W, GT21H16T1W, GT23L16U2W, GT30H24T3Y, GT23L24T3Y, GT23L24M1Z, GT23L32S4W, GT30H32S4W)
		erc:ROM Font Encoding (GB2312, GB12345, BIG5, UNICODE, ASCII, UNIJIS, JIS0208, LATIN)
		erf:ROM Font Family   (STANDARD, ARIAL, ROMAN, BOLD)
*/
/**************************************************************************/
void RA8875::setExternalFontRom(enum RA8875extRomType ert, enum RA8875extRomCoding erc, enum RA8875extRomFamily erf){
	uint8_t temp = _SFRSETReg;//just to preserve the reg in case something wrong
	switch(ert){ //type of rom
		case GT21L16T1W:
		case GT21H16T1W:
			temp &= 0x1F;
		break;
		case GT23L16U2W:
			temp &= 0x1F; temp |= 0x20;
		break;
		case GT23L24T3Y:
		case GT30H24T3Y:
		case ER3303_1://encoding GB12345
			temp &= 0x1F; temp |= 0x40;
			//erc = GB12345;//forced
		break;
		case GT23L24M1Z:
			temp &= 0x1F; temp |= 0x60;
		break;
		case GT23L32S4W:
		case GT30H32S4W:
			temp &= 0x1F; temp |= 0x80;
		break;
		default:
			_extFontRom = false;//wrong type, better avoid for future
			return;//cannot continue, exit
		}
		_fontRomType = ert;
	switch(erc){	//check rom font coding
		case GB2312:
			temp &= 0xE3;
		break;
		case GB12345:
			temp &= 0xE3; temp |= 0x04;
		break;
		case BIG5:
			temp &= 0xE3; temp |= 0x08;
		break;
		case UNICODE:
			temp &= 0xE3; temp |= 0x0C;
		break;
		case ASCII:
			temp &= 0xE3; temp |= 0x10;
		break;
		case UNIJIS:
			temp &= 0xE3; temp |= 0x14;
		break;
		case JIS0208:
			temp &= 0xE3; temp |= 0x18;
		break;
		case LATIN:
			temp &= 0xE3; temp |= 0x1C;
		break;
		default:
			_extFontRom = false;//wrong coding, better avoid for future
			return;//cannot continue, exit
		}
		_fontRomCoding = erc;
		_SFRSETReg = temp;
		setExtFontFamily(erf,false);
		_extFontRom = true;
		//writeReg(RA8875_SFRSET,_SFRSETReg);//0x2F
		//delay(4);
}

/**************************************************************************/
/*!
		select the font family for the external Font Rom Chip
		Parameters:
		erf: STANDARD, ARIAL, ROMAN, BOLD
		setReg:
		true(send phisically the register, useful when you change
		family after set setExternalFontRom)
		false:(change only the register container, useful during config)
*/
/**************************************************************************/
void RA8875::setExtFontFamily(enum RA8875extRomFamily erf,bool setReg) {
	_fontFamily = erf;
	switch(erf){	//check rom font family
		case STANDARD:
			_SFRSETReg &= 0xFC;
		break;
		case ARIAL:
			_SFRSETReg &= 0xFC; _SFRSETReg |= 0x01;
		break;
		case ROMAN:
			_SFRSETReg &= 0xFC; _SFRSETReg |= 0x02;
		break;
		case BOLD:
			_SFRSETReg |= ((1<<1) | (1<<0)); // set bits 1 and 0
		break;
		default:
			_fontFamily = STANDARD; _SFRSETReg &= 0xFC;
			return;
	}
	if (setReg) writeReg(RA8875_SFRSET,_SFRSETReg);
}

/**************************************************************************/
/*!
		choose from internal/external (if exist) Font Rom
		Parameters:
		s: Font source (INT,EXT)
*/
/**************************************************************************/
void RA8875::setFont(enum RA8875fontSource s) {
	//enum RA8875fontCoding c
	if (s == INT){
		//check the font coding
		if (_extFontRom) {
			setFontSize(X16,false);
			writeReg(RA8875_SFRSET,0b00000000);//_SFRSETReg
		}
		_FNCR0Reg &= ~((1<<7) | (1<<5));// Clear bits 7 and 5
		writeReg(RA8875_FNCR0,_FNCR0Reg);
		_fontSource = s;
		delay(1);
	} else {
		if (_extFontRom){
			_fontSource = s;
			//now switch
			_FNCR0Reg |= (1 << 5);
			writeReg(RA8875_FNCR0,_FNCR0Reg);//0x21
			delay(1);
			writeReg(RA8875_SFCLR,0x02);//Serial Flash/ROM CLK frequency/2
			setFontSize(X24,false);////X24 size
			writeReg(RA8875_SFRSET,_SFRSETReg);//at this point should be already set
			delay(4);
			writeReg(RA8875_SROC,0x28);// 0x28 rom 0,24bit adrs,wave 3,1 byte dummy,font mode, single mode
			delay(4);
		} else {
			setFont(INT);
		}
	}
}

/**************************************************************************/
/*!
		Enable/Disable the Font Full Alignemet feature (default off)
		Parameters:
		align: true,false
*/
/**************************************************************************/
void RA8875::setFontFullAlign(bool align) {
	align == true ? _FNCR1Reg |= (1 << 7) : _FNCR1Reg &= ~(1 << 7);
	writeReg(RA8875_FNCR1,_FNCR1Reg);
}

/**************************************************************************/
/*!
		Enable/Disable 90" Font Rotation (default off)
		Parameters:
		rot: true,false
*/
/**************************************************************************/
void RA8875::setFontRotate(bool rot) {
	rot == true ? _FNCR1Reg |= (1 << 4) : _FNCR1Reg &= ~(1 << 4);
	writeReg(RA8875_FNCR1,_FNCR1Reg);
}

/**************************************************************************/
/*!
		Set distance between text lines (default off)
		Parameters:
		pix: 0...63 pixels
*/
/**************************************************************************/
void RA8875::setFontInterline(uint8_t pix){
	if (pix > 0x3F) pix = 0x3F;
	_fontInterline = pix;
	//_FWTSETReg &= 0xC0;
	//_FWTSETReg |= spc & 0x3F;
	writeReg(RA8875_FLDR,_fontInterline);
}
/**************************************************************************/
/*!
		Set the Text position for write Text only.
		Parameters:
		x:horizontal in pixels
		y:vertical in pixels
*/
/**************************************************************************/
void RA8875::setCursor(uint16_t x, uint16_t y) {
	if (!_textWrap){
		if (x >= _width) x = _width-1;
		if (y >= _height) y = _height-1;
	}
	_cursorX = x;
	_cursorY = y;
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_F_CURXL,RA8875_F_CURXH,RA8875_F_CURYL,RA8875_F_CURYH};
	uint8_t data[] = {(uint8_t)(x & 0xFF),(uint8_t)(x >> 8),(uint8_t)(y & 0xFF),(uint8_t)(y >> 8)};
	setMultipleRegisters(reg,data,4);
#else
	writeReg(RA8875_F_CURXL,(x & 0xFF));
	writeReg(RA8875_F_CURXH,(x >> 8));
	writeReg(RA8875_F_CURYL,(y & 0xFF));
	writeReg(RA8875_F_CURYH,(y >> 8));
#endif
}

/**************************************************************************/
/*!
		Update the library tracked  _cursorX,_cursorX and give back
		Parameters:
		x*:horizontal pos in pixels
		y*:vertical pos in pixels
		USE: xxx.getCursor(&myX,&myY);
*/
/**************************************************************************/
void RA8875::getCursor(uint16_t *x, uint16_t *y) {
	uint8_t t1,t2;
	t1 = readReg(RA8875_F_CURXL);
	t2 = readReg(RA8875_F_CURXH);
	_cursorX = (t2 << 8) | (t1 & 0xFF);
	t1 = readReg(RA8875_F_CURYL);
	t2 = readReg(RA8875_F_CURYH);
	_cursorY = (t2 << 8) | (t1 & 0xFF);
	*x = _cursorX;
	*y = _cursorY;
}
/**************************************************************************/
/*!     Show/Hide text cursor
		Parameters:
		cur:(true/false) true:visible, false:not visible
		c: cursor type (NORMAL, BLINK)
*/
/**************************************************************************/
void RA8875::showCursor(bool cur,enum RA8875tcursor c){
	if (c == BLINK){
		_textCursorStyle = c;
		_MWCR0Reg |= (1 << 5);
	} else {
		_textCursorStyle = NORMAL;
		_MWCR0Reg &= ~(1 << 5);
	}
	bitWrite(_MWCR0Reg,6,cur);//set cursor visibility flag
	writeReg(RA8875_MWCR0,_MWCR0Reg);
}

/**************************************************************************/
/*!     Set cursor property blink and his rate
		Parameters:
		rate:blink speed (fast 0...255 slow)
*/
/**************************************************************************/
void RA8875::setCursorBlinkRate(uint8_t rate){
	writeReg(RA8875_BTCR,rate);//set blink rate
}

/**************************************************************************/
/*!
		set the text color and his background
		Parameters:
		fColor:16bit foreground color (text) RGB565
		bColor:16bit background color RGB565
*/
/**************************************************************************/
void RA8875::setTextColor(uint16_t fColor, uint16_t bColor){
	setForegroundColor(fColor);
	setBackgroundColor(bColor);
	_FNCR1Reg &= ~(1 << 6);
	writeReg(RA8875_FNCR1,_FNCR1Reg);
}

/**************************************************************************/
/*!
		set the text color w transparent background
		Parameters:
		fColor:16bit foreground color (text) RGB565
*/
/**************************************************************************/

void RA8875::setTextColor(uint16_t fColor){
	setForegroundColor(fColor);
	_FNCR1Reg |= (1 << 6);
	writeReg(RA8875_FNCR1,_FNCR1Reg);
}

/**************************************************************************/
/*!
		Set the Text size by it's multiple. normal should=0, max is 3 (x4)
		Parameters:
		scale:0..3  -> 0:normal, 1:x2, 2:x3, 3:x4
*/
/**************************************************************************/
void RA8875::setFontScale(uint8_t scale){
	if (scale > 3) scale = 3;
 	_FNCR1Reg &= ~(0xF); // clear bits from 0 to 3
	_FNCR1Reg |= scale << 2;
	_FNCR1Reg |= scale;
	writeReg(RA8875_FNCR1,_FNCR1Reg);
	_textScale = scale;
}

/**************************************************************************/
/*!
		Choose between 16x16(8x16) - 24x24(12x24) - 32x32(16x32)
		for External Font ROM
		Parameters:
		ts:X16,X24,X32
		halfSize:true/false (16x16 -> 8x16 and so on...)
*/
/**************************************************************************/
void RA8875::setFontSize(enum RA8875tsize ts,bool halfSize){
	switch(ts){
		case X16:
			_FWTSETReg &= 0x3F;
		break;
		case X24:
			_FWTSETReg &= 0x3F; _FWTSETReg |= 0x40;
		break;
		case X32:
			_FWTSETReg &= 0x3F; _FWTSETReg |= 0x80;
		break;
		default:
		return;
	}
	_textSize = ts;
	writeReg(RA8875_FWTSET,_FWTSETReg);
}

/**************************************************************************/
/*!
		Choose space in pixels between chars
		Parameters:
		spc:0...63pix (default 0=off)
*/
/**************************************************************************/
void RA8875::setFontSpacing(uint8_t spc){//ok
	if (spc > 0x3F) spc = 0x3F;
	_fontSpacing = spc;
	_FWTSETReg &= 0xC0;
	_FWTSETReg |= spc & 0x3F;
	writeReg(RA8875_FWTSET,_FWTSETReg);

}
/**************************************************************************/
/*!	PRIVATE
		This is the function that write text. Still in development
		NOTE: It identify correctly (when I got it) println and nl & rt

*/
/**************************************************************************/
void RA8875::textWrite(const char* buffer, uint16_t len) {
	bool goBack = false;
	uint16_t i,ny;
	uint8_t t1,t2;

	if (_currentMode == GRAPHIC){
		changeMode(TEXT);
		goBack = true;
	}

	if (len == 0) len = strlen(buffer);

	writeCommand(RA8875_MRWC);
	for (i=0;i<len;i++){
		if (buffer[i] == '\r') continue;
		if (buffer[i] == '\n') {
			//get current y
			t1 = readReg(RA8875_F_CURYL);
			t2 = readReg(RA8875_F_CURYH);
			//calc new line y
			ny = (t2 << 8) | (t1 & 0xFF);
			//update y
			ny = ny + (16 + (16*_textScale))+_fontInterline;//TODO??
			setCursor(0,ny);

		} else {
			writeData(buffer[i]);
			waitBusy(0x80);
		}

		if (_textScale > 0) delay(1);
	}

	if (goBack) changeMode(GRAPHIC);
}

/**************************************************************************/
/*!
      Sets set the foreground color using 16bit RGB565 color
	  Parameters:
	  color:16bit color RGB565
*/
/**************************************************************************/
void RA8875::setForegroundColor(uint16_t color){
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_FGCR0,RA8875_FGCR1,RA8875_FGCR2};
	uint8_t data[] = {(uint8_t)((color & 0xF800) >> 11),(uint8_t)((color & 0x07E0) >> 5),(uint8_t)(color & 0x001F)};
	setMultipleRegisters(reg,data,3);
#else
	writeReg(RA8875_FGCR0,((color & 0xF800) >> 11));
	writeReg(RA8875_FGCR1,((color & 0x07E0) >> 5));
	writeReg(RA8875_FGCR2,(color & 0x001F));
#endif
}
/**************************************************************************/
/*!
      Sets set the foreground color using 8bit R,G,B
	  Parameters:
	  R:8bit RED
	  G:8bit GREEN
	  B:8bit BLUE
*/
/**************************************************************************/
void RA8875::setForegroundColor(uint8_t R,uint8_t G,uint8_t B){
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_FGCR0,RA8875_FGCR1,RA8875_FGCR2};
	uint8_t data[] = {R,G,B};
	setMultipleRegisters(reg,data,3);
#else
	writeReg(RA8875_FGCR0,R);
	writeReg(RA8875_FGCR1,G);
	writeReg(RA8875_FGCR2,B);
#endif
}
/**************************************************************************/
/*!
      Sets set the background color using 16bit RGB565 color
	  Parameters:
	  color:16bit color RGB565
*/
/**************************************************************************/
void RA8875::setBackgroundColor(uint16_t color){
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_BGCR0,RA8875_BGCR1,RA8875_BGCR2};
	uint8_t data[] = {(uint8_t)((color & 0xF800) >> 11),(uint8_t)((color & 0x07E0) >> 5),(uint8_t)(color & 0x001F)};
	setMultipleRegisters(reg,data,3);
#else
	writeReg(RA8875_BGCR0,((color & 0xF800) >> 11));
	writeReg(RA8875_BGCR1,((color & 0x07E0) >> 5));
	writeReg(RA8875_BGCR2,(color & 0x001F));
#endif
}
/**************************************************************************/
/*!
      Sets set the background color using 8bit R,G,B
	  Parameters:
	  R:8bit RED
	  G:8bit GREEN
	  B:8bit BLUE
*/
/**************************************************************************/
void RA8875::setBackgroundColor(uint8_t R,uint8_t G,uint8_t B){
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_BGCR0,RA8875_BGCR1,RA8875_BGCR2};
	uint8_t data[] = {R,G,B};
	setMultipleRegisters(reg,data,3);
#else
	writeReg(RA8875_BGCR0,R);
	writeReg(RA8875_BGCR1,G);
	writeReg(RA8875_BGCR2,B);
#endif
}
/**************************************************************************/
/*!
      Sets set the trasparent background color using 16bit RGB565 color
	  Parameters:
	  color:16bit color RGB565
*/
/**************************************************************************/
void RA8875::setTrasparentColor(uint16_t color){
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_BGTR0,RA8875_BGTR1,RA8875_BGTR2};
	uint8_t data[] = {(uint8_t)((color & 0xF800) >> 11),(uint8_t)((color & 0x07E0) >> 5),(uint8_t)(color & 0x001F)};
	setMultipleRegisters(reg,data,3);
#else
	writeReg(RA8875_BGTR0,((color & 0xF800) >> 11));
	writeReg(RA8875_BGTR1,((color & 0x07E0) >> 5));
	writeReg(RA8875_BGTR2,(color & 0x001F));
#endif
}
/**************************************************************************/
/*!
      Sets set the Trasparent background color using 8bit R,G,B
	  Parameters:
	  R:8bit RED
	  G:8bit GREEN
	  B:8bit BLUE
*/
/**************************************************************************/
void RA8875::setTrasparentColor(uint8_t R,uint8_t G,uint8_t B){
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_BGTR0,RA8875_BGTR1,RA8875_BGTR2};
	uint8_t data[] = {R,G,B};
	setMultipleRegisters(reg,data,3);
#else
	writeReg(RA8875_BGTR0,R);
	writeReg(RA8875_BGTR1,G);
	writeReg(RA8875_BGTR2,B);
#endif
}
/************************* Graphics ***********************************/

/**************************************************************************/
/*!		Set graphic cursor beween 8 different ones.
	    Graphic cursors has to be inserted before use!
		Parameters:
		cur: 0...7
*/
/**************************************************************************/
void RA8875::setGraphicCursor(uint8_t cur) {
	if (cur > 7) cur = 7;
	uint8_t temp = readReg(RA8875_MWCR1);
	temp &= ~(0x70);//clear bit 6,5,4
	temp |= cur << 4;
	temp |= cur;
	if (_useMultiLayers){
		_currentLayer == 1 ? temp |= (1 << 0) : temp &= ~(1 << 0);
	} else {
		temp &= ~(1 << 0);//
	}
	writeData(temp);
}

/**************************************************************************/
/*!		Show the graphic cursor
	    Graphic cursors has to be inserted before use!
		Parameters:
		cur: true,false
*/
/**************************************************************************/
void RA8875::showGraphicCursor(bool cur) {
	uint8_t temp = readReg(RA8875_MWCR1);
	cur == true ? temp |= (1 << 7) : temp &= ~(1 << 7);
	if (_useMultiLayers){
		_currentLayer == 1 ? temp |= (1 << 0) : temp &= ~(1 << 0);
	} else {
		temp &= ~(1 << 0);//
	}
	writeData(temp);
}

/**************************************************************************/
/*!
	From Adafruit_RA8875, need to be fixed!!!!!!!!!
*/
/**************************************************************************/
bool RA8875::waitPoll(uint8_t regname, uint8_t waitflag) {
	while (1) {
		uint8_t temp = readReg(regname);
		if (!(temp & waitflag)) return true;
	}
	return false; // MEMEFIX: yeah i know, unreached! - add timeout?
}

/**************************************************************************/
/*!
	Just a wait routine until job it's done
	Parameters:
	res:0x80(for most operations),0x40(BTE wait), 0x01(DMA wait)
*/
/**************************************************************************/
void RA8875::waitBusy(uint8_t res) {
	uint8_t w;
	do {
	if (res == 0x01) writeCommand(RA8875_DMACR);//dma
	w = readStatus();
	} while ((w & res) == res);
}


/**************************************************************************/
/*!
		Set the position for Graphic Write
		Parameters:
		x:horizontal position
		y:vertical position
*/
/**************************************************************************/
void RA8875::setXY(int16_t x, int16_t y) {
	if (x < 0) x = 0;
	if (y < 0) y = 0;

	setX(x);
	setY(y);
}

void RA8875::setX(uint16_t x) {
	if (x >= _width) x = _width-1;
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_CURH0,RA8875_CURH1};
	uint8_t data[] = {(uint8_t)x,(uint8_t)(x >> 8)};
	setMultipleRegisters(reg,data,2);
#else
	writeReg(RA8875_CURH0, x);
	writeReg(RA8875_CURH1, (x >> 8));
#endif

}

void RA8875::setY(uint16_t y) {
	if (y >= _height) y = _height-1;
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_CURV0,RA8875_CURV1};
	uint8_t data[] = {(uint8_t)y,(uint8_t)(y >> 8)};
	setMultipleRegisters(reg,data,2);
#else
	writeReg(RA8875_CURV0, y);
	writeReg(RA8875_CURV1, y >> 8);
#endif

}

/**************************************************************************/
/*!
	  Write n Pixels directly
		Parameters:
		num:pixels
		p:16bit color

*/
/**************************************************************************/
/* void RA8875::pushPixels(uint32_t num, uint16_t p) {
	startSend();
	SPItranfer(RA8875_DATAWRITE);
	while (num--) {
		SPItranfer(p >> 8);
		SPItranfer(p);
	}
	endSend();
}
 */
/**************************************************************************/
/*!
		Define a window for perform scroll
		Parameters:
		XL: x window start left
		XR: x window end right
		YT: y window start top
		YB: y window end bottom

*/
/**************************************************************************/
void RA8875::setScrollWindow(int16_t XL,int16_t XR ,int16_t YT ,int16_t YB){
	checkLimitsHelper(XL,YT);
	checkLimitsHelper(XR,YB);

	_scrollXL = XL; _scrollXR = XR; _scrollYT = YT; _scrollYB = YB;
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_HSSW0,RA8875_HSSW1,RA8875_HESW0,RA8875_HESW1,RA8875_VSSW0,RA8875_VSSW1,RA8875_VESW0,RA8875_VESW1};
	uint8_t data[] = {(uint8_t)_scrollXL,(uint8_t)(_scrollXL >> 8),(uint8_t)_scrollXR,(uint8_t)(_scrollXR >> 8),(uint8_t)_scrollYT,(uint8_t)(_scrollYT >> 8),(uint8_t)_scrollYB,(uint8_t)(_scrollYB >> 8)};
	setMultipleRegisters(reg,data,8);
#else
    writeReg(RA8875_HSSW0,_scrollXL);
    writeReg(RA8875_HSSW1,(_scrollXL >> 8));

    writeReg(RA8875_HESW0,_scrollXR);
    writeReg(RA8875_HESW1,(_scrollXR >> 8));

    writeReg(RA8875_VSSW0,_scrollYT);
    writeReg(RA8875_VSSW1,(_scrollYT >> 8));

    writeReg(RA8875_VESW0,_scrollYB);
    writeReg(RA8875_VESW1,(_scrollYB >> 8));
#endif
}

/**************************************************************************/
/*!
		Perform the scroll

*/
/**************************************************************************/
void RA8875::scroll(uint16_t x,uint16_t y){
	if (y > _scrollYB) y = _scrollYB;//??? mmmm... not sure
	if (_scrollXL == 0 && _scrollXR == 0 && _scrollYT == 0 && _scrollYB == 0){
		//do nothing, scroll window inactive
	} else {
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_HOFS0,RA8875_HOFS1,RA8875_VOFS0,RA8875_VOFS1};
	uint8_t data[] = {(uint8_t)x,(uint8_t)(x >> 8),(uint8_t)y,(uint8_t)(y >> 8)};
	setMultipleRegisters(reg,data,4);
#else
		writeReg(RA8875_HOFS0,x);
		writeReg(RA8875_HOFS1,x >> 8);

		writeReg(RA8875_VOFS0,y);
		writeReg(RA8875_VOFS1,y >> 8);
#endif
	}
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
void RA8875::DMA_blockModeSize(int16_t BWR,int16_t BHR,int16_t SPWR){
  	writeReg(RA8875_DTNR0,BWR);
  	writeReg(RA8875_BWR1,BWR >> 8);

  	writeReg(RA8875_DTNR1,BHR);
  	writeReg(RA8875_BHR1,BHR >> 8);

  	writeReg(RA8875_DTNR2,SPWR);
  	writeReg(RA8875_SPWR1,SPWR >> 8);
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
void RA8875::DMA_startAddress(unsigned long adrs){
  	writeReg(RA8875_SSAR0,adrs);
  	writeReg(RA8875_SSAR1,adrs >> 8);
	writeReg(RA8875_SSAR2,adrs >> 16);
  	//writeReg(0xB3,adrs >> 24);// not more in datasheet!
}

/**************************************************************************/
/*! (STILL DEVELOPING)
		Display an image stored in Flash RAM
		Note: you should have the optional FLASH Chip connected to RA8875!
		Note: You should store some image in that chip!

*/
/**************************************************************************/
void RA8875::drawFlashImage(int16_t x,int16_t y,int16_t w,int16_t h,uint8_t picnum){
	checkLimitsHelper(x,y);
	checkLimitsHelper(w,h);

	writeReg(RA8875_SFCLR,0x00);
	writeReg(RA8875_SROC,0x87);
	writeReg(RA8875_DMACR,0x02);
	//setActiveWindow(0,_width-1,0,_height-1);

	setXY(x,y);

	DMA_startAddress(261120 * (picnum-1));
	DMA_blockModeSize(w,h,w);
	writeReg(RA8875_DMACR,0x03);
	waitBusy(0x01);
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
void RA8875::BTE_size(uint16_t w, uint16_t h){
    writeReg(RA8875_BEWR0,w);//BET area width literacy
    writeReg(RA8875_BEWR1,w >> 8);//BET area width literacy
    writeReg(RA8875_BEHR0,h);//BET area height literacy
    writeReg(RA8875_BEHR1,h >> 8);//BET area height literacy
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
void RA8875::BTE_source(uint16_t SX,uint16_t DX ,uint16_t SY ,uint16_t DY){
	uint8_t temp0,temp1;
    writeReg(RA8875_HSBE0,SX);//BTE horizontal position of read/write data
    writeReg(RA8875_HSBE1,SX >> 8);//BTE horizontal position of read/write data

    writeReg(RA8875_HDBE0,DX);//BET written to the target horizontal position
    writeReg(RA8875_HDBE1,DX >> 8);//BET written to the target horizontal position

    writeReg(RA8875_VSBE0,SY);//BTE vertical position of read/write data

	temp0 = SY >> 8;
	temp1 = readReg(RA8875_VSBE1);
	temp1 &= 0x80;
    temp0 = temp0 | temp1;
	writeReg(RA8875_VSBE1,temp0);//BTE vertical position of read/write data

    writeReg(RA8875_VDBE0,DY);//BET written to the target  vertical  position

	temp0 = DY >> 8;
	temp1 = readReg(RA8875_VDBE1);
	temp1 &= 0x80;
	temp0 = temp0 | temp1;
	writeReg(RA8875_VDBE1,temp0);//BET written to the target  vertical  position
}

/**************************************************************************/
/*! TESTING

*/
/**************************************************************************/
void RA8875::BTE_ROP_code(unsigned char setx){//
    writeReg(RA8875_BECR1,setx);//BECR1
}

/**************************************************************************/
/*! TESTING

*/
/**************************************************************************/
void RA8875::BTE_enable(void) {
	uint8_t temp = readReg(RA8875_BECR0);
	temp |= (1 << 7); //bit set 7
	writeReg(RA8875_BECR0,temp);
}


/**************************************************************************/
/*! TESTING

*/
/**************************************************************************/
void RA8875::writeTo(enum RA8875writes d){
	uint8_t temp = readReg(RA8875_MWCR1);
	switch(d){
		if (_useMultiLayers){
		case L1:
			temp &= ~((1<<3) | (1<<2));// Clear bits 3 and 2
			temp &= ~(1 << 0); //clear bit 0
			_currentLayer = 0;
		break;
		case L2:
			temp &= ~((1<<3) | (1<<2));// Clear bits 3 and 2
			temp |= (1 << 0); //bit set 0
			_currentLayer = 1;
		break;
		}
		case CGRAM:
			temp &= ~(1 << 3); //clear bit 3
			temp |= (1 << 2); //bit set 2
			if (bitRead(_FNCR0Reg,7)){//REG[0x21] bit7 must be 0
				_FNCR0Reg &= ~(1 << 7); //clear bit 7
				writeReg(RA8875_FNCR0,_FNCR0Reg);
			}
		break;
		case PATTERN:
			temp |= (1 << 3); //bit set 3
			temp |= (1 << 2); //bit set 2
		break;
		case CURSOR:
			temp |= (1 << 3); //bit set 3
			temp &= ~(1 << 2); //clear bit 2
		break;
		default:
		break;
	}
	writeReg(RA8875_MWCR1,temp);
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
/* void RA8875::fillRect(void) {
	writeCommand(RA8875_DCR);
	writeData(RA8875_DCR_LINESQUTRI_STOP | RA8875_DCR_DRAWSQUARE);
	writeData(RA8875_DCR_LINESQUTRI_START | RA8875_DCR_FILL | RA8875_DCR_DRAWSQUARE);
}
 */
/**************************************************************************/
/*!
      Basic pixel write
	  Parameters:
	  x:horizontal pos
	  y:vertical pos
	  color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawPixel(int16_t x, int16_t y, uint16_t color){
	//checkLimitsHelper(x,y);
	setXY(x,y);
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	SPI.beginTransaction(settings);
	writecommand_cont(RA8875_CMDWRITE);
	writecommand_last(RA8875_MRWC);

	writecommand_cont(RA8875_DATAWRITE);
	writecommand16_last(color);
	SPI.endTransaction();
#else
	writeCommand(RA8875_MRWC);
	writeData16(color);
#endif
}

/**************************************************************************/
/*!
      Basic line draw
	  Parameters:
	  x0:horizontal start pos
	  y0:vertical start
	  x1:horizontal end pos
	  y1:vertical end pos
	  color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawLine(int16_t x0, int16_t y0, int16_t x1, int16_t y1, uint16_t color){
	checkLimitsHelper(x0,y0);
	if (x1 >= _width) x1 = _width-1;
	if (y1 >= _height) y1 = _height-1;

	lineAddressing(x0,y0,x1,y1);

	setForegroundColor(color);

	writeReg(RA8875_DCR,0x80);
	waitPoll(RA8875_DCR, RA8875_DCR_LINESQUTRI_STATUS);
}

/**************************************************************************/
/*!
      for compatibility with popular Adafruit_GFX
	  draws a single vertical line
	  Parameters:
	  x:horizontal start
	  y:vertical start
	  h:height
	  color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawFastVLine(int16_t x, int16_t y, int16_t h, uint16_t color){
	if (h < 1) h = 1;
	drawLine(x, y, x, y+h, color);
}

/**************************************************************************/
/*!
      for compatibility with popular Adafruit_GFX
	  draws a single orizontal line
	  Parameters:
	  x:horizontal start
	  y:vertical start
	  w:width
	  color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawFastHLine(int16_t x, int16_t y, int16_t w, uint16_t color){
	if (w < 1) w = 1;
	drawLine(x, y, x+w, y, color);
}

/**************************************************************************/
/*!
	  draws a rectangle
	  Parameters:
	  x:horizontal start
	  y:vertical start
	  w: width
	  h:height
	  color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
	rectHelper(x, y, x+w, y+h, color, false);
}

/**************************************************************************/
/*!
	  draws a FILLED rectangle
	  Parameters:
	  x:horizontal start
	  y:vertical start
	  w: width
	  h:height
	  color: RGB565 color
*/
/**************************************************************************/
void RA8875::fillRect(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color){
	rectHelper(x, y, x+w, y+h, color, true);
}

/**************************************************************************/
/*!
      Fill the screen by using a specified RGB565 color
	  Parameters:
	  color: RGB565 color
*/
/**************************************************************************/
void RA8875::fillScreen(uint16_t color){
	rectHelper(0, 0, _width-1, _height-1, color, true);
}

/**************************************************************************/
/*!
      Draw circle
	  Parameters:
      x0:The 0-based x location of the center of the circle
      y0:The 0-based y location of the center of the circle
      r:radius
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color){
	if (r <= 0) return;
	circleHelper(x0, y0, r, color, false);
}

/**************************************************************************/
/*!
      Draw filled circle
	  Parameters:
      x0:The 0-based x location of the center of the circle
      y0:The 0-based y location of the center of the circle
      r:radius
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::fillCircle(int16_t x0, int16_t y0, int16_t r, uint16_t color){
	if (r <= 0) return;
	circleHelper(x0, y0, r, color, true);
}

/**************************************************************************/
/*!
      Draw Triangle
	  Parameters:
      x0:The 0-based x location of the point 0 of the triangle LEFT
      y0:The 0-based y location of the point 0 of the triangle LEFT
      x1:The 0-based x location of the point 1 of the triangle TOP
      y1:The 0-based y location of the point 1 of the triangle TOP
      x2:The 0-based x location of the point 2 of the triangle RIGHT
      y2:The 0-based y location of the point 2 of the triangle RIGHT
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color){
	triangleHelper(x0, y0, x1, y1, x2, y2, color, false);
}

/**************************************************************************/
/*!
      Draw filled Triangle
	  Parameters:
      x0:The 0-based x location of the point 0 of the triangle
      y0:The 0-based y location of the point 0 of the triangle
      x1:The 0-based x location of the point 1 of the triangle
      y1:The 0-based y location of the point 1 of the triangle
      x2:The 0-based x location of the point 2 of the triangle
      y2:The 0-based y location of the point 2 of the triangle
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::fillTriangle(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color){
	triangleHelper(x0, y0, x1, y1, x2, y2, color, true);
}

/**************************************************************************/
/*!
      Draw an ellipse
	  Parameters:
      xCenter:   x location of the center of the ellipse
      yCenter:   y location of the center of the ellipse
      longAxis:  Size in pixels of the long axis
      shortAxis: Size in pixels of the short axis
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawEllipse(int16_t xCenter, int16_t yCenter, int16_t longAxis, int16_t shortAxis, uint16_t color){
	ellipseHelper(xCenter, yCenter, longAxis, shortAxis, color, false);
}

/**************************************************************************/
/*!
      Draw a filled ellipse
	  Parameters:
      xCenter:   x location of the center of the ellipse
      yCenter:   y location of the center of the ellipse
      longAxis:  Size in pixels of the long axis
      shortAxis: Size in pixels of the short axis
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::fillEllipse(int16_t xCenter, int16_t yCenter, int16_t longAxis, int16_t shortAxis, uint16_t color){
	ellipseHelper(xCenter, yCenter, longAxis, shortAxis, color, true);
}

/**************************************************************************/
/*!
      Draw a curve
      Parameters:
      xCenter:]   x location of the ellipse center
      yCenter:   y location of the ellipse center
      longAxis:  Size in pixels of the long axis
      shortAxis: Size in pixels of the short axis
      curvePart: Curve to draw in clock-wise dir: 0[180-270�],1[270-0�],2[0-90�],3[90-180�]
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawCurve(int16_t xCenter, int16_t yCenter, int16_t longAxis, int16_t shortAxis, uint8_t curvePart, uint16_t color){
	curveHelper(xCenter, yCenter, longAxis, shortAxis, curvePart, color, false);
}

/**************************************************************************/
/*!
      Draw a filled curve
      Parameters:
      xCenter:]   x location of the ellipse center
      yCenter:   y location of the ellipse center
      longAxis:  Size in pixels of the long axis
      shortAxis: Size in pixels of the short axis
      curvePart: Curve to draw in clock-wise dir: 0[180-270�],1[270-0�],2[0-90�],3[90-180�]
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::fillCurve(int16_t xCenter, int16_t yCenter, int16_t longAxis, int16_t shortAxis, uint8_t curvePart, uint16_t color){
	curveHelper(xCenter, yCenter, longAxis, shortAxis, curvePart, color, true);
}

/**************************************************************************/
/*!
      Draw a rounded rectangle
	  Parameters:
      x:   x location of the rectangle
      y:   y location of the rectangle
      w:  the width in pix
      h:  the height in pix
	  r:  the radius of the rounded corner
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::drawRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color){
	roundRectHelper(x, y, x+w, y+h, r, color, false);
}


/**************************************************************************/
/*!
      Draw a filled rounded rectangle
	  Parameters:
      x:   x location of the rectangle
      y:   y location of the rectangle
      w:  the width in pix
      h:  the height in pix
	  r:  the radius of the rounded corner
      color: RGB565 color
*/
/**************************************************************************/
void RA8875::fillRoundRect(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color){
	roundRectHelper(x, y, x+w, y+h, r, color, true);
}
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
//				Helpers functions
//+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
/**************************************************************************/
/*!
      helper function for circles
*/
/**************************************************************************/
void RA8875::circleHelper(int16_t x0, int16_t y0, int16_t r, uint16_t color, bool filled){
	checkLimitsHelper(x0,y0);
	if (r < 1) r = 1;
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_DCHR0,RA8875_DCHR1,RA8875_DCVR0,RA8875_DCVR1,RA8875_DCRR};
	uint8_t data[] = {(uint8_t)x0,(uint8_t)(x0 >> 8),(uint8_t)y0,(uint8_t)(y0 >> 8),(uint8_t)r};
	setMultipleRegisters(reg,data,5);
#else
	writeReg(RA8875_DCHR0,x0);
	writeReg(RA8875_DCHR1,x0 >> 8);

	writeReg(RA8875_DCVR0,y0);
	writeReg(RA8875_DCVR1,y0 >> 8);

	writeReg(RA8875_DCRR,r);
#endif
	setForegroundColor(color);

	writeCommand(RA8875_DCR);
	filled == true ? writeData(RA8875_DCR_CIRCLE_START | RA8875_DCR_FILL) : writeData(RA8875_DCR_CIRCLE_START | RA8875_DCR_NOFILL);
	waitPoll(RA8875_DCR, RA8875_DCR_CIRCLE_STATUS);//ZzZzz
}

/**************************************************************************/
/*!
		helper function for rects
*/
/**************************************************************************/
void RA8875::rectHelper(int16_t x, int16_t y, int16_t w, int16_t h, uint16_t color, bool filled){
	checkLimitsHelper(x,y);
	if (w < 1) w = 1;
	if (h < 1) h = 1;

	lineAddressing(x,y,w,h);

	setForegroundColor(color);

	writeCommand(RA8875_DCR);
	filled == true ? writeData(0xB0) : writeData(0x90);
	waitPoll(RA8875_DCR, RA8875_DCR_LINESQUTRI_STATUS);
}


/**************************************************************************/
/*!
		common helper for check value limiter
*/
/**************************************************************************/
void RA8875::checkLimitsHelper(int16_t &x,int16_t &y){
	if (x < 0) x = 0;
	if (y < 0) y = 0;
	if (x >= _width) x = _width - 1;
	if (y >= _height) y = _height -1;
	x = x;
	y = y;
}
/**************************************************************************/
/*!
      helper function for triangles
*/
/**************************************************************************/
void RA8875::triangleHelper(int16_t x0, int16_t y0, int16_t x1, int16_t y1, int16_t x2, int16_t y2, uint16_t color, bool filled){
	checkLimitsHelper(x0,y0);
	checkLimitsHelper(x1,y1);
	checkLimitsHelper(x2,y2);

	lineAddressing(x0,y0,x1,y1);
	//p2
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_DTPH0,RA8875_DTPH1,RA8875_DTPV0,RA8875_DTPV1};
	uint8_t data[] = {(uint8_t)x2,(uint8_t)(x2 >> 8),(uint8_t)y2,(uint8_t)(y2 >> 8)};
	setMultipleRegisters(reg,data,4);
#else
	writeReg(RA8875_DTPH0,x2);
	writeReg(RA8875_DTPH1,x2 >> 8);
	writeReg(RA8875_DTPV0,y2);
	writeReg(RA8875_DTPV1,y2 >> 8);
#endif
	setForegroundColor(color);

	writeCommand(RA8875_DCR);
	filled == true ? writeData(0xA1) : writeData(0x81);
	waitPoll(RA8875_DCR, RA8875_DCR_LINESQUTRI_STATUS);
}

/**************************************************************************/
/*!
      helper function for ellipse
*/
/**************************************************************************/
void RA8875::ellipseHelper(int16_t xCenter, int16_t yCenter, int16_t longAxis, int16_t shortAxis, uint16_t color, bool filled){
	//TODO:limits!
	curveAddressing(xCenter,yCenter,longAxis,shortAxis);

	setForegroundColor(color);

	writeCommand(RA8875_ELLIPSE);
	filled == true ? writeData(0xC0) : writeData(0x80);
	waitPoll(RA8875_ELLIPSE, RA8875_ELLIPSE_STATUS);
}

/**************************************************************************/
/*!
      helper function for curve
*/
/**************************************************************************/
void RA8875::curveHelper(int16_t xCenter, int16_t yCenter, int16_t longAxis, int16_t shortAxis, uint8_t curvePart, uint16_t color, bool filled){
	//TODO:limits!
	curveAddressing(xCenter,yCenter,longAxis,shortAxis);

	setForegroundColor(color);

	writeCommand(RA8875_ELLIPSE);
	filled == true ? writeData(0xD0 | (curvePart & 0x03)) : writeData(0x90 | (curvePart & 0x03));
	waitPoll(RA8875_ELLIPSE, RA8875_ELLIPSE_STATUS);
}

/**************************************************************************/
/*!
	  helper function for rounded Rects
*/
/**************************************************************************/
void RA8875::roundRectHelper(int16_t x, int16_t y, int16_t w, int16_t h, int16_t r, uint16_t color, bool filled){
	if (r < 1) rectHelper(x,y,w,h,color,filled);//??? reason?

	checkLimitsHelper(x,y);
	checkLimitsHelper(w,h);

	lineAddressing(x,y,w,h);
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_ELL_A0,RA8875_ELL_A1,RA8875_ELL_B0,RA8875_ELL_B1};
	uint8_t data[] = {(uint8_t)r,(uint8_t)(r >> 8),(uint8_t)r,(uint8_t)(r >> 8)};
	setMultipleRegisters(reg,data,4);
#else
	writeReg(RA8875_ELL_A0,r);
	writeReg(RA8875_ELL_A1,r >> 8);
	writeReg(RA8875_ELL_B0,r);
	writeReg(RA8875_ELL_B1,r >> 8);
#endif
	setForegroundColor(color);

	writeCommand(RA8875_ELLIPSE);
	filled == true ? writeData(0xE0) : writeData(0xA0);
	waitPoll(RA8875_ELLIPSE, RA8875_DCR_LINESQUTRI_STATUS);
}

/**************************************************************************/
/*!
		Graphic line addressing helper
*/
/**************************************************************************/
void RA8875::lineAddressing(int16_t x0, int16_t y0, int16_t x1, int16_t y1){
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_DLHSR0,RA8875_DLHSR1,RA8875_DLVSR0,RA8875_DLVSR1,RA8875_DLHER0,RA8875_DLHER1,RA8875_DLVER0,RA8875_DLVER1};
	uint8_t data[] = {(uint8_t)x0,(uint8_t)(x0 >> 8),(uint8_t)y0,(uint8_t)(y0 >> 8),(uint8_t)x1,(uint8_t)(x1 >> 8),(uint8_t)y1,(uint8_t)(y1 >> 8)};
	setMultipleRegisters(reg,data,8);
#else
	//X0
	writeReg(RA8875_DLHSR0,x0);
	writeReg(RA8875_DLHSR1,x0 >> 8);
	//Y0
	writeReg(RA8875_DLVSR0,y0);
	writeReg(RA8875_DLVSR1,y0 >> 8);
	//X1
	writeReg(RA8875_DLHER0,x1);
	writeReg(RA8875_DLHER1,(x1) >> 8);
	//Y1
	writeReg(RA8875_DLVER0,y1);
	writeReg(RA8875_DLVER1,(y1) >> 8);
#endif
}

/**************************************************************************/
/*!
		curve addressing
*/
/**************************************************************************/
void RA8875::curveAddressing(int16_t x0, int16_t y0, int16_t x1, int16_t y1){
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	uint8_t reg[] = {RA8875_DEHR0,RA8875_DEHR1,RA8875_DEVR0,RA8875_DEVR1,RA8875_ELL_A0,RA8875_ELL_A1,RA8875_ELL_B0,RA8875_ELL_B1};
	uint8_t data[] = {(uint8_t)x0,(uint8_t)(x0 >> 8),(uint8_t)y0,(uint8_t)(y0 >> 8),(uint8_t)x1,(uint8_t)(x1 >> 8),(uint8_t)y1,(uint8_t)(y1 >> 8)};
	setMultipleRegisters(reg,data,8);
#else
	//center
	writeReg(RA8875_DEHR0,x0);
	writeReg(RA8875_DEHR1,x0 >> 8);
	writeReg(RA8875_DEVR0,y0);
	writeReg(RA8875_DEVR1,y0 >> 8);
	//long,short ax
	writeReg(RA8875_ELL_A0,x1);
	writeReg(RA8875_ELL_A1,x1 >> 8);
	writeReg(RA8875_ELL_B0,y1);
	writeReg(RA8875_ELL_B1,y1 >> 8);
#endif
}

/************************* Mid Level ***********************************/

/**************************************************************************/
/*!
		on/off GPIO (basic for Adafruit module
*/
/**************************************************************************/
void RA8875::GPIOX(bool on) {
    writeReg(RA8875_GPIOX, on);
}

/**************************************************************************/
/*!
		PWM out
		Parameters:
		pw:pwm selection (1,2)
		p:0...255 rate
*/
/**************************************************************************/
void RA8875::PWMout(uint8_t pw,uint8_t p) {
	uint8_t reg;
	if (pw > 1){
		reg = RA8875_P2DCR;
	} else {
		reg = RA8875_P1DCR;
	}
	 writeReg(reg, p);
}

/**************************************************************************/
/*!
		Set the brightness of the backlight (if connected to pwm)
		(basic controls pwm 1)
		Parameters:
		val:0...255
*/
/**************************************************************************/
void RA8875::brightness(uint8_t val) {
	PWMout(1,val);
}

/**************************************************************************/
/*! PRIVATE
		Setup PWM engine
		Parameters:
		pw:pwm selection (1,2)
		on: turn on/off
		clock: the clock setting
*/
/**************************************************************************/
void RA8875::PWMsetup(uint8_t pw,bool on, uint8_t clock) {
	uint8_t reg;
	uint8_t set;
	if (pw > 1){
		reg = RA8875_P2CR;
		if (on){
			set = RA8875_PxCR_ENABLE;
		} else {
			set = RA8875_PxCR_DISABLE;
		}
	} else {
		reg = RA8875_P1CR;
		if (on){
			set = RA8875_PxCR_ENABLE;
		} else {
			set = RA8875_PxCR_DISABLE;
		}
	}
	writeReg(reg,(set | (clock & 0xF)));
}


/**************************************************************************/
/*!
		Instruct the RA8875 chip to use 2 layers (if it's possible)
		Return false if not possible, true if possible
		Parameters:
		on:enable multiple layers (2)

*/
/**************************************************************************/
bool RA8875::useLayers(bool on) {
	bool clearBuffer = false;
	if (_maxLayers > 1){
		if (on){
			_useMultiLayers = true;
			_DPCRReg |= (1 << 7);
			clearBuffer = true;
		} else {
			_useMultiLayers = false;
			_DPCRReg &= ~(1 << 7);
		}
		writeReg(RA8875_DPCR,_DPCRReg);
		if (clearBuffer) {
			//for some reason if you switch to multilayer the layer 2 has garbage
			//better clear
			writeTo(L2);//switch to layer 2
			clearMemory(false);//clear memory of layer 2
			writeTo(L1);//switch to layer 1
		}
		return true;//it's possible with current conf
	}
	_useMultiLayers = false;
	return false;//not possible with current conf
}


/**************************************************************************/
/*!


*/
/**************************************************************************/
void RA8875::layerEffect(enum RA8875boolean efx){
	uint8_t	reg = 0b00000000;
	//reg &= ~(0x07);//clear bit 2,1,0
	switch(efx){//                       bit 2,1,0 of LTPR0
		case LAYER1: //only layer 1 visible  [000]
			//do nothing
		break;
		case LAYER2: //only layer 2 visible  [001]
			reg |= (1 << 0);
		break;
		case TRANSPARENT: //transparent mode [011]
			reg |= (1 << 0); reg |= (1 << 1);
		break;
		case LIGHTEN: //lighten-overlay mode [010]
			reg |= (1 << 1);
		break;
		case OR: //bool OR mode           [100]
			reg |= (1 << 2);
		break;
		case AND: //bool AND mode         [101]
			reg |= (1 << 0); reg |= (1 << 2);
		break;
		case FLOATING: //floating windows    [110]
			reg |= (1 << 1); reg |= (1 << 2);
		break;
		default:
			//do nothing
		break;
	}
	if (_useMultiLayers) writeReg(RA8875_LTPR0,reg);
}

/**************************************************************************/
/*!


*/
/**************************************************************************/
void RA8875::layerTransparency(uint8_t layer1,uint8_t layer2){
	if (layer1 > 8) layer1 = 8;
	if (layer2 > 8) layer2 = 8;

	uint8_t res = 0b00000000;//RA8875_LTPR1
	//reg &= ~(0x07);//clear bit 2,1,0
	switch (layer1){
		case 0: //disable layer
			res |= (1 << 3);
		break;
		case 1: //1/8
			res |= (1 << 0); res |= (1 << 1); res |= (1 << 2);
		break;
		case 2: //1/4
			res |= (1 << 1); res |= (1 << 2);
		break;
		case 3: //3/8
			res |= (1 << 0); res |= (1 << 2);
		break;
		case 4: //1/2
			res |= (1 << 2);
		break;
		case 5: //5/8
			res |= (1 << 0); res |= (1 << 1);
		break;
		case 6: //3/4
			res |= (1 << 1);
		break;
		case 7: //7/8
			res |= (1 << 0);
		break;
	}

	switch (layer2){
		case 0: //disable layer
			res |= (1 << 7);
		break;
		case 1: //1/8
			res |= (1 << 4); res |= (1 << 5); res |= (1 << 6);
		break;
		case 2: //1/4
			res |= (1 << 5); res |= (1 << 6);
		break;
		case 3: //3/8
			res |= (1 << 4); res |= (1 << 6);
		break;
		case 4: //1/2
			res |= (1 << 6);
		break;
		case 5: //5/8
			res |= (1 << 4); res |= (1 << 5);
		break;
		case 6: //3/4
			res |= (1 << 5);
		break;
		case 7: //7/8
			res |= (1 << 4);
		break;
	}
	if (_useMultiLayers) writeReg(RA8875_LTPR1,res);
}

/**************************************************************************/
/*!
      Change the beam scan direction on display
	  Parameters:
	  invertH:true(inverted),false(normal) horizontal
	  invertV:true(inverted),false(normal) vertical
*/
/**************************************************************************/
void RA8875::scanDirection(bool invertH,bool invertV){
	invertH == true ? _DPCRReg |= (1 << 3) : _DPCRReg &= ~(1 << 3);
	invertV == true ? _DPCRReg |= (1 << 2) : _DPCRReg &= ~(1 << 2);
	writeReg(RA8875_DPCR,_DPCRReg);
}

/**************************************************************************/
/*!
      turn display on/off
*/
/**************************************************************************/
void RA8875::displayOn(bool on) {
	on == true ? writeReg(RA8875_PWRR, RA8875_PWRR_NORMAL | RA8875_PWRR_DISPON) : writeReg(RA8875_PWRR, RA8875_PWRR_NORMAL | RA8875_PWRR_DISPOFF);
}


/**************************************************************************/
/*!
    Sleep mode on/off (caution! in SPI this need some more code!)
*/
/**************************************************************************/
void RA8875::sleep(bool sleep) {
	sleep == true ? writeReg(RA8875_PWRR, RA8875_PWRR_DISPOFF | RA8875_PWRR_SLEEP) : writeReg(RA8875_PWRR, RA8875_PWRR_DISPOFF);
}

/************************* Low Level ***********************************/

/**************************************************************************/
/*! PRIVATE
		Write in a register
		Parameters:
		reg: the register
		val: the data
*/
/**************************************************************************/
void  RA8875::writeReg(uint8_t reg, uint8_t val) {
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	SPI.beginTransaction(settings);
	writecommand_cont(RA8875_CMDWRITE);
	writecommand_last(reg);
	writecommand_cont(RA8875_DATAWRITE);
	writecommand_last(val);
	SPI.endTransaction();
#else
	writeCommand(reg);
	writeData(val);
#endif
}

/**************************************************************************/
/*! PRIVATE
		Returns the value inside register
		Parameters:
		reg: the register
*/
/**************************************************************************/
uint8_t  RA8875::readReg(uint8_t reg) {
	writeCommand(reg);
	return readData(false);
}

/**************************************************************************/
/*!
		Write data
		Parameters:
		d: the data
*/
/**************************************************************************/
void  RA8875::writeData(uint8_t data) {
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	SPI.beginTransaction(settings);
	writecommand_cont(RA8875_DATAWRITE);
	writecommand_last(data);
	SPI.endTransaction();
#else
	startSend();
	SPItranfer(RA8875_DATAWRITE);
	SPItranfer(data);
	endSend();
#endif

}

/**************************************************************************/
/*!
		Write 16 bit data
		Parameters:
		d: the data (16 bit)
*/
/**************************************************************************/
void  RA8875::writeData16(uint16_t data) {
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	SPI.beginTransaction(settings);
	writecommand_cont(RA8875_DATAWRITE);
	writecommand16_last(data);
	SPI.endTransaction();
#else
	startSend();
	SPItranfer(RA8875_DATAWRITE);
	SPItranfer(data >> 8);
	SPItranfer(data);
	endSend();
#endif

}

/**************************************************************************/
/*!	PRIVATE

*/
/**************************************************************************/
uint8_t  RA8875::readData(bool stat)
{
	//SPI.setClockDivider(SPI_CLOCK_DIV8);//2Mhz (3.3Mhz max)
	startSend();
	if (stat){
		SPItranfer(RA8875_CMDREAD);
	} else {
		SPItranfer(RA8875_DATAREAD);
	}
	uint8_t x = SPItranfer(0x0);
	endSend();
	return x;
}

/**************************************************************************/
/*!

*/
/**************************************************************************/
uint8_t  RA8875::readStatus(void) {
	return readData(true);
}

/**************************************************************************/
/*! PRIVATE
		Write a command
		Parameters:
		d: the command
*/
/**************************************************************************/
void RA8875::writeCommand(uint8_t d) {
#if defined _SPI_HYPERDRIVE && (defined(__MK20DX128__) || defined(__MK20DX256__))
	SPI.beginTransaction(settings);
	writecommand_cont(RA8875_CMDWRITE);
	writecommand_last(d);
	SPI.endTransaction();
#else
	startSend();
	SPItranfer(RA8875_CMDWRITE);
	SPItranfer(d);
	endSend();
#endif

}

/**************************************************************************/
/*! PRIVATE
		starts SPI communication
*/
/**************************************************************************/
void RA8875::startSend(){
	// set cs low
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_RESET);
}

/**************************************************************************/
/*! PRIVATE
		ends SPI communication
*/
/**************************************************************************/
void RA8875::endSend(){
	// set cs high
	HAL_GPIO_WritePin(cs_port, cs_pin, GPIO_PIN_SET);
}

// do actual SPI
uint8_t RA8875::SPItranfer(uint8_t d)
{
	uint8_t r= 0;
	HAL_StatusTypeDef s= HAL_SPI_TransmitReceive(hspi, &d, &r, 1, 100);
	if(s != HAL_OK) {
		printf("SPI transfer failed: %d\r\n", s);
	}
	return r;
}

#include "stdarg.h"
int RA8875::printf(const char* format, ...)
{
    va_list args;
    va_start(args, format);
    char buffer[132]; // max length for display anyway
    int n= vsnprintf(buffer, sizeof(buffer), format, args);
    va_end(args);
    this->textWrite(buffer, n);
    return n;
}
