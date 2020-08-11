#include "ArduinoMSGraph.h"
#include "ArduinoMSGraphCerts.h"

ArduinoMSGraph::ArduinoMSGraph(Client &client, const char *tenant, const char *clientId) {
    this->client = &client;
    this->_tenant = tenant;
    this->_clientId = clientId;
}


/**
 * Perform a HTTP request.
 * 
 * @param responseDoc JsonDocument passed as reference to hold the result.
 * @param url URL to request
 * @param payload Raw payload to send together with the request.
 * @param method Method for the HTTP request: GET, POST, ...
 * @param sendAuth If true, send the Bearer token together with the request.
 * @returns True if request successful, false on error.
 */
bool ArduinoMSGraph::requestJsonApi(JsonDocument& responseDoc, const char *url, const char *payload, const char *method, bool sendAuth) {
	const char* cert;
	if (strstr(url, "graph.microsoft.com") != NULL) {
		cert = rootCACertificateGraph;
	} else {
		cert = rootCACertificateLogin;
	}

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
			Serial.printf("[HTTPS] Auth token valid for %d s.\n", getTokenLifetime());
		}

		// Start connection and send HTTP header
		int httpCode = https.sendRequest(method, payload);

		// httpCode will be negative on error
		if (httpCode > 0) {
			// HTTP header has been send and Server response header has been handled
			Serial.printf("[HTTPS] Method: %s, Response code: %d\n", method, httpCode);

			// File found at server (HTTP 200, 301), or HTTP 400, 401 with response payload
			if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_BAD_REQUEST || httpCode == HTTP_CODE_UNAUTHORIZED) {
				// String res = https.getString();
				// DBG_PRINTLN(res);

				// Parse JSON data
				DeserializationError error = deserializeJson(responseDoc, https.getStream());
				if (error) {
					DBG_PRINT(F("deserializeJson() failed: "));
					DBG_PRINTLN(error.c_str());
					https.end();
					return false;
				} else {
					https.end();
					return true;
				}
			} else {
				Serial.printf("[HTTPS] Other HTTP code: %d\nResponse: ", httpCode);
				DBG_PRINTLN(https.getString());
				https.end();
				return false;
			}
		} else {
			Serial.printf("[HTTPS] Request failed: %s\n", https.errorToString(httpCode).c_str());
			https.end();
			return false;
		}
    } else {
    	DBG_PRINTLN(F("[HTTPS] Unable to connect"));
		return false;
    }
}


/**
 * Start the device login flow and request login page data.
 * 
 * @param responseDoc JsonDocument passed as reference to hold the result.
 * Reserve a size of at least: JSON_OBJECT_SIZE(6) + 540
 * @param scope The scope to request from Azure AD.
 * @returns True if request successful, false on error
 */
bool ArduinoMSGraph::startDeviceLoginFlow(JsonDocument &responseDoc, const char *scope) {
	DBG_PRINT(F("startDeviceLoginFlow() - Scope: "));
	DBG_PRINTLN(scope);

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
 * @returns True if token is available, if false, continue polling.
 */
bool ArduinoMSGraph::pollForToken(JsonDocument &responseDoc, const char *device_code) {
	DBG_PRINTLN(F("pollForToken()"));

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
			strncpy(_context.access_token, responseDoc["access_token"], sizeof(_context.access_token));
			strncpy(_context.refresh_token, responseDoc["refresh_token"], sizeof(_context.refresh_token));
			strncpy(_context.id_token, responseDoc["id_token"], sizeof(_context.id_token));
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
	DBG_PRINTLN(F("refreshToken()"));
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
			strncpy(_context.access_token, responseDoc["access_token"], sizeof(_context.access_token));
			success = true;
		}
		if (!responseDoc["refresh_token"].isNull()) {
			strncpy(_context.refresh_token, responseDoc["refresh_token"], sizeof(_context.refresh_token));
			success = true;
		}
		if (!responseDoc["id_token"].isNull()) {
			strncpy(_context.id_token, responseDoc["id_token"], sizeof(_context.id_token));
		}
		if (!responseDoc["expires_in"].isNull()) {
			int _expires_in = responseDoc["expires_in"].as<unsigned long>();
			_context.expires = millis() + (_expires_in * 1000); // Calculate timestamp when token expires
		}

		DBG_PRINTLN(F("refreshToken() - Success"));
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
	DBG_PRINT(F("saveContextInSPIFFS() - Success - Bytes written: "));
	DBG_PRINTLN(bytesWritten);
	// DBG_PRINTLN(contextDoc.as<String>());

	return bytesWritten > 0;
}


/**
 * Try to restote Graph context from SPIFFS
 * 
 * @returns True if restore was successful.
 */
bool ArduinoMSGraph::readContextFromSPIFFS() {
	File file = SPIFFS.open(CONTEXT_FILE);
	boolean success = false;

	if (!file) {
		DBG_PRINTLN(F("loadContext() - No file found"));
	} else {
		size_t size = file.size();
		if (size == 0) {
			DBG_PRINTLN(F("loadContext() - File empty"));
		} else {
			const int capacity = JSON_OBJECT_SIZE(3) + 5000;
			DynamicJsonDocument contextDoc(capacity);
			DeserializationError err = deserializeJson(contextDoc, file);

			if (err) {
				DBG_PRINT(F("loadContext() - deserializeJson() failed with code: "));
				DBG_PRINTLN(err.c_str());
			} else {
				int numSettings = 0;
				if (!contextDoc["access_token"].isNull()) {
					strncpy(_context.access_token, contextDoc["access_token"], sizeof(_context.access_token));
					numSettings++;
				}
				if (!contextDoc["refresh_token"].isNull()) {
					strncpy(_context.refresh_token, contextDoc["refresh_token"], sizeof(_context.refresh_token));
					numSettings++;
				}
				if (!contextDoc["id_token"].isNull()){
					strncpy(_context.id_token, contextDoc["id_token"], sizeof(_context.id_token));
				}
				_context.expires = 0;
				if (numSettings >= 2) {
					DBG_PRINTLN(F("loadContext() - Success"));
					success = true;
				} else {
					Serial.printf("loadContext() - ERROR Number of valid settings in file: %d, should greater or equals 2.\n", numSettings);
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
	DBG_PRINTLN(F("removeContextFromSPIFFS()"));
	return SPIFFS.remove(CONTEXT_FILE);
}


/**
 * Get presence information of the current user
 * 
 * @returns Presence information
 */
GraphPresence ArduinoMSGraph::getUserPresence() {
	// See: https://github.com/microsoftgraph/microsoft-graph-docs/blob/ananya/api-reference/beta/resources/presence.md
	GraphPresence result;
	result.error.tokenNeedsRefresh = false;

	const size_t capacity = JSON_OBJECT_SIZE(4) + 512;
	DynamicJsonDocument responseDoc(capacity);

	bool res = requestJsonApi(responseDoc, "https://graph.microsoft.com/beta/me/presence", "", "GET", true);

	if (!res) {
		result.error.hasError = false;
	} else if (responseDoc.containsKey("error")) {
		const char* _error_code = responseDoc["error"]["code"];
		if (strcmp(_error_code, "InvalidAuthenticationToken") == 0) {
			DBG_PRINTLN(F("pollPresence() - Refresh needed"));

			result.error.tokenNeedsRefresh = true;
		} else {
			Serial.printf("pollPresence() - Error: %s\n", _error_code);
		}

		strncpy(result.error.message, _error_code, sizeof(result.error.message));
		result.error.hasError = true;
	} else {
		// Return presence info
		strncpy(result.id, responseDoc["id"], sizeof(result.id));
		strncpy(result.availability, responseDoc["availability"], sizeof(result.availability));
		strncpy(result.activity, responseDoc["activity"], sizeof(result.activity));
		result.error.hasError = false;
	}
	return result;
}


int ArduinoMSGraph::getTokenLifetime() {
	return (_context.expires - millis()) / 1000;
}