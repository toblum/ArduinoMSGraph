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
				// this->client->stop();
				// delete this->client;
				// this->client = NULL;
				
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
 * @param doc JsonDocument passed as reference to hold the result.
 * @param scope The scope to request from Azure AD.
 * @returns void
 */
void ArduinoMSGraph::startDeviceLoginFlow(JsonDocument &doc, const char *scope) {
	DBG_PRINT(F("startDeviceLoginFlow() - "));
	DBG_PRINTLN(scope);

	char url[255];
    sprintf(url,"https://login.microsoftonline.com/%s/oauth2/v2.0/devicecode", this->_tenant);
	char payload[255];
    sprintf(payload,"client_id=%s&scope=scope", this->_clientId, scope);

	boolean res = requestJsonApi(doc, url, payload);
}

int ArduinoMSGraph::getTokenLifetime() {
	return (_tokenExpires - millis()) / 1000;
}