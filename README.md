# TRMNL LilyGo EPD47
# Features
* Queries /api/display endpoint
  * MAC - auto filled
  * Access-Token - set with `config.hpp`
  * Battery-Voltage - calculated
  * RSSI - calculated
  * Colors - uses a uniform pallet of 16 grays, see `config.hpp`
  * Width - auto filled
  * Height - auto filled
* Uses a gamma of 0.7 to brighten up images
* Recieves a **8bit** grayscale and downsamples to **4bit** [BYOS Laravel](https://github.com/usetrmnl/byos_laravel) has no 4bit support
* Manual refresh using the button
* Deep Sleep after every refresh
# Usage
* Use "auto-join" on Laravel
* In `config.hpp` set the following
  * ACCESS_TOKEN
  * API_URL
  * SSID
  * PASSWORD
# Notes
I was interesetd in the TRMNL devices and software, but saw there was no support for the dev board I had bought.
I thought I'd wait for official support for it, but since epdiy wasn't merged I don't think this will happen.
Porting the TRMNL firmware seems too difficult for now and the API is so simple so I decided to write my own.
