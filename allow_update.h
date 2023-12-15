// TODO: I should make this a class....

//flag to use from web update to reboot the ESP
bool shouldReboot = false;
// Check if user is logged in
bool allow_update = false;
unsigned long allowed_update_at = 0;

void onRequest(AsyncWebServerRequest *request) {
  //Handle Unknown Request
  request->send(404);
}


void block_until_update() {

  AsyncWebServer server(80);
  AsyncWebSocket ws("/ws");            // access at ws://[esp ip]/ws
  AsyncEventSource events("/events");  // event source (Server-Sent events)
  server.addHandler(&events);
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/html", "Version 1.0: OTA Mode");
  });
  // HTTP basic authentication
  server.on("/login", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (!request->authenticate(ota_user, ota_pass)) {
      return request->requestAuthentication();
    }
    request->send(200, "text/plain", "Login Success!");
    allow_update = true;
    allowed_update_at = millis();
  });
  // Simple Firmware Update Form
  server.on("/update", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (allow_update) {
      request->send(200, "text/html", "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>");
    } else {
      request->send(403, "text/plain", "forbidden");
    }
  });

  // From the WebServers examples list: https://github.com/me-no-dev/ESPAsyncWebServer.
  // Can export compiled binary for this from Sketch menu (Ctrl-Alt-S)..
  // Note: It's the bin file, not the elf file.
  server.on(
    "/update", HTTP_POST, [](AsyncWebServerRequest *request) {
      shouldReboot = !Update.hasError();
      String response_text = shouldReboot ? "OK" : "FAIL";
      if (!allow_update) {
        response_text = "Forbidden!";
      }
      AsyncWebServerResponse *response = request->beginResponse(200, "text/plain", response_text);
      response->addHeader("Connection", "close");
      request->send(response);
    },
    [](AsyncWebServerRequest *request, String filename, size_t index, uint8_t *data, size_t len, bool final) {
      // If updates are not allowed, return immedietely.
      if (!allow_update) {
        return;
      }
      if (!index) {
        Serial.printf("Update Start: %s\n", filename.c_str());
        Serial.printf("Update Start: %s\n", filename.c_str());
        if (!Update.begin()) {
          Update.printError(Serial);
        }
      }
      if (!Update.hasError()) {
        if (Update.write(data, len) != len) {
          Update.printError(Serial);
        }
      }
      if (final) {
        if (Update.end(true)) {
          Serial.printf("Update Success: %uB\n", index + len);
        } else {
          Update.printError(Serial);
        }
      }
    });
  // Catch-All Handlers
  // Any request that can not find a Handler that canHandle it
  // ends in the callbacks below.
  server.onNotFound(onRequest);
  server.begin();
  while (!shouldReboot) {
    // Disable update after a minute.
    if (millis() - allowed_update_at > (60 * 1000)) {
      allow_update = false;
    }
    digitalWrite(ledR, HIGH);
    delay(100);
    digitalWrite(ledR,LOW);
    delay(100);
  }
  // Once past this infinite loop, we can reboot an apply the update.
  // Though if the OTA pin is still set, we will simply return to OTA mode.
  ESP.restart();
}
