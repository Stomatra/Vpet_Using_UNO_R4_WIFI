#include <Mouse.h>
#include <SPI.h>
#include <gfxfont.h>
#include <Adafruit_SPITFT_Macros.h>
#include <Adafruit_SPITFT.h>
#include <Adafruit_GrayOLED.h>
#include <Adafruit_GFX.h>
#include <XPT2046_Touchscreen.h>
#include <Adafruit_ST77xx.h>
#include <Adafruit_ST7796S.h>
#include <Adafruit_ST7789.h>
#include <Adafruit_ST7735.h>
#include <Arduino.h>
#include <SD.h>

#define CS_PIN  4
#define TIRQ_PIN  3
// 如果不用中断，建议如下初始化
XPT2046_Touchscreen ts(CS_PIN, -1);

#if defined(ARDUINO_FEATHER_ESP32) // Feather Huzzah32
#define TFT_CS         14
#define TFT_RST        15
#define TFT_DC         32

#elif defined(ESP8266)
#define TFT_CS         4
#define TFT_RST        16
#define TFT_DC         5

#else
#define TFT_CS        10
#define TFT_RST        -1
#define TFT_DC         7
#endif

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

static uint16_t count = 0;
static uint16_t iscontrol = 0;

class Status {
private:
	bool touched;
	uint16_t touchCount;
	uint16_t touched_state;
public:
	Status() : touched(false), touchCount(0) {
	}

	void update() {
		if (touched) {
			touched = false;
		}
	}

	bool isTouched() const {
		return touched;
	}

	uint16_t getTouchCount() const {
		return touchCount;
	}

	uint16_t getTouchedState() const {
		return touched_state;
	}

	uint16_t touch() {
		touched = true;
		touchCount++;
		touched_state = random(0, 5);//max status num is 5
		return touched_state;
	}

	void setTouchedState(uint16_t state) {
		touched_state = state;
	}

	void resetTouchCount() {
		touchCount = 0;
	}
};

static Status status;

const char* imgFiles[] = {
	"img0.bmp",
	"img1.bmp",
	"img2.bmp",
	"img3.bmp",
	"img4.bmp"
};

void DrawImage(Status status) {
	uint16_t s = status.getTouchedState();
	if (s < 5) {
		drawBmp(imgFiles[s], 0, 0);
	}
}

// BMP pic showing function(simpilfied version from Adafruit)
void drawBmp(const char* filename, int x, int y) {
	File bmpFile;
	int bmpWidth, bmpHeight;
	uint8_t bmpDepth;
	uint32_t bmpImageoffset;
	uint32_t rowSize;
	uint8_t sdbuffer[3 * 320];
	bool goodBmp = false;

	Serial.print("Try to open the pic file: ");
	Serial.println(filename);

	bmpFile = SD.open(filename);
	if (!bmpFile) {
		Serial.print("Cannot open the pic file: ");
		Serial.println(filename);
		return;
	}

	if (read16(bmpFile) == 0x4D42) {
		Serial.println("BMP is correct");
		(void)read32(bmpFile);
		(void)read32(bmpFile);
		bmpImageoffset = read32(bmpFile);
		(void)read32(bmpFile);
		bmpWidth = read32(bmpFile);
		bmpHeight = read32(bmpFile);
		Serial.print("pic size: ");
		Serial.print(bmpWidth);
		Serial.print("x");
		Serial.println(bmpHeight);
		if (read16(bmpFile) == 1) {
			bmpDepth = read16(bmpFile);
			Serial.print("color depth: ");
			Serial.println(bmpDepth);
			if (bmpDepth == 24 && read32(bmpFile) == 0) {
				goodBmp = true;
				rowSize = (bmpWidth * 3 + 3) & ~3;
				bmpFile.seek(bmpImageoffset);

				for (int row = 0; row < bmpHeight; row++) {
					bmpFile.read(sdbuffer, rowSize);
					uint8_t* buffptr = sdbuffer;
					for (int col = 0; col < bmpWidth; col++) {
						uint8_t b = *buffptr++;
						uint8_t g = *buffptr++;
						uint8_t r = *buffptr++;
						uint16_t color = tft.color565(r, g, b);
					}
				}
				Serial.println("pic showing finished");
			}
		}
	}
	bmpFile.close();
	if (!goodBmp) Serial.println("BMP format is not approved or failed to parse");
}
// read 16 or 32 bits from the SD card file
uint16_t read16(File& f) {
	uint16_t result;
	((uint8_t*)&result)[0] = f.read();
	((uint8_t*)&result)[1] = f.read();
	return result;
}
uint32_t read32(File& f) {
	uint32_t result;
	((uint8_t*)&result)[0] = f.read();
	((uint8_t*)&result)[1] = f.read();
	((uint8_t*)&result)[2] = f.read();
	((uint8_t*)&result)[3] = f.read();
	return result;
}

// the setup function runs once when you press reset or power the board
void setup() {
	Serial.begin(115200);
	Serial.println(F("Starting"));
	Mouse.begin();

	pinMode(D9, OUTPUT);
	digitalWrite(D9, HIGH);

	pinMode(TFT_CS, OUTPUT);
	digitalWrite(TFT_CS, HIGH); // 屏幕片选拉高，防止干扰
	pinMode(CS_PIN, OUTPUT);
	digitalWrite(CS_PIN, HIGH); // SD卡片选拉高，初始化前先拉高

	// 先初始化SD卡
	if (!SD.begin(CS_PIN)) {
		Serial.println("SD卡初始化失败！");
		while (1);
	}
	Serial.println("SD卡初始化成功");

	// 再初始化屏幕和触摸
	ts.begin();
	ts.setRotation(3);
	tft.init(240, 320);
	Serial.println(F("屏幕初始化完成"));

	status.setTouchedState(0);
	DrawImage(status);
	delay(5000);
	tft.setRotation(1);
}

// the loop function runs over and over again until power down or reset
void loop() {
	if (ts.touched()) {
		TS_Point p = ts.getPoint();
		Serial.print("Touch detected at: ");
		Serial.print(p.x);
		Serial.print(", ");
		Serial.println(p.y);
		status.touch();
		if (status.getTouchCount() == 5) {
			//触摸5次，咬住鼠标2秒
			DrawImage(status);
			for (int i = 0; i < 200; i++) {
				Mouse.move(100, 100);
				delay(10);
			}
			status.setTouchedState(0);
			status.resetTouchCount();
			status.update();
			DrawImage(status);
			delay(5000);
		}
		else {
			status.setTouchedState(random(1, 4));
			delay(2000);
			status.setTouchedState(0);
			status.update();
			DrawImage(status);
			delay(5000);
		}
	}
}