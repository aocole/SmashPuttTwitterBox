#include <Ethernet.h>
#include <SPI.h>
#include <TextFinder.h>
#include <LiquidCrystal.h>

boolean DEBUG = true;

int LIGHT_PIN = 2;

LiquidCrystal lcd(4, 5, 6, 7, 8, 9);

byte mac[] = { 0x90, 0xA2, 0xDA, 0x00, 0x51, 0x16 };
EthernetClient client;

TextFinder finder( client );  

int loopCount;
int tweetCount;

String last_tweet;
String last_author;
long last_timestamp;

void setup() {
  Serial.begin(9600);
  delay(100);

  // Hook up the light hardware
  pinMode(LIGHT_PIN, OUTPUT);     

  // Get the LCD fired up
  lcd.begin(16, 2);
  printMessage("TwitterBox v2.0");
  
  // Turn on the network
  printMessage("Starting up ", "Ethernet...");
  // start the Ethernet connection:
  if (Ethernet.begin(mac) == 0) {
    Serial.println("Failed to configure Ethernet using DHCP");
    // no point in carrying on, so do nothing forevermore:
    for(;;)
      ;
  }
  delay(1000);
  
  loopCount = 0;
  tweetCount = 0;
}

void loop() {   
   loopCount++;

   // Look for a new tweet
   if (getTweet()) {
      // Turn the light on
      if(DEBUG) Serial.println("Turning the light on...");
      digitalWrite(LIGHT_PIN, HIGH);   
  
      // Print the new tweet
      if(DEBUG) Serial.println(last_tweet);
      printMessage("New tweet from:", last_author, 10000);
     
      // Turn the light off
      if(DEBUG) Serial.println("Turning the light off...");
      digitalWrite(LIGHT_PIN, LOW);
   } else {
      printMessage("No new tweets");
      printMessage("Last tweet from:", last_author);
   }     
    
    // Clean up
    client.stop();
    client.flush();
    
    // A few closing messages
    printMessage("Tweets Counted:", String(tweetCount));
    //printMessage("Total Loops:", loopCount);
    //printMessage("Last tweet on:", last_timestamp);
    //printMessage("Pausing before", "next attempt...", 5000);
}
  
boolean getTweet() {
   boolean return_value;
  
   printMessage("Searching for", "@officenomads");
   if (client.connect("search.twitter.com", 80)) {
      if(DEBUG) Serial.println("Connection Established!");
      client.println("GET http://search.twitter.com/search.atom?q=%40officenomads HTTP/1.0"); 
      client.println("Host: search.twitter.com");
      client.println("Connection: close");
      client.println("Accept-Charset: ISO-8859-1,UTF-8;q=0.7,*;q=0.7");
      client.println("Cache-Control: no-cache");
      client.println("Accept-Language: de,en;q=0.7,en-us;q=0.3");
      client.println("Referer: http://officenomads.com/");
      client.println();
      if(DEBUG) Serial.println("Search request sent");
    } else {
      printMessage("Connection to", "twitter failed!");
    } 
        
    /*
    <entry>
      <id>tag:search.twitter.com,2005:112221813009944576</id>    
      <published>2011-09-09T17:52:19Z</published>
      <link type="text/html" href="http://twitter.com/she_bikes/statuses/112221813009944576" rel="alternate"/>
      <title>@officenomads we meet again</title>    
      <content type="html">&lt;em&gt;@officenomads&lt;/em&gt; we meet again</content>    
      <updated>2011-09-09T17:52:19Z</updated>    
      <link type="image/png" href="http://a2.twimg.com/profile_images/1056271498/mms_picture__33__normal.jpg" rel="image"/>
      <twitter:geo/>
      <twitter:metadata>      
      <twitter:result_type>recent</twitter:result_type>
      </twitter:metadata>
      <twitter:source>&lt;a href="http://twitter.com/"&gt;web&lt;/a&gt;</twitter:source>
      <twitter:lang>en</twitter:lang>
      <author>
        <name>she_bikes (Alexandra)</name>
        <uri>http://twitter.com/she_bikes</uri>
      </author>
    </entry>    
    */
    
    if (client.connected()) {
       if(finder.find("entry") ) { 
          // Pull the tweet from the <title> elemet      
          char tweet_buffer[140];
          finder.getString("<title>" ,"</title>", tweet_buffer, 140);
          String new_tweet = String(tweet_buffer);
          
          // Stop right now if we've seen this before
          if (new_tweet == last_tweet) {
            if(DEBUG) Serial.println("We've seen this tweet before! Moving on...");
            return false;
          }
          
          // Pull the timestamp from the <updated> element
          char ts_buffer[17];
          finder.getString("<updated>" ,"</updated>", ts_buffer, 17);
          //Serial.print(ts_buffer);
          //2011-09-09T17:52
          //0123456789012345
          ts_buffer[0] = ts_buffer[2];  
          ts_buffer[1] = ts_buffer[3];
          ts_buffer[2] = ts_buffer[5];  
          ts_buffer[3] = ts_buffer[6];
          ts_buffer[4] = ts_buffer[8];
          ts_buffer[5] = ts_buffer[9];
          ts_buffer[6] = ts_buffer[11];
          ts_buffer[7] = ts_buffer[12];
          ts_buffer[8] = ts_buffer[14];
          ts_buffer[9] = ts_buffer[15];
          for (int i = 10; i < 16; i++) {
            ts_buffer[i] = ' ';
          }
          long new_timestamp = atol(ts_buffer);
          if(DEBUG) {
            Serial.print("New TS: ");
            Serial.println(new_timestamp);
          }
          
          // Stop right now if this tweet is old
          if (new_timestamp <= last_timestamp) {
            if(DEBUG) Serial.println("We found an old tweet! Moving on...");
            return false;
          }
          
          // We can assume at this point it's a new tweet so we can save it for later
          last_tweet = new_tweet;
          last_timestamp = new_timestamp;
          tweetCount++;

          // Save the author
          char author_buffer[16];
          finder.getString("<name>" ," (", author_buffer, 16);        
          last_author = "@" + String(author_buffer);
          last_author.toLowerCase();
       } else {
         printMessage("Cound not tweet", "Trying again...", 5000);
         return false;
       }
    } else {
       printMessage("Disconnected!", "Trying again...", 5000);
       return false;
    } 
    
    return true; 
}

void printMessage(String line1) {
  printMessage(line1, "");
}

void printMessage(String line1, String line2) {
  printMessage(line1, line2, 3000);
}

void printMessage(String line1, String line2, int delayMS) {
  // One to the serial
  if(DEBUG) {
    Serial.println(line1);
    if (line2 != "") {
      Serial.println(line2);
    }
  }
  
  // Another to the LCD
  lcd.clear();
  lcd.setCursor(0, 0);
  lcd.print(line1);
  lcd.setCursor(0, 1);
  lcd.print(line2);
  delay(delayMS);
  //lcd.clear();  
}  

