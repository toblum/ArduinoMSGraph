#include <Arduino.h>
#include <ArduinoMSGraph.h>
#include <WiFiClientSecure.h>

char ssid[] = "";							  // your network SSID (name)
char password[] = "";					  // your network password
char clientId[] = ""; // your network password

WiFiClientSecure client;
ArduinoMSGraph graphClient(client, clientId);

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

	// client.setCACert(rootCACertificateLogin);

	DynamicJsonDocument doc(10000);
	boolean res = graphClient.requestJsonApi(doc, "https://login.microsoftonline.com/contoso.onmicrosoft.com/oauth2/v2.0/devicecode", "client_id=6cc6d040-79b6-48c7-ae23-1c6c436428ce&scope=offline_access%20openid%20Presence.Read");

	Serial.println(doc.as<String>());
}

void loop()
{
}