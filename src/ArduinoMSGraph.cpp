#include "ArduinoMSGraph.h"
#include "ArduinoMSGraphCerts.h"

ArduinoMSGraph::ArduinoMSGraph(Client &client, const char *tenant, const char *clientId) {
    this->client = &client;
    this->_tenant = tenant;
    this->_clientId = clientId;
}


/**
 * Must be called on a regular basis. Handles refreshing of expiered tokens.
 */
void ArduinoMSGraph::loop() {
	if (_context.expires <= millis()) {
		DBG_PRINT(F("loop() - Token needs refresh"));
	}
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
bool ArduinoMSGraph::requestJsonApi(JsonDocument& responseDoc, String url, String payload, String method, bool sendAuth) {
    const char* cert;
    if (url.indexOf("graph.microsoft.com") > -1) {
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
			char authHeader[512];
			sprintf(authHeader, "Bearer %s", _context.access_token.c_str());
			DBG_PRINTLN("###################");
			DBG_PRINTLN(authHeader);
			DBG_PRINTLN("###################");
			https.addHeader("Authorization", authHeader);
			Serial.printf("[HTTPS] Auth token valid for %d s.\n", getTokenLifetime());
		}

		// Start connection and send HTTP header
		int httpCode = 0;
		if (method == "POST") {
			httpCode = https.POST(payload);
		} else {
			httpCode = https.GET();
		}

		// httpCode will be negative on error
		if (httpCode > 0) {
			// HTTP header has been send and Server response header has been handled
			Serial.printf("[HTTPS] Method: %s, Response code: %d\n", method.c_str(), httpCode);

			// File found at server (HTTP 200, 301), or HTTP 400 with response payload
			if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_BAD_REQUEST) {

				// String payload = https.getString();
				// DBG_PRINTLN(payload);

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
	DBG_PRINT(F("startDeviceLoginFlow() - "));
	DBG_PRINTLN(scope);

	char url[255];
    sprintf(url,"https://login.microsoftonline.com/%s/oauth2/v2.0/devicecode", this->_tenant);
	char payload[255];
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

	char url[255];
    sprintf(url,"https://login.microsoftonline.com/%s/oauth2/v2.0/token", this->_tenant);
	char payload[512];
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
			_context.access_token = responseDoc["access_token"].as<String>();
			_context.refresh_token = responseDoc["refresh_token"].as<String>();
			_context.id_token = responseDoc["id_token"].as<String>();
			unsigned int _expires_in = responseDoc["expires_in"].as<unsigned int>();
			_context.expires = millis() + (_expires_in * 1000); // Calculate timestamp when token expires

			return true;
		} else {
			DBG_PRINTLN("pollForToken() - Not all expected keys (access_token, refresh_token) found.");
			return false;
		}
	}
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
	contextDoc["expires"] = _context.expires;

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
					_context.access_token = contextDoc["access_token"].as<String>();
					numSettings++;
				}
				if (!contextDoc["refresh_token"].isNull()) {
					_context.refresh_token = contextDoc["refresh_token"].as<String>();
					numSettings++;
				}
				if (!contextDoc["id_token"].isNull()){
					_context.id_token = contextDoc["id_token"].as<String>();
				}
				if (!contextDoc["expires"].isNull()){
					_context.expires = contextDoc["expires"].as<unsigned int>();
				}
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

	const size_t capacity = JSON_OBJECT_SIZE(4) + 220;
	DynamicJsonDocument responseDoc(capacity);

	bool res = requestJsonApi(responseDoc, "https://graph.microsoft.com/beta/me/presence", "", "GET", true);

	if (!res) {
		result.error.hasError = false;
	} else if (responseDoc.containsKey("error")) {
		const char* _error_code = responseDoc["error"]["code"];
		if (strcmp(_error_code, "InvalidAuthenticationToken")) {
			DBG_PRINTLN(F("pollPresence() - Refresh needed"));

			_context.expires = 0; // Force token refresh
		} else {
			Serial.printf("pollPresence() - Error: %s\n", _error_code);
		}
		result.error.hasError = true;
	} else {
		// Return presence info
		
		result.id = responseDoc["id"].as<String>();
		result.availability = responseDoc["availability"].as<String>();
		result.activity = responseDoc["activity"].as<String>();
		result.error.hasError = false;
	}
	return result;
}


int ArduinoMSGraph::getTokenLifetime() {
	return (_context.expires - millis()) / 1000;
}