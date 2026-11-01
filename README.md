# 🎵 Spotify Song Display Using ESP32 + 0.96-inch OLED Display

Hi everyone!

I recently worked on a small ESP32 project that displays the currently playing Spotify song on a **0.96-inch OLED display**.

The project uses the Spotify Web API to retrieve the currently playing track and displays the song information on the OLED screen through an ESP32 connected to Wi-Fi.

## 🛠️ Components Required

1. ESP32 development board
2. 0.96-inch OLED display (I2C)
3. Breadboard
4. Jumper wires
5. USB Type-C cable (depending on your ESP32 board)
6. Spotify Premium account

## 🔌 Hardware Connections

Connect the OLED display to the ESP32 according to the following table:

| OLED Display | ESP32 |
|---|---|
| GND | GND |
| VDD | 3V3 |
| SCK / SCL | GPIO 22 |
| SDA | GPIO 21 |

**Hardware setup:** Connect the components using jumper wires and ensure the OLED is powered correctly.

## 💻 Software Setup

### Step 1: Create a Spotify Developer App

1. Log in to your Spotify Premium account.
2. Visit https://developer.spotify.com/dashboard
3. Create a new application.
4. Add the following Redirect URI:

   `http://127.0.0.1:8888/callback`

5. Enable the Web API.
6. Copy your Client ID and Client Secret and keep them private.

### Step 2: Authorize Your Spotify Account

1. Open Notepad.
2. Paste the following URL and replace `YOUR_CLIENT_ID` with your Spotify application's Client ID:

   `https://accounts.spotify.com/authorize?client_id=YOUR_CLIENT_ID&response_type=code&redirect_uri=http://127.0.0.1:8888/callback&scope=user-read-currently-playing%20user-read-playback-state`

3. Open the URL in your browser.
4. Log in and click Agree to authorize the application.
5. After authorization, the browser may show a connection error because no local callback server is running. This is expected in this manual procedure.
6. Copy the authorization code from the redirected URL and use it promptly, as authorization codes expire.

### Step 3: Generate the Refresh Token

1. Open Google Colab: https://colab.research.google.com/
2. Create a new notebook.
3. Open the provided `Collab_run_code` file.
4. Copy its code into the notebook and run it.
5. Follow the instructions to exchange your authorization code for a refresh token.
6. Copy and securely save the refresh token.

### Step 4: Upload the ESP32 Code

1. Open Arduino IDE.
2. Open the provided `Main_code_spotify` file.
3. Update the configuration with your own credentials:

   ```cpp
   const char* WIFI_SSID     = "your_wifi_name";
   const char* WIFI_PASSWORD = "your_wifi_password";

   const char* CLIENT_ID     = "your_client_id";
   const char* CLIENT_SECRET = "your_client_secret";
   const char* REFRESH_TOKEN = "your_refresh_token";
   ```

4. Select your ESP32 board and the correct COM port.
5. Compile and upload the program to your ESP32.

**Important:** Never upload your Wi-Fi password, Client Secret or Refresh Token to a public GitHub repository or share them in screenshots. Keep credentials in a private configuration file.

## 🎉 Final Result

Once everything is configured:

1. The ESP32 connects to Wi-Fi.
2. It communicates with the Spotify Web API.
3. It retrieves the currently playing track information.
4. The OLED displays the song information.
5. Play music using the same authorized Spotify account.

And that's it! Your Spotify Song Display using ESP32 and a 0.96-inch OLED is ready.

## 📂 Project Files

- `Collab_run_code` – Used to obtain the Spotify refresh token.
- `Main_code_spotify` – Main ESP32 Arduino code.

## 🚀 Conclusion

This project helped me explore ESP32, OLED displays, I2C communication, Wi-Fi connectivity and API integration.

It was a fun way to combine embedded systems with music and software development!

Feel free to share your suggestions or improvements.

#ESP32 #Arduino #Spotify #OLED #EmbeddedSystems #IoT #DIYProjects #Electronics
