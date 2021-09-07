// Wireless SD Card.
// 2019-02-24  T. Nakagawa

#include <SD.h>
#include <WiFi.h>

const char ssid[] = "SDcard_saver";
const char pass[] = "sdcardsaver";
const IPAddress ip(192,168,30,3);
const IPAddress subnet(255,255,255,0);
constexpr int PIN_TRI = 16;  // H: Disconnect from the adaptor.
constexpr int PIN_FET = 17;  // H: SDC power-on.
constexpr int PIN_SCK = 18;
constexpr int PIN_MISO = 19;
constexpr int PIN_SS = 4;
constexpr int PIN_LED = 22;
constexpr int PIN_MOSI = 23;

WiFiServer server(80);

bool sdc_initialized = false;

void sdc_reset() {
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_TRI, HIGH);

  if (sdc_initialized) {
    SD.end();
  }
  pinMode(PIN_SCK, INPUT);
  pinMode(PIN_SS, INPUT);
  pinMode(PIN_MOSI, INPUT);

  digitalWrite(PIN_FET, LOW);
  delay(500);
  digitalWrite(PIN_FET, HIGH);
  delay(500);
  sdc_initialized = false;

  digitalWrite(PIN_TRI, LOW);
  digitalWrite(PIN_LED, HIGH);
}

void sdc_begin() {
  digitalWrite(PIN_LED, LOW);
  digitalWrite(PIN_TRI, HIGH);

  if (!sdc_initialized) {
    sdc_initialized = true;
    if (SD.begin(PIN_SS, SPI, 10000000)) {
      Serial.println("SD initialization succeeded.");
    } else {
      Serial.println("SD initialization failed.");
    }
  } else {
    SPI.begin();
  }
  pinMode(PIN_SCK, OUTPUT);
  pinMode(PIN_SS, OUTPUT);
  pinMode(PIN_MOSI, OUTPUT);
}

void sdc_end() {
  SPI.end();
  pinMode(PIN_SCK, INPUT);
  pinMode(PIN_SS, INPUT);
  pinMode(PIN_MOSI, INPUT);

  digitalWrite(PIN_TRI, LOW);
  digitalWrite(PIN_LED, HIGH);
}

void setup() {
  pinMode(PIN_SCK, INPUT);
  pinMode(PIN_MISO, INPUT);
  pinMode(PIN_SS, INPUT);
  pinMode(PIN_MOSI, INPUT);
  pinMode(PIN_TRI, OUTPUT);
  digitalWrite(PIN_TRI, LOW);
  pinMode(PIN_FET, OUTPUT);
  digitalWrite(PIN_FET, HIGH);
  pinMode(PIN_LED, OUTPUT);
  digitalWrite(PIN_LED, HIGH);

  Serial.begin(115200);
  while (!Serial) ;

  WiFi.softAP(ssid,pass);
  delay(100);
  WiFi.softAPConfig(ip,ip,subnet);
  IPAddress myIP = WiFi.softAPIP();
  server.begin();
}

void processIndex(WiFiClient *client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-type: text/html");
  client->println();
  client->println("<!DOCTYPE html>");
  client->println("<head><title>Index</title></head>");
  client->println("<body>");
  client->println("<h1>wireless SD card saver</h1>");
  client->println("<form action=\"upload.cgi\" method=\"post\" enctype=\"multipart/form-data\"><input type=\"submit\" value=\"Upload\"> <input type=\"file\" name=\"file\"></form>");
  //client->println("<form action=\"list.cgi\"><input type=\"submit\" value=\"List\"></form>");
  //client->println("<form action=\"remove.cgi\"><input type=\"submit\" value=\"Remove\"></form>");
  //client->println("<form action=\"reset.cgi\"><input type=\"submit\" value=\"Reset\"></form>");
  client->println("</body>");
  client->println("</html>");
}

void processList(WiFiClient *client) {
  client->println("HTTP/1.1 200 OK");
  client->println("Content-type: text/html");
  client->println();
  client->println("<!DOCTYPE html>");
  client->println("<head><title>List</title></head>");
  client->println("<body>");
  sdc_begin();
  File root = SD.open("/");
  if (root) {
    client->println("<pre>");
    for (File file; file = root.openNextFile(); ) {
      if (!file.isDirectory()) {
        client->print(file.size());
        client->print("\t");
        client->println(file.name());
      }
      file.close();
    }
    root.close();
    const unsigned long used_size = SD.usedBytes();
    client->print(used_size);
    client->println("\tUSED");
    const unsigned long free_size = SD.totalBytes() - SD.usedBytes();
    client->print(free_size);
    client->println("\tFREE");
    client->println("</pre>");
  } else {
    Serial.println("Directory open failed.");
    client->println("<p>Directory open failed.</p>");
  }
  sdc_end();
  client->println("</body>");
  client->println("</html>");
}

void processRemove(WiFiClient *client) {
  bool succeeded = true;
  sdc_begin();
  File root = SD.open("/");
  for (File file; file = root.openNextFile(); ) {
    if (!file.isDirectory()) {
      if (!SD.remove(file.name())) {
        Serial.print("File removal failed: ");
        Serial.println(file.name());
        succeeded = false;
        file.close();
        break;
      }
    }
    file.close();
  }
  root.close();
  sdc_end();

  client->println("HTTP/1.1 200 OK");
  client->println("Content-type: text/html");
  client->println();
  client->println("<!DOCTYPE html>");
  client->println("<head><title>Remove</title></head>");
  client->println("<body>");
  if (succeeded) {
    client->println("<p>File removal succeeded.</p>");
  } else {
    client->println("<p>File removal failed.</p>");
  }
  client->println("</body>");
  client->println("</html>");
}

void processReset(WiFiClient *client) {
  sdc_reset();

  client->println("HTTP/1.1 200 OK");
  client->println("Content-type: text/html");
  client->println();
  client->println("<!DOCTYPE html>");
  client->println("<head><title>Reset</title></head>");
  client->println("<body>");
  client->println("<p>Card reset succeeded.</p>");
  client->println("</body>");
  client->println("</html>");
}

void processUpload(WiFiClient *client) {
  String boundary;
  String filename;
  while (client->connected() && client->available()) {
    String line = client->readStringUntil('\n');
    line.trim();
    if (line.startsWith("--")) {
      boundary = line;
      Serial.print("Boundary: "); Serial.println(boundary);
    } else if (boundary.length() && line.indexOf("name=\"file\"") >= 0) {
      const int bgn = line.indexOf("filename=\"");
      if (bgn >= 0) {
        const int end = line.indexOf("\"", bgn + 10);
        if (end >= 0) {
          filename = line.substring(bgn + 10, end);
        }
      }
      Serial.print("Filename: "); Serial.println(filename);
    } else if (filename.length() && line == "") {
      break;
    }
  }
  bool succeeded = false;
  if (filename.length()) {
    filename = "/" + filename;
    sdc_begin();
    if (SD.exists(filename)) {
      SD.remove(filename);
    }
    File file = SD.open(filename, FILE_WRITE);
    if (file) {
      Serial.print("Writing... ");
      boundary = "\r\n" + boundary + "--\r\n";
      static unsigned char *buf = new unsigned char[16 * 1024 + 512 + 1];
      size_t ofst = 0;
      while (client->connected() && client->available()) {
        const size_t avil = 16 * 1024 + 512 - ofst;
        size_t size = client->readBytes(buf + ofst, avil);
        ofst += size;
        if (ofst >= 16 * 1024 + boundary.length()) {
          file.write(buf, 16 * 1024);
          ofst -= 16 * 1024;
          memcpy(buf, buf + 16 * 1024, ofst);
        }
      }
      buf[ofst] = '\0';
      if (String((char *)(buf + ofst - boundary.length())).equals(boundary)) {
        file.write(buf, ofst - boundary.length());
        succeeded = true;
      }
      Serial.println("done");
      file.close();
    }
    sdc_end();
  }

  client->println("HTTP/1.1 200 OK");
  client->println("Content-type: text/html");
  client->println();
  client->println("<!DOCTYPE html>");
  client->println("<head><title>Upload</title></head>");
  client->println("<body>");
  if (succeeded) {
    client->println("<p>File upload succeeded.</p>");
  } else {
    client->println("<p>File upload failed.</p>");
  }
  client->println("</body>");
  client->println("</html>");
}

void loop() {
  WiFiClient client = server.available();
  if (client) {
    Serial.print("Accessed: ");
    const String line = client.readStringUntil('\n');
    Serial.println(line);
    if (line.startsWith("GET / ")) {
      processIndex(&client);
    } else if (line.startsWith("GET /list.cgi ")) {
      processList(&client);
    } else if (line.startsWith("GET /remove.cgi ")) {
      processRemove(&client);
    } else if (line.startsWith("GET /reset.cgi ")) {
      processReset(&client);
    } else if (line.startsWith("POST /upload.cgi ")) {
      processUpload(&client);
    }
    client.stop();
  }
}
