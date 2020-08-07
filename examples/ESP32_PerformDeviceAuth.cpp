#include <Arduino.h>
#include <ArduinoMSGraph.h>
#include <WiFiClientSecure.h>

char ssid[] = "";		// your network SSID (name)
char password[] = "";	// your network password
char clientId[] = ""; 	// Azure app client id
char tenant[] = "contoso.onmicrosoft.com"; // Tenant guid or name

WiFiClientSecure client;
ArduinoMSGraph graphClient(client, tenant, clientId);

void setup()
{
	Serial.begin(115200);

	WiFi.mode(WIFI_STA);
	WiFi.begin(ssid, password);
	Serial.println("");

	// Wait for connection
	while (WiFi.status() != WL_CONNECTED)
	{
		delay(500);
		Serial.print(".");
	}
	Serial.println("");
	Serial.print("Connected to ");
	Serial.println(ssid);
	Serial.print("IP address: ");
	IPAddress ipAddress = WiFi.localIP();
	Serial.println(ipAddress);

	const size_t capacity = JSON_OBJECT_SIZE(6) + 540;
	DynamicJsonDocument doc(capacity);
	graphClient.startDeviceLoginFlow(doc);

	Serial.println(doc.as<String>());
}

void loop()
{
}