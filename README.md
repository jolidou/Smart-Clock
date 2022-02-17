# Smart Clock - Joli Dou

---

# Overview

## Demo Video:

[demonstration video](https://youtu.be/MGcN4Ic7-s4)

## Implementing Specifications:

1. Display Modes:
   I kept track of the current display state, and when the button on Pin 39 was pressed and released, I toggled to the display that was not the current one. To implement IMU mode, I created variables for average acceleration, gravitational acceleration, a motion threshold, and the time of the latest motion detected. To set the proper display, I checked if the display mode was IMU, and if not, I just displayed time as normal (ALWAYS_ON). A design note is that when toggling to IMU_MODE, I reset the latest motion to toggle time. I decided it was more intuitive to only consider motion after users had gone into IMU mode.
   IMU Mode:
   I set up the IMU and used it to obtain average acceleration magnitude in the same manner as in the step counter lab. I then checked if average acceleration exceeded the motion threshold (determined experimentally to be 11.5). If it did, I reset the motion timer to the current time, and if not, if the time since latest motion exceeded the timeout of 15s, I "turned off" the display by writing an empty string to it. If the mode was ALWAYS_ON, I just displayed the time as normal.

2. Timing Modes:
   For the Hour:Minute mode, I created a character array with the colon, and one without, and every second, I switched the display between them. This created the desired "blinking colon" display. To time the switching, I maintained a variable to track latest colon switching time.
   For the Hour:Minute:Second mode, I created a character array with zero-padded seconds (i.e. if seconds was a single digit, I added a zero to the left).
   To toggle between timing modes, I used a similar approach to the display mode state machine (see Display Modes).

3. Getting Current Time:
   To make HTTP requests, I used the timeouts, buffers, and do_http_GET/char_append code from lab 2. However, I changed the URL to the time API, and I set the getting period to be every minute (60000ms). For requests to be made every minute but time to update every second, I had to manually update seconds. To do this, every 1000ms I incremented a seconds variable, and wrapped seconds back to 0 upon reaching 60. From the HTTP request, I used memset and strncpy methods to obtain only the parts of the response buffer corresponding to hours and minutes. I converted hours to an int using atoi and converted it from military time.
   To create the time printed on the screen, I used sprintf with the hours (int), minutes (character array), and seconds (int, padded using %02d). For differences in printing time specific to each timing mode, see Timing Modes section above.

## Design Strategies:

1. String (Parsing)
   I used strncpy with offsets to obtain hours and minutes from the response buffer, with hours being strncpy(hourStr, response_buffer+11, 2) and minutes being strncpy(minuteStr, response_buffer + 14, 2). I used sprintf to display time. For example, in Hour:Min mode, I with %d:%s for time with colon and %d %s for time without colon. I also learned to use zero padding for seconds (%02d).
2. Military Time
   I used atoi to convert hours to a number, checked if it was greater than 12, and if so, subtracted 12 from it.
3. Display
   To minimize display rewriting, I used the empty string to create blackspace (e.g. for IMU mode when no motion was detected). I also padded the ends of all strings with it to avoid any overlap.

## Resources

1. Getting Time (in loop function)

```cpp
if ((startup == 0) || ((millis() - last_request_time) >= GETTING_PERIOD)) { // GETTING_PERIOD since last lookup? Look up again
    //formulate GET request...first line:
    strcpy(request_buffer, "GET http://iesc-s3.mit.edu/esp32test/currenttime HTTP/1.1\r\n");
    strcat(request_buffer, "Host: iesc-s3.mit.edu\r\n"); //add more to the end
    strcat(request_buffer, "\r\n"); //add blank line!
    //submit to function that performs GET.  It will return output using response_buffer char array
    do_http_GET("iesc-s3.mit.edu", request_buffer, response_buffer, OUT_BUFFER_SIZE, RESPONSE_TIMEOUT, true);
    // Serial.println(response_buffer); //print to serial monitor
    memset(minuteStr, 0, sizeof(minuteStr));
    memset(hourStr, 0, sizeof(hourStr));
    strncpy(hourStr, response_buffer + 11, 2);
    hours = atoi(hourStr);
    if (hours > 12) {
      hours = hours - 12;
    }
    strncpy(minuteStr, response_buffer + 14, 2);
    ++startup;
    last_request_time = millis();//remember when this happened so we perform next lookup in GETTING_PERIOD milliseconds
  }

  set_display();

  if (millis() - last_sec_update >= SEC_UPDATE) {
    ++seconds;
    if (seconds == 60) {
      seconds = 0;
    }

    last_sec_update = millis();
  }

```

2. Hour:Min Display:

```cpp
//1) Hour: Minute - colon flashes on-off at 0.5 Hz with 50% duty cycle (every 1000 ms)
void hour_min () {
  char withColon[100];
  char withoutColon[100];
  sprintf(withColon, "%d:%s              ", hours, minuteStr);
  sprintf(withoutColon, "%d %s              ", hours, minuteStr);

  if (millis() - last_colon_time >= 1000) {
    tft.setCursor(0,2,1);
    if (colon_state == 0) {
      tft.print(withColon);
      colon_state = 1;
    } else {
      tft.print(withoutColon);
      colon_state = 0;
    }
    last_colon_time = millis();
  }
}
```

3. Hour:Min:Sec Display:

```cpp
void hour_min_sec() {
  char curr_time[100];
  sprintf(curr_time, "%d:%s:%02d              ", hours, minuteStr, seconds);
  tft.setCursor(0, 2, 1);
  tft.println(curr_time);
}
```

---

# Summary

In this project, I implemented a "Smart Clock" with two display and two timing modes. I implemented state machines to use buttons to toggle between these modes. Using string parsing and display options, I implemented the Hour:Min and Hour:Min:Second modes and their respective "heartbeat" signals. Then, I implemented IMU mode by calculating average acceleration magnitude from IMU readings and comparing it to a threshold. Some key concepts were (1)state machines, (2)string parsing, and (3)timing. Hardware concepts included IMU, buttons, and the LCD display.
