#include <HTTPClient.h>

void open_door() {
  // From https://randomnerdtutorials.com/esp32-http-get-post-arduino/#http-get-1
  HTTPClient http;

  http.begin("http://open_sesame.busin/open_door_ajax ");

  // Send HTTP GET request
  int httpResponseCode = http.GET();

  if (httpResponseCode > 0) {
    Serial.printf("HTTP Response code: %d \n", httpResponseCode);
    String payload = http.getString();
    Serial.println(payload);
  } else {
    Serial.print("Error code: ");
    Serial.println(httpResponseCode);
  }
  // Free resources
  http.end();
}