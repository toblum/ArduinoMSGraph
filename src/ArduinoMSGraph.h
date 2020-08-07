/*
    Copyright (c) 2020 Tobias Blum. All right reserved.
*/

#define DBG_PRINT(x) Serial.print(x)
#define DBG_PRINTLN(x) Serial.println(x)

#ifndef ArduinoMSGraph_h
#define ArduinoMSGraph_h

#include <Arduino.h>
#include <ArduinoJson.h>
#include <HTTPClient.h>


class ArduinoMSGraph
{
public:
    ArduinoMSGraph(Client &client, const char *tenant, const char *clientId);
    // ArduinoSpotify(Client &client, const char *clientId, const char *clientSecret, const char *refreshToken = "");

    // // Auth Methods
    // void setRefreshToken(const char *refreshToken);
    // bool refreshAccessToken();
    // bool checkAndRefreshAccessToken();
    // const char *requestAccessTokens(const char *code, const char *redirectUrl);

    // // Generic Request Methods
    boolean requestJsonApi(JsonDocument &doc, String url, String payload = "", String type = "POST", boolean sendAuth = false);
    // int makeGetRequest(const char *command, const char *authorization, const char *accept = "application/json", const char *host = SPOTIFY_HOST);
    // int makeRequestWithBody(const char *type, const char *command, const char *authorization, const char *body = "", const char *contentType = "application/json", const char *host = SPOTIFY_HOST);
    // int makePostRequest(const char *command, const char *authorization, const char *body = "", const char *contentType = "application/json", const char *host = SPOTIFY_HOST);
    // int makePutRequest(const char *command, const char *authorization, const char *body = "", const char *contentType = "application/json", const char *host = SPOTIFY_HOST);

    // Helper
    int getTokenLifetime();

    // Authentication Methods
    void startDeviceLoginFlow(JsonDocument &doc, const char *scope = "offline_access%20openid%20Presence.Read");

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
    // char _bearerToken[200];
    // const char *_refreshToken;
    const char *_clientId;
    const char *_tenant;
    const char *_bearerToken;

    unsigned int _tokenExpires = 0;
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