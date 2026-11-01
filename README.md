# Spotify_song_display_ESP32_OLED0.96
Spotify song display on 0.96 inch oled screen using esp 32

items needed - breadboard , jumper wires , esp32 , usb c type cable , oled 0.96 inch , Spotify premium account
Hardware part - 

1 CONNECT THE OLED 0.96 AND ESP32 AS GIVEN IN THE TABLE

OLED	 = ESP32
GND = 	 GND
VDD	 =   3V3
SCK	 =   G22 
SDA	 =   G21


SOFTWARE - 

STEP 1)
i - logged in to your spotify premimum account
ii - go to developer.spotify.com/dashboard
iii - create app
iv - Redirect URI: http://127.0.0.1:8888/callback, and tick Web API
v- Copy the Client ID and Client Secret

STEP 2)
i - Paste this link into Notepad, replace YOUR_CLIENT_ID with yours, then open it in your browser:
https://accounts.spotify.com/authorize?client_id=YOUR_CLIENT_ID&response_type=code&redirect_uri=http://127.0.0.1:8888/callback&scope=user-read-currently-playing%20user-read-playback-state
ii - Click Agree. The page will show an error like “This site can’t be reached.” That is  normal.
In the address bar, copy everything after code=. That is  your code. It expires in a few minutes ( so do very quickly now).

STEP 3)
i- open googles collab then new notebook and then run the code given  in file as "Collab_run_code" to get the refresh token of your premium account.
ii - when u get the refresh token copy and save it . ( if it dont comes means u did it slow so do it fast untill u get refresh token )
iii - Open Arduino software and run the code in file "Main_code_spotify" and replace these things with your information 
const char* WIFI_SSID     = "your wifi name";
const char* WIFI_PASSWORD = "your wifi password";
const char* CLIENT_ID     = "...";
const char* CLIENT_SECRET = "...";
const char* REFRESH_TOKEN = "...";
iv run the code and the oled screen will work and run any song through the same account.
V done
