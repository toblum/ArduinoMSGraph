#include "ArduinoMSGraph.h"
#include "ArduinoMSGraphCerts.h"

ArduinoMSGraph::ArduinoMSGraph(Client &client, const char *tenant, const char *clientId)
{
    this->client = &client;
    this->_tenant = tenant;
    this->_clientId = clientId;
}

boolean ArduinoMSGraph::requestJsonApi(JsonDocument& doc, String url, String payload, String type, boolean sendAuth) {
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
			https.addHeader("Authorization", _bearerToken);
			Serial.printf("[HTTPS] Auth token valid for %d s.\n", getTokenLifetime());
		}

		// Start connection and send HTTP header
		int httpCode = 0;
		if (type == "POST") {
			httpCode = https.POST(payload);
		} else {
			httpCode = https.GET();
		}

		// httpCode will be negative on error
		if (httpCode > 0) {
			// HTTP header has been send and Server response header has been handled
			Serial.printf("[HTTPS] Method: %s, Response code: %d\n", type.c_str(), httpCode);

			// File found at server (HTTP 200, 301), or HTTP 400 with response payload
			if (httpCode == HTTP_CODE_OK || httpCode == HTTP_CODE_MOVED_PERMANENTLY || httpCode == HTTP_CODE_BAD_REQUEST) {

                // String payload = https.getString();
                // DBG_PRINTLN(payload);

				// Parse JSON data
				DeserializationError error = deserializeJson(doc, https.getStream());
				
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
			return true;
		} else {
			DBG_PRINTLN("pollForToken() - Not all expected keys (access_token, refresh_token) found.");
			return false;
		}
	}
}


int ArduinoMSGraph::getTokenLifetime() {
	return (_tokenExpires - millis()) / 1000;
}