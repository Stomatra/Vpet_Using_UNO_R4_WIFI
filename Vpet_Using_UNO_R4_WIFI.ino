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
XPT2046_Touchscreen ts(CS_PIN, TIRQ_PIN);  // Param 2 - Touch IRQ Pin - interrupt enabled polling

#if defined(ARDUINO_FEATHER_ESP32) // Feather Huzzah32
#define TFT_CS         14
#define TFT_RST        15
#define TFT_DC         32

#elif defined(ESP8266)
#define TFT_CS         4
#define TFT_RST        16
#define TFT_DC         5

#else
// For the breakout board, you can use any 2 or 3 pins.
// These pins will also work for the 1.8" TFT shield.
#define TFT_CS        10
#define TFT_RST        -1
#define TFT_DC         7
#endif

Adafruit_ST7789 tft = Adafruit_ST7789(TFT_CS, TFT_DC, TFT_RST);

/*
要实现的功能：使用UNO R4 WIFI实现一个虚拟桌宠
1.在桌宠启动的时候，可以显示一个图片，或者如果允许，可以显示一个动画，在未操作的时候一直是这样的
2.当触摸屏幕的时候，桌宠可以显示出另外的图片或者动画，之后过几秒回到原来的状态
3.当触摸次数过多的时候，桌宠会“气急败坏”，然后会“咬住”你的鼠标几秒（此时你的鼠标动不了），然后回到初始状态
*/

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
		touched_state = random(0, 5);//����״̬������5
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

// BMPͼƬ��ʾ�������򻯰棬������24λBMP��320x240��
void drawBmp(const char* filename, int x, int y) {
	File bmpFile;
	int bmpWidth, bmpHeight;   // ͼƬ���
	uint8_t bmpDepth;          // ɫ��
	uint32_t bmpImageoffset;   // ����ƫ��
	uint32_t rowSize;          // ÿ���ֽ���
	uint8_t sdbuffer[3 * 320];   // һ�����ػ���
	uint8_t buffidx = sizeof(sdbuffer);
	bool goodBmp = false;

	bmpFile = SD.open(filename);
	if (!bmpFile) {
		Serial.print("�Ҳ���ͼƬ�ļ�: ");
		Serial.println(filename);
		return;
	}

	// BMPͷ����
	if (read16(bmpFile) == 0x4D42) { // BMPǩ��
		(void)read32(bmpFile); // �ļ���С
		(void)read32(bmpFile); // ����
		bmpImageoffset = read32(bmpFile); // ����ƫ��
		(void)read32(bmpFile); // DIBͷ��С
		bmpWidth = read32(bmpFile);
		bmpHeight = read32(bmpFile);
		if (read16(bmpFile) == 1) { // ɫ����
			bmpDepth = read16(bmpFile); // ɫ��
			if (bmpDepth == 24 && read32(bmpFile) == 0) { // ��֧��24λ��ѹ��
				goodBmp = true;
				rowSize = (bmpWidth * 3 + 3) & ~3;
				bmpFile.seek(bmpImageoffset);

				for (int row = 0; row < bmpHeight; row++) {
					bmpFile.read(sdbuffer, rowSize);
					uint8_t* buffptr = sdbuffer;
					for (int col = 0; col < bmpWidth; col++) {
						// BMPΪBGR��ʽ
						uint8_t b = *buffptr++;
						uint8_t g = *buffptr++;
						uint8_t r = *buffptr++;
						uint16_t color = tft.color565(r, g, b);
						tft.drawPixel(x + col, y + bmpHeight - row - 1, color);
					}
				}
			}
		}
	}
	bmpFile.close();
	if (!goodBmp) Serial.println("BMP��ʽ��֧�ֻ����ʧ��");
}

// ��������
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
	Mouse.begin();

	pinMode(D9, OUTPUT);
	digitalWrite(D9, HIGH);

	ts.begin();
	ts.setRotation(3);
	tft.init(240, 320);
	Serial.println(F("Initialized"));

	if (!SD.begin()) {
		Serial.println("SD����ʼ��ʧ�ܣ�");
		while (1);
	}
	Serial.println("SD����ʼ���ɹ�");

	status.setTouchedState(0);
	DrawImage(status);
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
			Mouse.press(MOUSE_LEFT);
			delay(2000);
			Mouse.release(MOUSE_LEFT);
			status.setTouchedState(0);
			status.resetTouchCount();
			status.update();
		}
		else {
			status.setTouchedState(random(1, 4));
			delay(2000);
			status.setTouchedState(0);
			status.update();
		}
	}
}