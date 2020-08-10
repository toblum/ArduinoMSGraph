/*
	Copyright (c) 2020 Tobias Blum. All right reserved.
*/

#ifndef ArduinoMSGraph_h
#define ArduinoMSGraph_h

#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)

#define CONTEXT_FILE "/graph_context.json"			// Filename of the context file

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>
#include "SPIFFS.h"

struct GraphError {
	bool hasError;
	String message;
};

struct GraphAuthContext {
	String access_token;
	String refresh_token;
	String id_token;
	unsigned int expires;
};

struct GraphPresence {
	String id;
	String availability;
	String activity;

	GraphError error;
};


class ArduinoMSGraph {
public:
	// Constructor
	ArduinoMSGraph(Client &client, const char *tenant, const char *clientId);

	// General
	void loop();

	// // Auth Methods
	// void setRefreshToken(const char *refreshToken);
	// bool refreshAccessToken();
	// bool checkAndRefreshAccessToken();
	// const char *requestAccessTokens(const char *code, const char *redirectUrl);

	// // Generic Request Methods
	bool requestJsonApi(JsonDocument &doc, const char *url, const char *payload = "", const char *method = "POST", bool sendAuth = false);
	// int makeGetRequest(const char *command, const char *authorization, const char *accept = "application/json", const char *host = SPOTIFY_HOST);
	// int makeRequestWithBody(const char *type, const char *command, const char *authorization, const char *body = "", const char *contentType = "application/json", const char *host = SPOTIFY_HOST);
	// int makePostRequest(const char *command, const char *authorization, const char *body = "", const char *contentType = "application/json", const char *host = SPOTIFY_HOST);
	// int makePutRequest(const char *command, const char *authorization, const char *body = "", const char *contentType = "application/json", const char *host = SPOTIFY_HOST);

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

	// // User methods
	// CurrentlyPlaying getCurrentlyPlaying(const char *market = "");
	// PlayerDetails getPlayerDetails(const char *market = "");
	// bool play(const char *deviceId = "");
	// bool playAdvanced(char *body, const char *deviceId = "");
	// bool pause(const char *deviceId = "");
	// bool setVolume(int volume, const char *deviceId = "");
	// bool toggleShuffle(bool shuffle, const char *deviceId = "");
	// bool setRepeatMode(RepeatOptions repeat, const char *deviceId = "");
	// bool nextTrack(const char *deviceId = "");
	// bool previousTrack(const char *deviceId = "");
	// bool playerControl(char *command, const char *deviceId = "", const char *body = "");
	// bool playerNavigate(char *command, const char *deviceId = "");
	// bool seek(int position, const char *deviceId = "");

	// // Image methods
	// bool getImage(char *imageUrl, Stream *file);

	// int portNumber = 443;
	// int tagArraySize = 10;
	// int currentlyPlayingBufferSize = 10000;
	// int playerDetailsBufferSize = 10000;
	// bool autoTokenRefresh = true;
	Client *client;

private:
	const char *_clientId;
	const char *_tenant;

	GraphAuthContext _context;

	// const char *_clientSecret;
	// unsigned int timeTokenRefreshed;
	// unsigned int tokenTimeToLiveMs;
	// int getContentLength();
	// int getHttpStatusCode();
	// void skipHeaders(bool tossUnexpectedForJSON = true);
	// void closeClient();
	// void parseError();
	// const char *requestAccessTokensBody =
	//     R"(grant_type=authorization_code&code=%s&redirect_uri=%s&client_id=%s&client_secret=%s)";
	// const char *refreshAccessTokensBody =
	//     R"(grant_type=refresh_token&refresh_token=%s&client_id=%s&client_secret=%s)";
};

#endif