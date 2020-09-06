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

#define MSGRAPH_DEBUG = 1

#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)

#define CONTEXT_FILE "/graph_context.json"			// Filename of the context file

#include <Arduino.h>
#include <vector>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "SPIFFS.h"

typedef struct {
	bool hasError;
	bool tokenNeedsRefresh;
	char message[257];
} GraphError;

typedef struct {
	char access_token[4097];
	char refresh_token[2049];	// https://docs.microsoft.com/en-us/linkedin/shared/authentication/programmatic-refresh-tokens#sample-response
	char id_token[4097];
	unsigned long expires;
} GraphAuthContext;

typedef struct {
	const char *name;
	const char *payload;
} GraphRequestHeader;

typedef struct {
	char id[37];
	char availability [33];
	char activity[33];
} GraphPresence;

typedef struct {
	char *dateTime;
	char *timeZone;
} GraphDate;

typedef struct {
	char *id;
	char *subject;
	char *bodyPreview;
	char *locationTitle;
	GraphDate startDate;
	GraphDate endDate;
} GraphEvent;


class ArduinoMSGraph {
public:
	Client *client;

	// Constructor
	ArduinoMSGraph(Client &client, const char *tenant, const char *clientId);

	// Generic Request Methods
	bool requestJsonApi(JsonDocument &doc, const char *url, const char *payload = "", const char *method = "POST", bool sendAuth = false, GraphRequestHeader extraHeader = { NULL, NULL });

	// Helper
	int getTokenLifetime();
	GraphError getLastError();

	// SPIFFS Helper
	bool saveContextToSPIFFS();
	bool readContextFromSPIFFS();
	bool removeContextFromSPIFFS();

	// Authentication Methods
	bool startDeviceLoginFlow(JsonDocument &doc, const char *scope = "offline_access%20openid%20Presence.Read");
	bool pollForToken(JsonDocument &doc, const char *device_code);
	bool refreshToken();

	// Graph Data Methods
	GraphPresence getUserPresence();
	std::vector<GraphEvent> getUserEvents(int count = 3, const char *timezone = "Europe/Berlin");

private:
	const char *_clientId;
	const char *_tenant;

	GraphAuthContext _context;
	GraphError _lastError;
};

#endif