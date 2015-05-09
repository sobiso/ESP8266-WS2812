#include <user_config.h>
#include <SmingCore/SmingCore.h>
#include <WS2812.h>

// Put you SSID and Password here
#define WIFI_SSID "gospo"
#define WIFI_PWD ""

HttpServer server;
FTPServer ftp;
Timer procTimer;

int inputs[] = {0, 2}; // Set input GPIO pins here
Vector<String> namesInput;
const int countInputs = sizeof(inputs) /  sizeof(inputs[0]);
char buffer1[] = "\xff\xff\xff\xff\x00\x00\x00\x00\x40";
char WS_buffer[] = {0, 0, 0};




void readCompass();

void onIndex(HttpRequest &request, HttpResponse &response)
{
	TemplateFileStream *tmpl = new TemplateFileStream("index.html");
	auto &vars = tmpl->variables();
	//vars["counter"] = String(counter);
	response.sendTemplate(tmpl); // this template object will be deleted automatically
}

void onFile(HttpRequest &request, HttpResponse &response)
{
	String file = request.getPath();
	if (file[0] == '/')
		file = file.substring(1);

	if (file[0] == '.')
		response.forbidden();
	else
	{
		response.setCache(86400, true); // It's important to use cache for better performance.
		response.sendFile(file);
	}
}

void onAjaxInput(HttpRequest &request, HttpResponse &response)
{
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	json["status"] = (bool)true;

	JsonObject& gpio = json.createNestedObject("gpio");
	for (int i = 0; i < countInputs; i++)
		gpio[namesInput[i].c_str()] = digitalRead(inputs[i]);

	response.sendJsonObject(stream);
}

void onAjaxFrequency(HttpRequest &request, HttpResponse &response)
{
	int freq = request.getQueryParameter("value").toInt();
	System.setCpuFrequency((CpuFrequency)freq);

	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();
	json["status"] = (bool)true;
	json["value"] = (int)System.getCpuFrequency();

	response.sendJsonObject(stream);
}

void onAjaxRGB(HttpRequest &request, HttpResponse &response)
{
	if (request.getRequestMethod() == RequestMethod::POST)
		{
			debugf("Update config");
			// Update config
			if (request.getPostParameter("R").length() > 0) // Network
			{
				int R = request.getPostParameter("R").toInt();
				int G = request.getPostParameter("G").toInt();
				int B = request.getPostParameter("B").toInt();

				WS_buffer[0] = char(R);
				WS_buffer[1] = char(G);			
				WS_buffer[2] = char(B);

				ws2812_writergb(12, WS_buffer, sizeof(WS_buffer));

			}
		}
	JsonObjectStream* stream = new JsonObjectStream();
	JsonObject& json = stream->getRoot();		
	response.sendJsonObject(stream);
}

void startWebServer()
{
	server.listen(80);
	server.addPath("/", onIndex);
	server.addPath("/ajax/input", onAjaxInput);
	server.addPath("/ajax/frequency", onAjaxFrequency);
	server.addPath("/ajax/rgb", onAjaxRGB);
	server.setDefaultHandler(onFile);

	Serial.println("\r\n=== WEB SERVER STARTED ===");
	Serial.println(WifiStation.getIP());
	Serial.println("==============================\r\n");
}

void startFTP()
{
	if (!fileExist("index.html"))
		fileSetContent("index.html", "<h3>Please connect to FTP and upload files from folder 'web/build' (details in code)</h3>");

	// Start FTP server
	ftp.listen(21);
	ftp.addUser("me", "123"); // FTP account
}

// Will be called when WiFi station was connected to AP
void connectOk()
{
	Serial.println("I'm CONNECTED");

	startFTP();
	startWebServer();
}

void init()
{
	Serial.begin(SERIAL_BAUD_RATE); // 115200 by default
	Serial.systemDebugOutput(true); // Enable debug output to serial

	WifiStation.enable(true);
	WifiStation.config(WIFI_SSID, WIFI_PWD);
	WifiAccessPoint.enable(false);

	for (int i = 0; i < countInputs; i++)
	{
		namesInput.add(String(inputs[i]));
		pinMode(inputs[i], INPUT);
	}

	// Run our method when station was connected to AP
	WifiStation.waitConnection(connectOk);

	
    ws2812_writergb(12, buffer1, sizeof(buffer1));
  	

}

// =====================================================================================================================================================================================
static void ICACHE_FLASH_ATTR send_ws_0(uint8_t gpio)
{
    uint8_t i;
    i = 4; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
    i = 9; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}

static void ICACHE_FLASH_ATTR send_ws_1(uint8_t gpio)
{
    uint8_t i;
    i = 8; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TS_ADDRESS, 1 << gpio);
    i = 6; while (i--) GPIO_REG_WRITE(GPIO_OUT_W1TC_ADDRESS, 1 << gpio);
}

// Byte triples in the buffer are interpreted as R G B values and sent to the hardware as G R B.
int ICACHE_FLASH_ATTR ws2812_writergb(uint8_t gpio, char *buffer, size_t length)
{
    // Initialize the output pin:
    pinMode(gpio, OUTPUT);
    digitalWrite(gpio, 0);

    // Ignore incomplete Byte triples at the end of buffer:
    length -= length % 3;

    // Send the buffer:
    os_intr_lock();
    const char * const end = buffer + length;
    while (buffer != end) {
        uint8_t mask = 0x80;
        while (mask) {
            (*buffer & mask) ? send_ws_1(gpio) : send_ws_0(gpio);
            mask >>= 1;
        }
        ++buffer;
    }
    os_intr_unlock();
}