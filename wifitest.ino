/*
  See readme for details.
  Author: Philip Matuskiewicz - philip.matuskiewicz@nyct.com

  #using NodeMCU Amica with ESP8266 chipset

  Modified 3/30/16
*/

#include <ESP8266WiFi.h>

#define ID              39 // hard coded id
#define WLAN_SSID       ""
#define WLAN_PASS       ""
#define HOST            ""
#define IP              "5.5.5.5"// ip of remote.server.com
#define WEBPAGE         "/wifi"
#define DOWNLOAD_SIZE   262144 // 256kb in characters
long TIMEOUT = 10L;
#define LOG_INTERVAL_MIN    10
int LOG_INTERVAL_MAX =   120;

IPAddress LOCAL_PING_TARGET(192,168,0,1);//local router ip
IPAddress REMOTE_PING_TARGET(5,5,5,5);//ip of remote.server.com

int testid = 0;
int testplan = 0; //0=undefined, 1=file download, 2=streaming mode
int minrate = 0; //minimum rate in kbps
int maxrate = 0; //maximum rate in kbps (0 for no max)

long rates[] = {64L, 128L, 256L, 512L, 768L, 1024L, 1536L, 2048L, 3072L};

// PING FUNCTIONALITY
#include "os_type.h"
extern "C" {
#include "ping.h"
}

ping_option pingOps; // Struct used by ping_start, see ping.h
IPAddress PING_TARGET;
int P_TIMES = 10;//how many pings to do
//generic holder for main function callback
int P_LOSS = -1;
int P_RCV = 0;
long P_AVG = 0L;
//local holder for last local data
int PL_LOSS = -1;
int PL_RCV = 0;
long PL_AVG = 0L;
//remote holder for last remote data
int PR_LOSS = -1;
int PR_RCV = 0;
long PR_AVG = 0L;

// This function is called when a ping is received or the request times out:
static void ICACHE_FLASH_ATTR ping_recv (void* arg, void *pdata)
{
  struct ping_resp *pingrsp = (struct ping_resp *)pdata;

  if (pingrsp->bytes > 0L)
  {
    /*
    Serial.print("PING reply from: ");
    Serial.print(PING_TARGET);
    Serial.print(": ");
    Serial.print("bytes=");
    Serial.print(pingrsp->bytes);
    Serial.print(" time=");
    Serial.print(pingrsp->resp_time);
    */
    P_AVG = P_AVG + pingrsp->resp_time;//add to the average counter
    //Serial.println("ms");
  }
  /*
  else
  {
    Serial.println("Request timed out");
  }
  */
}

// This function is called after the ping request is completed
static void ICACHE_FLASH_ATTR ping_sent (void* arg, void *pdata)
{
  struct ping_resp *pingrsp = (struct ping_resp *)pdata;
  /*
  Serial.print("Ping statistics for: ");
  Serial.println(PING_TARGET);
  Serial.print("Packets: Sent = ");
  Serial.print(pingrsp->total_count);
  Serial.print(", Recieved = ");
  */
  int rcv = pingrsp->total_count-pingrsp->timeout_count;
  /*
  Serial.print(rcv);
  Serial.print(", Lost = ");
  */
  int los = pingrsp->timeout_count;
  /*
  Serial.print(los);
  Serial.print(" (");
  Serial.print(float(pingrsp->timeout_count)/pingrsp->total_count*100);
  Serial.println("% loss)");
  */

  //do calculations to release lock on ping
  P_AVG = P_AVG / rcv;
  P_LOSS = los;
  P_RCV = rcv;
}

boolean ping_send()
{
  P_AVG = 0L;// reset ping
  P_LOSS = -1;// reset loss
  P_RCV = 0;
  
  pingOps.count = P_TIMES;
  pingOps.ip = uint32_t(PING_TARGET);
  pingOps.coarse_time = 3; // how long before the ping times out (seconds)
  ping_regist_sent(&pingOps, ping_sent); 
  ping_regist_recv(&pingOps, ping_recv);

  Serial.print("Pinging: ");
  Serial.print(PING_TARGET);
  Serial.println(" with 32 bytes of data");
  ping_start(&pingOps); // send the ping
  delay(500L);

  int ct = 0; // catch all way to make sure this never hangs but waits for the ping to complete before moving onto the rest of the app.
  while( ((P_LOSS + P_RCV) < P_TIMES) && ct < 56L){//14 second max lag (56 tries at .25 seconds each = 14 seconds), worst case, ping times out after 12 seconds, add latency
    delay(250L);//make programming thread wait for pinger to end  
    ct = ct + 1L;
  }

  if(ct > 55L){
    return false;  
  }

  return true;
  
}

void ping_print(int loss, int rcv, long avg)
{  
        Serial.print("Packets lost: ");
        Serial.print(loss);
        Serial.print(", Packets received: ");
        Serial.print(rcv);
        Serial.print(", Average Latency (time): ");
        Serial.print(avg);
        Serial.println("ms");
}

// END PING FUNCTIONALITY
//repurpose wait for stream to show total actual bytes received
boolean post(int32_t rssi, IPAddress local_ip, boolean local_ping_success, boolean remote_ping_success, long elapse, long actualrate, long wait) {

  String url = WEBPAGE;
  url += "?testid=";
  url += testid;
  url += "&concurrent=1";
  url += "&testtype=";
  url += testplan;
  url += "&minrate=";
  url += minrate;
  url += "&maxrate=";
  url += maxrate;
  if(testplan == 1){
    url += "&file_size_used=";
  }else{
    url += "&max_bytes_per_second=";
  }
  url += actualrate;
  url += "&id=";
  url += ID;
  url += "&rssi=";
  url += rssi;
  url += "&localip=";
  url += local_ip;
  url += "&localtime=";
  url += millis();
  if(testplan == 1){
    url += "&random_pause_between_attempts=";
  }else{
    url += "&total_bytes_received_during_stream=";
  }
  url += wait;
  if(testplan == 1){
    url += "&time_to_download_ms=";
  }else{
    url += "&time_duration_streamed=";
  }
  url += elapse;
  if(local_ping_success){
    url += "&local_ping_lost_packet_count=";
    url += PL_LOSS;
    url += "&local_ping_received_packet_count=";
    url += PL_RCV;
    url += "&local_ping_avg_latency_ms=";
    url += PL_AVG;
  }
  if(remote_ping_success){
    url += "&remote_ping_lost_packet_count=";
    url += PR_LOSS;
    url += "&remote_ping_received_packet_count=";
    url += PR_RCV;
    url += "&remote_ping_avg_latency_ms=";
    url += PR_AVG;
  }

  WiFiClient client;
  client.setTimeout(TIMEOUT * 1000L);
  if (!client.connect(IP, 80)) {
    Serial.println("HTTP connection failed");
    return false;
  }
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + HOST + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(10L);

  Serial.println(url);
  delay(1000L);

  while (client.connected()) {
    String line = client.readStringUntil('\r');
    Serial.println(line);
    if (line.startsWith("HTTP")) {
      Serial.println(line);
      Serial.println("ending client.");//just check to make sure we got a code back!
      client.stop();
      return true;
    }
  }

  Serial.println("Client failed to retrieve data, closing.");
  return false;
}

long fileDownloadTest(long downloadsizekbps)
{  
  long downloadsize = downloadsizekbps * 1000L;//1KB is 1000 bytes
  
  String url = WEBPAGE;
  url += "?file=";
  url += downloadsize;

  Serial.print("Timing download for ");
  Serial.print(downloadsize);
  Serial.println(" characters (bytes).");

  WiFiClient client;
  client.setTimeout(1L);
  if (!client.connect(IP, 80)) {
    Serial.println("HTTP connection failed");
    return false;
  }
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + HOST + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(500L);

  long rd = 0L;
  long start = 0L;
  int incommingbyte = 0;
  while (client.connected() && rd < downloadsize) {//timeout will kill client if this hangs
    if(start > 0L){
        incommingbyte = client.read();
        rd++;
    }else{
      //otherwise read headers
      String line = client.readStringUntil('\n');
      if(line.equals("\r")){
        start = millis();
      }
    }
  }
  long fin = millis();
  client.stop();
  long elapse = fin - start;

  Serial.print("READ ");
  Serial.print(rd);
  Serial.print(" characters (bytes) in ");
  Serial.print(elapse);
  Serial.println("ms");

  return elapse;
}

long streamBufferTest(int32_t rssi, IPAddress local_ip, boolean local_ping_success, boolean remote_ping_success, long downloadratekbps, long timeToStreamForS)
{  
  String url = WEBPAGE;
  url += "/testfile";//really large file

  Serial.print("Timing rate for ");
  Serial.print(downloadratekbps);
  Serial.print(" rate (kbps) for ");
  Serial.print(timeToStreamForS);
  Serial.println(" seconds.");

  long timeToStreamFor = timeToStreamForS * 1000L;//seconds to milliseconds
  long maxBytesPerSecond = downloadratekbps * 125L;//roughly 1kbps is 125 bytes per second

  WiFiClient client;
  client.setTimeout(100L);
  if (!client.connect(IP, 80)) {
    Serial.println("HTTP connection failed");
    return false;
  }
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + HOST + "\r\n" +
               "Connection: close\r\n\r\n");
  delay(500L);

  long bytesRecv = 0L;
  long tempBytesRecv = 0L;
  long duration = 0L;
  long bytesExhaustedResetTime = 0L;
  long stopTime = 0L;
  
  while (client.connected()) {//timeout will kill client if this hangs
    if(stopTime > 0L){
        client.read();
        tempBytesRecv++;
        
        //HOLD when we have received the maximum amount of data in a second
        while(tempBytesRecv > maxBytesPerSecond){
          //wait for the time to expire to release the wait loop
          if(millis() > bytesExhaustedResetTime){
              break;//goes to next routine that will reset this value and the next exhaustion time
          }
          delay(5L);
        }

        //reset receiving stats every second
        if(millis() > bytesExhaustedResetTime){
          bytesRecv = bytesRecv+tempBytesRecv;
          tempBytesRecv = 0L;
          bytesExhaustedResetTime = millis() + 1000L;//bytes/second reset for next second
        }

        //When we've hit the total time, stop and calclulate stats
        if(millis() > stopTime){
          long endTime = millis();
          long startTime = stopTime - timeToStreamFor;

          duration = endTime - startTime; //in milliseconds
          client.stop();//break out of the while loop
        }
        
    }else{
      //otherwise read headers
      String line = client.readStringUntil('\n');
      if(line.equals("\r")){
        stopTime = millis() + timeToStreamFor;
        bytesExhaustedResetTime = millis() + 1000L;//bytes/second
      }
    }
  }

  long bytesExpected = (duration/1000L) * (maxBytesPerSecond);
  //we need to post the durationSec, bytesExpected, bytesReceived to determine how long we could sustain for at full capacity / degraded capacity
  
  Serial.print("streamed ");
  Serial.print(bytesRecv);
  Serial.print(" bytes in ");
  Serial.print(duration);
  Serial.print(" seconds... expected at least ");
  Serial.print(bytesExpected);
  Serial.print(" bytes in this timeframe.  Maximum bytes per second set to ");
  Serial.println(maxBytesPerSecond);
  
  post(rssi, local_ip, local_ping_success, remote_ping_success, duration, maxBytesPerSecond, bytesRecv);

}

boolean TestSetup()
{  
  String url = WEBPAGE;
  url += "?testplan";

  WiFiClient client;
  client.setTimeout(TIMEOUT * 1000L);
  if (!client.connect(IP, 80)) {
    Serial.println("HTTP connection failed");
    return false;
  }
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: " + HOST + "\r\n" +
               "Connection: close\r\n\r\n");

  delay(1000L);

  int start = 0;
  String conf;
  while (client.connected()) {
    if(start > 0){
        conf = client.readStringUntil('\n');
        client.stop();
    }else{
      //otherwise read headers
      String line = client.readStringUntil('\n');
      if(line.equals("\r")){
        start = 1;
      }
    }
  }

  Serial.println(conf);

  int pos = conf.indexOf(';');
  int lpos = conf.lastIndexOf(';');
  String TestId = conf.substring(0, pos);
  int npos = conf.indexOf(';', pos+1);
  
  String TestPlan = conf.substring(pos+1, npos);
  int nnpos = conf.indexOf(';', npos+1);
  String MaxWaitBetweenTests = conf.substring(npos+1, nnpos);
  
  String MinBitRate;
  String MaxBitRate = "";
  if(lpos!=nnpos){
    //there is a maximum bitrate specified
    MinBitRate = conf.substring(nnpos+1, lpos);
    MaxBitRate = conf.substring(lpos+1, conf.length());
    maxrate = MaxBitRate.toInt();
  }else{
    MinBitRate = conf.substring(nnpos+1, conf.length());
  }
  testid = TestId.toInt();
  testplan = TestPlan.toInt();
  LOG_INTERVAL_MAX = MaxWaitBetweenTests.toInt();
  minrate = MinBitRate.toInt();
  
  Serial.println(testid);
  Serial.println(testplan);
  Serial.println(LOG_INTERVAL_MAX);
  Serial.println(minrate);
  Serial.println(maxrate);

  if(minrate < 1){
    Serial.println("BAD SETUP DATA, STOPPED.");
    return false;
  }
  return true;
}

void setup(void)
{
  Serial.begin(115200);

  pinMode (A0, INPUT);
  randomSeed(analogRead(A0));

  pinMode(D0, OUTPUT);
  digitalWrite(D0, LOW);
  delay(1500);
  digitalWrite(D0, HIGH);//turn light off

  Serial.println("Connecting...");
  WiFi.begin(WLAN_SSID, WLAN_PASS);
  int i = 0;
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000L);
    Serial.print(".");
    i++;
    if (i > TIMEOUT) {
      WiFi.disconnect();
      Serial.println("TIMED OUT CONNECTING TO WIFI!  Waiting until the next scheduled logging event for max interval.");
      delay(LOG_INTERVAL_MAX * 1000L);
      return;
    }
  }
  Serial.println("WiFi Connected!");

  while(!TestSetup()){
    delay(10000L);  
  }
  
}

void loop() {
  digitalWrite(D0, LOW);//turn light on
  long r = random(LOG_INTERVAL_MIN,LOG_INTERVAL_MAX);

  PING_TARGET = LOCAL_PING_TARGET;
  boolean local_ping_success = ping_send();
  if(local_ping_success){
    PL_LOSS = P_LOSS;
    PL_RCV = P_RCV;
    PL_AVG = P_AVG;
  }

  PING_TARGET = REMOTE_PING_TARGET;
  boolean remote_ping_success = ping_send();
  if(remote_ping_success){
    PR_LOSS = P_LOSS;
    PR_RCV = P_RCV;
    PR_AVG = P_AVG;
  }
  
  int32_t rssi = WiFi.RSSI();
  IPAddress local_ip = WiFi.localIP();

  long actual = rates[random(0, (sizeof(rates)/sizeof(int))-1)];
  if(maxrate == 0){
    while(actual < minrate){
       actual = rates[random(0, (sizeof(rates)/sizeof(int))-1)];
    }
  }else{
    while(actual < minrate || actual > maxrate){
       actual = rates[random(0, (sizeof(rates)/sizeof(int))-1)];
    }
  }

  if(testplan == 1){
    //file downloading test
    long elapse = fileDownloadTest(actual);
    post(rssi, local_ip, local_ping_success, remote_ping_success, elapse, actual, r);//post what we're going to wait for between tests (randomly 5-x)
    Serial.print("Random wait of x seconds is: ");
    Serial.println(r);
    digitalWrite(D0, HIGH);
    delay(r * 1000L);
  }else{
    //streaming testing
    //LOG_INTERVAL_MAX becomes the time to stream for, data posting happens from the call
    streamBufferTest(rssi, local_ip, local_ping_success, remote_ping_success, actual, LOG_INTERVAL_MAX);//always go with specified time via server
    digitalWrite(D0, HIGH);
    delay(5000L);//5 seconds between video streams
  }



}

