/*
	Copyright (c) 2020 Tobias Blum. All rights reserved.

	ArduinoMSGraph - A library to wrap the Microsoft Graph API (supports ESP32 & possibly others)
	https://github.com/toblum/ArduinoMSGraph

	Example: Authentication flow and polling presence information

	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

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

enum STATES {
	no_context,
	context_available,
	wait_login,
	token_needs_refresh
};
STATES currentState = no_context;

void setup()
{
	Serial.begin(115200);

	// Init SPIFFS
	if(!SPIFFS.begin(true)){
		Serial.println("An Error has occurred while mounting SPIFFS");
		return;
	}

	bool got_context = graphClient.readContextFromSPIFFS();
	if (got_context) {
		currentState = token_needs_refresh;
	}

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
}

void loop()
{
	if (currentState == no_context) {
		DBG_PRINTLN("##########################################");
		DBG_PRINTLN("STATE: no_context");
		// Start device login flow
		graphClient.startDeviceLoginFlow(deviceCodeDoc, "offline_access%20openid%20Presence.Read%20Calendars.Read");

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

		currentState = wait_login;
	}

	if (currentState == wait_login) {
		DBG_PRINTLN("##########################################");
		DBG_PRINTLN("STATE: wait_login");
		bool res = graphClient.pollForToken(pollingDoc, deviceCode);

		if (res) {
			DBG_PRINTLN("GOT ACCESS TOKEN! Yay!");
			DBG_PRINTLN(pollingDoc["access_token"].as<String>());

			graphClient.saveContextToSPIFFS();
			currentState = context_available;
		} else {
			DBG_PRINTLN("No token received, continue polling.");
			delay(10000);
		}
	}

	if (currentState == context_available) {
		DBG_PRINTLN("##########################################");
		DBG_PRINTLN("STATE: context_available");

		GraphPresence gp = graphClient.getUserPresence();
		GraphError gpe = graphClient.getLastError();
		if (!gpe.hasError) {
			DBG_PRINT("PRESENCE: ");
			DBG_PRINT(gp.activity);
			DBG_PRINT(" - ");
			DBG_PRINTLN(gp.activity);
		} else {
			DBG_PRINT("GPE error: ");
			DBG_PRINTLN(gpe.message);
			if (gpe.tokenNeedsRefresh) {
				currentState = token_needs_refresh;
			}
		}

		std::vector<GraphEvent> events = graphClient.getUserEvents(5, "Europe/Paris");
		GraphError gee = graphClient.getLastError();
		if (!gee.hasError) {
			for (int i = 0; i < events.size(); i++) {
				DBG_PRINT(events[i].startDate.dateTime);
				DBG_PRINT(" - ");
				DBG_PRINTLN(events[i].subject);
			}
		} else {
			DBG_PRINT("GEE error: ");
			DBG_PRINTLN(gee.message);
			if (gee.tokenNeedsRefresh) {
				currentState = token_needs_refresh;
			}
		}
		delay(15000);
	}

	if (currentState == token_needs_refresh) {
		DBG_PRINTLN("##########################################");
		DBG_PRINTLN("STATE: token_needs_refresh");
		Serial.print("ESP.getFreeHeap(): ");
		Serial.println(ESP.getFreeHeap());
		bool res = graphClient.refreshToken();
		if (res) {
			graphClient.saveContextToSPIFFS();
			currentState = context_available;
		} else {
			delay(30000);
		}
	}
}