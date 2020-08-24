/*
	Copyright (c) 2020 Tobias Blum. All rights reserved.

	ArduinoMSGraph - A library to wrap the Microsoft Graph API (supports ESP32 & possibly others)
	https://github.com/toblum/ArduinoMSGraph

	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#ifndef ArduinoMSGraph_h
#define ArduinoMSGraph_h

// #define MSGRAPH_DEBUG = 1

#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)

#define CONTEXT_FILE "/graph_context.json"			// Filename of the context file

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "SPIFFS.h"

struct GraphError {
	bool hasError;
	bool tokenNeedsRefresh;
	char message[256];
};

struct GraphAuthContext {
	char access_token[4096];
	char refresh_token[2048];	// https://docs.microsoft.com/en-us/linkedin/shared/authentication/programmatic-refresh-tokens#sample-response
	char id_token[4096];
	unsigned long expires;
};

struct GraphPresence {
	char id[37];
	char availability [33];
	char activity[33];

	GraphError error;
};


class ArduinoMSGraph {
public:
	Client *client;

	// Constructor
	ArduinoMSGraph(Client &client, const char *tenant, const char *clientId);

	// Generic Request Methods
	bool requestJsonApi(JsonDocument &doc, const char *url, const char *payload = "", const char *method = "POST", bool sendAuth = false);

	// Helper
	int getTokenLifetime();

	// SPIFFS Helper
	bool saveContextToSPIFFS();
	bool readContextFromSPIFFS();
	bool removeContextFromSPIFFS();

	// Authentication Methods
	bool startDeviceLoginFlow(JsonDocument &doc, const char *scope = "offline_access%20openid%20Presence.Read");
	bool pollForToken(JsonDocument &doc, const char *device_code);
	bool refreshToken();

	// Graph Presence Methods
	GraphPresence getUserPresence();

private:
	const char *_clientId;
	const char *_tenant;

	GraphAuthContext _context;
};

#endif