#include <Arduino.h>
#include <ArduinoMSGraph.h>
#include <WiFiClientSecure.h>

#include "credentials.h"

WiFiClientSecure client;
ArduinoMSGraph graphClient(client, tenant, clientId);

bool tokenReceived = false;
const char *deviceCode;

const size_t capacity = JSON_OBJECT_SIZE(6) + 540;
DynamicJsonDocument deviceCodeDoc(capacity);
DynamicJsonDocument pollingDoc(10000);

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

	// Start device login flow
	graphClient.startDeviceLoginFlow(deviceCodeDoc);

	// Serial.println(doc.as<String>());

	// Consume result
	deviceCode = deviceCodeDoc["device_code"].as<const char*>();
	const char *user_code = deviceCodeDoc["user_code"].as<const char*>();
	const char *verification_uri = deviceCodeDoc["verification_uri"].as<const char*>();
	const char *message = deviceCodeDoc["message"].as<const char*>();

	Serial.print("deviceCode: ");
	Serial.println(deviceCode);
	Serial.print("user_code: ");
	Serial.println(user_code);
	Serial.print("verification_uri: ");
	Serial.println(verification_uri);
	Serial.print("message: ");
	Serial.println(message);
}

void loop()
{
	if (!tokenReceived) {
		DBG_PRINTLN();
		DBG_PRINTLN("##########################################");
		bool res = graphClient.pollForToken(pollingDoc, deviceCode);

		if (res) {
			DBG_PRINTLN("GOT ACCESS TOKEN!");
			DBG_PRINTLN(pollingDoc["access_token"].as<String>());
			tokenReceived = true;
		} else {
			DBG_PRINTLN("No token received, continue polling.");
			delay(10000);
		}
	}
}