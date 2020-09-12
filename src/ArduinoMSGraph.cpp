/*
	Copyright (c) 2020 Tobias Blum. All rights reserved.

	ArduinoMSGraph - A library to wrap the Microsoft Graph API (supports ESP32 & possibly others)
	https://github.com/toblum/ArduinoMSGraph

	This Source Code Form is subject to the terms of the Mozilla Public
	License, v. 2.0. If a copy of the MPL was not distributed with this
	file, You can obtain one at https://mozilla.org/MPL/2.0/.
*/

#include "ArduinoMSGraph.h"
#include "ArduinoMSGraphCerts.h"

/**
 * Create a new ArduinoMSGraph instance
 * 
 * @param client WiFiClient to pass to ArduinoMSGraph
 * @param tenant GUID or name of the tenant (e.g. contoso.onmicrosoft.com)
 * @param clientID Client ID of the Azure AD app
 */
ArduinoMSGraph::ArduinoMSGraph(const char *tenant, const char *clientId) {
    this->_tenant = tenant;
    this->_clientId = clientId;
}


/**
 * Perform a HTTP request, optionally with authentication information.
 * 
 * @param responseDoc JsonDocument passed as reference to hold the result.
 * @param url URL to request
 * @param payload Raw payload to send together with the request.
 * @param method Method for the HTTP request: GET, POST, ...
 * @param sendAuth If true, send the Bearer token together with the request.
 * 
 * @returns True if request successful, false on error.
 */
bool ArduinoMSGraph::requestJsonApi(JsonDocument& responseDoc, const char *url, const char *payload, const char *method, bool sendAuth, GraphRequestHeader extraHeader) {
	const char* cert;
	if (strstr(url, "graph.microsoft.com") != NULL) {
		cert = rootCACertificateGraph;
	} else {
		cert = rootCACertificateLogin;
	}

	#ifdef MSGRAPH_DEBUG
		DBG_PRINT("ESP.getFreeHeap(): ");
		DBG_PRINTLN(ESP.getFreeHeap());
	#endif

	// HTTPClient
	HTTPClient https;

	// Prepare empty response
	const int emptyCapacity = JSON_OBJECT_SIZE(1);
	DynamicJsonDocument emptyDoc(emptyCapacity);

	// DBG_PRINT("[HTTPS] begin...\n");
    if (https.begin(url, cert)) {
		https.setConnectTimeout(10000);
		https.setTimeout(10000);

		// Send auth header?
		if (sendAuth) {
			char authHeader[strlen(_context.access_token) + 8];
			sprintf(authHeader, "Bearer %s", _context.access_token);
			https.addHeader("Authorization", authHeader);
			if (extraHeader.name != NULL && strlen(extraHeader.name) > 0) {
				https.addHeader(extraHeader.name, extraHeader.payload);
			}
			#ifdef MSGRAPH_DEBUG
				Serial.printf("requestJsonApi() - Auth token valid for %d s.\n", getTokenLifetime());
			#endif
		}

		// Start connection and send HTTP header
		int httpCode = https.sendRequest(method, payload);

		// httpCode will be negative on error
		if (httpCode > 0) {
			// HTTP header has been send and Server response header has been handled
			#ifdef MSGRAPH_DEBUG
				Serial.printf("requestJsonApi() - Method: %s, Response code: %d\n", method, httpCode);
			#endif

			// File found at server (HTTP 200, 301), or HTTP 400, 401 with response payload
			if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_BAD_REQUEST || httpCode == HTTP_CODE_UNAUTHORIZED) {
				// if (strstr(url, "events") != NULL) {
				// 	String res = https.getString();
				// 	DBG_PRINTLN(res);
				// }

				// Parse JSON data
				DeserializationError error = deserializeJson(responseDoc, https.getStream());
				if (error) {
					DBG_PRINT(F("requestJsonApi() - deserializeJson() failed: "));
					DBG_PRINTLN(error.c_str());
					https.end();
					return false;
				} else {
					https.end();
					return true;
				}
			} else {
				Serial.printf("requestJsonApi() - Other HTTP code: %d\nResponse: ", httpCode);
				DBG_PRINTLN(https.getString());
				https.end();
				return false;
			}
		} else {
			Serial.printf("requestJsonApi() - Request failed: %s\n", https.errorToString(httpCode).c_str());
			https.end();
			return false;
		}
    } else {
    	DBG_PRINTLN(F("requestJsonApi() - Unable to connect"));
		return false;
    }
}


/**
 * Start the device login flow and request login page data.
 * 
 * @param responseDoc JsonDocument passed as reference to hold the result.
 * Reserve a size of at least: JSON_OBJECT_SIZE(6) + 540
 * @param scope The scope to request from Azure AD.
 * 
 * @returns True if request successful, false on error
 */
bool ArduinoMSGraph::startDeviceLoginFlow(JsonDocument &responseDoc, const char *scope) {
	#ifdef MSGRAPH_DEBUG
		DBG_PRINT(F("startDeviceLoginFlow() - Scope: "));
		DBG_PRINTLN(scope);
	#endif

	char url[58 + strlen(this->_tenant)];
    sprintf(url,"https://login.microsoftonline.com/%s/oauth2/v2.0/devicecode", this->_tenant);
	char payload[18 + strlen(this->_clientId) + strlen(scope)];
    sprintf(payload,"client_id=%s&scope=%s", this->_clientId, scope);

	bool res = requestJsonApi(responseDoc, url, payload);
	return res;
}


/**
 * Poll for the authentication token. Do this until the user has completed authentication.
 * 
 * @param responseDoc JsonDocument passed as reference to hold the result.
 * Reserve a size of at least: JSON_ARRAY_SIZE(1) + JSON_OBJECT_SIZE(7) + 530, better: 10000
 * @param device_code The device code to poll for.
 * 
 * @returns True if token is available, if false, continue polling.
 */
bool ArduinoMSGraph::pollForToken(JsonDocument &responseDoc, const char *device_code) {
	#ifdef MSGRAPH_DEBUG
		DBG_PRINTLN(F("pollForToken()"));
	#endif

	char url[53 + strlen(this->_tenant)];
    sprintf(url,"https://login.microsoftonline.com/%s/oauth2/v2.0/token", this->_tenant);
	char payload[80 + strlen(this->_clientId) + strlen(device_code)];
    sprintf(payload,"client_id=%s&grant_type=urn:ietf:params:oauth:grant-type:device_code&device_code=%s", this->_clientId, device_code);

	bool res = requestJsonApi(responseDoc, url, payload);

	if (!res) {
		return false;
	} else if (responseDoc.containsKey("error")) {
		const char* _error = responseDoc["error"];
		const char* _error_description = responseDoc["error_description"];

		if (strcmp(_error, "authorization_pending") == 0) {
			Serial.printf("pollForToken() - Wating for authorization by user: %s\n\n", _error_description);
		} else {
			Serial.printf("pollForToken() - Unexpected error: %s, %s\n\n", _error, _error_description);
		}
		return false;
	} else {
		if (responseDoc.containsKey("access_token") && responseDoc.containsKey("refresh_token")) {
			// Store tokens in context
			_context.access_token = strdup(responseDoc["access_token"].as<char *>());
			_context.refresh_token = strdup(responseDoc["refresh_token"].as<char *>());
			_context.id_token = strdup(responseDoc["id_token"].as<char *>());
			unsigned int _expires_in = responseDoc["expires_in"].as<unsigned long>();
			_context.expires = millis() + (_expires_in * 1000); // Calculate timestamp when token expires

			return true;
		} else {
			DBG_PRINTLN("pollForToken() - Not all expected keys (access_token, refresh_token) found.");
			return false;
		}
	}
}


/**
 * Refresh the access_token using the refresh_token.
 * 
 * @returns True if refresh successful, false on error.
 */
bool ArduinoMSGraph::refreshToken() {
	#ifdef MSGRAPH_DEBUG
		DBG_PRINTLN(F("refreshToken()"));
		// DBG_PRINTLN(_context.refresh_token);
		DBG_PRINTLN(strlen(_context.refresh_token));
	#endif
	// See: https://docs.microsoft.com/de-de/azure/active-directory/develop/v1-protocols-oauth-code#refreshing-the-access-tokens

	bool success = false;
	char url[53 + strlen(this->_tenant)];
    sprintf(url, "https://login.microsoftonline.com/%s/oauth2/v2.0/token", this->_tenant);

	char payload[51 + strlen(this->_clientId) + strlen(_context.refresh_token)];
    sprintf(payload, "client_id=%s&grant_type=refresh_token&refresh_token=%s", this->_clientId, _context.refresh_token);

	const size_t capacity = JSON_OBJECT_SIZE(7) + 10000;
	DynamicJsonDocument responseDoc(capacity);

	bool res = requestJsonApi(responseDoc, url, payload);

	// Replace tokens and expiration
	if (res && responseDoc.containsKey("access_token") && responseDoc.containsKey("refresh_token")) {
		if (!responseDoc["access_token"].isNull()) {
			_context.access_token = strdup(responseDoc["access_token"].as<char *>());
			success = true;
		}
		if (!responseDoc["refresh_token"].isNull()) {
			_context.refresh_token = strdup(responseDoc["refresh_token"].as<char *>());
			success = true;
		}
		if (!responseDoc["id_token"].isNull()) {
			_context.id_token = strdup(responseDoc["id_token"].as<char *>());
		}
		if (!responseDoc["expires_in"].isNull()) {
			int _expires_in = responseDoc["expires_in"].as<unsigned long>();
			_context.expires = millis() + (_expires_in * 1000); // Calculate timestamp when token expires
		}

		#ifdef MSGRAPH_DEBUG
			DBG_PRINTLN(F("refreshToken() - Success"));
		#endif
	} else {
		DBG_PRINT(F("refreshToken() - Error: "));
		DBG_PRINTLN(responseDoc["error_description"].as<const char*>());
	}
	return success;
}


/**
 * Save current Graph context in a JSON file in SPIFFS.
 * 
 * @returns True if saving was successful.
 */
bool ArduinoMSGraph::saveContextToSPIFFS() {
	const size_t capacity = JSON_OBJECT_SIZE(3) + 5000;
	DynamicJsonDocument contextDoc(capacity);

	contextDoc["access_token"] = _context.access_token;
	contextDoc["refresh_token"] = _context.refresh_token;
	contextDoc["id_token"] = _context.id_token;

	File contextFile = SPIFFS.open(CONTEXT_FILE, FILE_WRITE);
	size_t bytesWritten = serializeJsonPretty(contextDoc, contextFile);
	contextFile.close();

	#ifdef MSGRAPH_DEBUG
		DBG_PRINT(F("saveContextInSPIFFS() - Success - Bytes written: "));
		DBG_PRINTLN(bytesWritten);
		// DBG_PRINTLN(contextDoc.as<String>());
	#endif

	return bytesWritten > 0;
}


/**
 * Try to restote Graph context from SPIFFS
 * 
 * @returns True if restore was successful.
 */
bool ArduinoMSGraph::readContextFromSPIFFS() {
	File file = SPIFFS.open(CONTEXT_FILE);
	bool success = false;

	if (!file) {
		DBG_PRINTLN(F("readContextFromSPIFFS() - No file found"));
	} else {
		size_t size = file.size();
		if (size == 0) {
			DBG_PRINTLN(F("readContextFromSPIFFS() - File empty"));
		} else {
			const int capacity = JSON_OBJECT_SIZE(3) + 5000;
			DynamicJsonDocument contextDoc(capacity);
			DeserializationError err = deserializeJson(contextDoc, file);

			if (err) {
				DBG_PRINT(F("readContextFromSPIFFS() - deserializeJson() failed with code: "));
				DBG_PRINTLN(err.c_str());
			} else {
				int numSettings = 0;
				if (!contextDoc["access_token"].isNull()) {
					_context.access_token = strdup(contextDoc["access_token"].as<char *>());
					numSettings++;
				}
				if (!contextDoc["refresh_token"].isNull()) {
					_context.refresh_token = strdup(contextDoc["refresh_token"].as<char *>());
					numSettings++;
				}
				if (!contextDoc["id_token"].isNull()){
					_context.id_token = strdup(contextDoc["id_token"].as<char *>());
				}
				_context.expires = 0;
				if (numSettings >= 2) {
					#ifdef MSGRAPH_DEBUG
						DBG_PRINTLN(F("readContextFromSPIFFS() - Success"));
					#endif
					success = true;
				} else {
					Serial.printf("readContextFromSPIFFS() - ERROR Number of valid settings in file: %d, should greater or equals 2.\n", numSettings);
				}
				// DBG_PRINTLN(contextDoc.as<String>());
			}
		}
		file.close();
	}

	return success;
}


/**
 * Remove the stored context from SPIFFS.
 * 
 * @returns True when removing was successful
 */
bool ArduinoMSGraph::removeContextFromSPIFFS() {
	#ifdef MSGRAPH_DEBUG
		DBG_PRINTLN(F("removeContextFromSPIFFS()"));
	#endif

	return SPIFFS.remove(CONTEXT_FILE);
}


/**
 * Get presence information of the current user
 * 
 * @returns Presence information
 */
GraphPresence ArduinoMSGraph::getUserPresence() {
	// See: https://github.com/microsoftgraph/microsoft-graph-docs/blob/ananya/api-reference/beta/resources/presence.md
	GraphError resultError;
	GraphPresence result;

	const size_t capacity = JSON_OBJECT_SIZE(4) + 512;
	DynamicJsonDocument responseDoc(capacity);

	bool res = requestJsonApi(responseDoc, "https://graph.microsoft.com/beta/me/presence", "", "GET", true);
	// serializeJsonPretty(responseDoc, Serial);

	if (!res) {
		resultError.hasError = true;
		resultError.message = strdup("Request error");
	} else if (responseDoc.containsKey("error")) {
		_handleApiError(responseDoc, resultError);
	} else {
		// Return presence info
		result.id = (char *)responseDoc["id"].as<char *>();
		result.availability = (char *)responseDoc["availability"].as<char *>();
		result.activity = (char *)responseDoc["activity"].as<char *>();
	}

	this->_lastError = resultError;
	return result;
}


/**
 * Get {count} next events in the users calendar.
 * 
 * @param count Number of events of request.
 * @param timezone Timezone in which the times should be returned. Default "Europe/Berlin"
 * 
 * @returns Vector of GraphEvent structures to hold the result.
 */
std::vector<GraphEvent> ArduinoMSGraph::getUserEvents(int count, const char *timezone) {
	// See: https://docs.microsoft.com/en-us/graph/api/user-list-events?view=graph-rest-1.0
	GraphError resultError;
	std::vector<GraphEvent> result;

	const size_t capacity = 10000;
	DynamicJsonDocument responseDoc(capacity);

	char url[97 + 3];
    sprintf(url, "https://graph.microsoft.com/v1.0/me/events?$select=subject,start,end,location,bodyPreview&$top=%d", count);

	char timezoneParam[129];
	sprintf(timezoneParam, "outlook.timezone=\"%s\"", timezone);
	GraphRequestHeader extraHeader = { "Prefer", timezoneParam };

	bool res = requestJsonApi(responseDoc, url, "", "GET", true, extraHeader);
	// serializeJsonPretty(responseDoc, Serial);

	if (!res) {
		resultError.hasError = true;
		resultError.message = strdup("Request error");
	} else if (responseDoc.containsKey("error")) {
		_handleApiError(responseDoc, resultError);
	} else {
		// Return event info
		if (responseDoc.containsKey("value")) {
			JsonArray items = responseDoc["value"];

			for (JsonObject item : items) {
				GraphEvent event;
				event.id = (char *)item["id"].as<char *>();
				event.subject = (char *)item["subject"].as<char *>();
				event.locationTitle = (char *)item["location"]["displayName"].as<char *>();
				event.bodyPreview = (char *)item["bodyPreview"].as<char *>();
				event.startDate.dateTime = (char *)item["start"]["dateTime"].as<char *>();
				event.startDate.timeZone = (char *)item["start"]["timeZone"].as<char *>();
				event.endDate.dateTime = (char *)item["end"]["dateTime"].as<char *>();
				event.endDate.timeZone = (char *)item["end"]["timeZone"].as<char *>();

				result.push_back(event);
			}
		}
	}

	this->_lastError = resultError;
	return result;
}


/**
 * Handle erros returned in errorDoc and set errorObject accordingly
 * 
 * @param errorDoc Response from the Graph API that contains a error
 * @param errorObject GraphError object to hold the error informnations
 */
void ArduinoMSGraph::_handleApiError(JsonDocument &errorDoc, GraphError &errorObject) {
	const char* _error_code = errorDoc["error"]["code"];
	if (strcmp(_error_code, "InvalidAuthenticationToken") == 0) {
		#ifdef MSGRAPH_DEBUG
			DBG_PRINTLN(F("_handleApiError() - Refresh needed"));
		#endif

		errorObject.tokenNeedsRefresh = true;
	} else {
		#ifdef MSGRAPH_DEBUG
			Serial.printf("_handleApiError() - Other error: %s\n", _error_code);
		#endif
	}

	errorObject.message = (char *)errorDoc["error"]["code"].as<char *>();
	errorObject.hasError = true;
}


/**
 * Return access token lifetime in seconds
 * 
 * @returns Token lifetime in seconds
 */
int ArduinoMSGraph::getTokenLifetime() {
	return (_context.expires - millis()) / 1000;
}


/**
 * Return the error object for the last request
 * 
 * @returns Error object of the last request
 */
GraphError ArduinoMSGraph::getLastError() {
	return this->_lastError;
}