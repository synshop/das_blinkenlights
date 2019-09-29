#include <iostream>
#include <string>
#include <ctime>
#include <csignal>

#include <sstream>
#include <locale>
#include <iomanip>

#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <stdint.h>
#include <unistd.h>

#include <yaml.h>

using namespace std;


#define NUM_LEDS 646
#define FADE_VAL 1
#define FAST_FADE_VAL 16
#define EFFECT_DELAY 120
#define WAIT_DELAY 30

#define EFFECTS 6

uint8_t buffer[NUM_LEDS * 3];
uint8_t alt_buffer[NUM_LEDS * 3];

int signaled = 0;
uint8_t lights_on = 0;


// functions

void signalHandler(int signum)
{
  signaled = signum;
  cout << "signal: " << signaled << endl;
}

void displayBuffer(void)
{
  uint8_t led_frame[4];

  uint8_t brightness;

  //brightness = 15;

  // max brightness to reduce end of strip flicker
  brightness = 7;

  // start of frame all 0x00
  uint8_t buf[1];
  for(int i = 0; i < 4; i++) {
    buf[0] = 0x00;
    wiringPiSPIDataRW(0, buf, 1);
  }

  // write out frame
  for(int i = 0; i < NUM_LEDS; i++)
  {
    led_frame[0] = 0b11100000 | (0b00011111 & brightness);

    led_frame[1] = buffer[i*3];
    led_frame[2] = buffer[i*3+1];
    led_frame[3] = buffer[i*3+2];

    wiringPiSPIDataRW(0, led_frame, 4);
  }

  // end of frame all FFs
  for(int i = 0; i < 4; i++) {
    buf[0] = 0xFF;
    wiringPiSPIDataRW(0, buf, 1);
  }
}

uint8_t InSchedule(void)
{
  /*
    Function reads in schedule from YAML formatted configuration file to determine if
    it's currently time to turn on the lights or not.
  */

  uint8_t in_schedule = 0;

  time_t now = time(0);
  tm *ltm = localtime(&now);

  stringstream parse_time;
  string hr_time, min_time;
  int hr_time_int, min_time_int, start_secs, end_secs;

  const static string dow[] = {
    "Su",
    "Mo",
    "Tu",
    "We",
    "Th",
    "Fr",
    "Sa"
  };

  FILE *fh = fopen("schedule.conf", "r");
  yaml_parser_t parser;
  yaml_event_t  event;   /* New variable */

  uint8_t in_events = 0;
  uint8_t in_sequence = 0;
  uint8_t in_mapping = 0;
  uint8_t in_read = 0;

  string event_name, start_time, end_time, day_of_week, disabled;

  /* Initialize parser */
  if(!yaml_parser_initialize(&parser))
    fputs("Failed to initialize parser!\n", stderr);
  if(fh == NULL)
    fputs("Failed to open schedule file!\n", stderr);

  /* Set input file */
  yaml_parser_set_input_file(&parser, fh);

  /* START new code */
  do {
    if (!yaml_parser_parse(&parser, &event)) {
       printf("Parser error %d\n", parser.error);
       exit(EXIT_FAILURE);
    }

    switch(event.type)
    {
    case YAML_NO_EVENT:
      //puts("No event!");
      break;
    /* Stream start/end */
    case YAML_STREAM_START_EVENT:
      //puts("STREAM START");
      break;
    case YAML_STREAM_END_EVENT:
      //puts("STREAM END");
      break;
    /* Block delimeters */
    case YAML_DOCUMENT_START_EVENT:
      //puts("Start Document");
      break;
    case YAML_DOCUMENT_END_EVENT:
      //puts("End Document");
      break;
    case YAML_SEQUENCE_START_EVENT:
      //puts("Start Sequence");
      break;
    case YAML_SEQUENCE_END_EVENT:
      //puts("End Sequence");
      in_events = 0;
      break;
    case YAML_MAPPING_START_EVENT:
      //puts("Start Mapping");
      break;
    case YAML_MAPPING_END_EVENT:
      //puts("End Mapping");

      if(in_events)
      {
        uint8_t event_in_event = 0;

        // cout << "Event: " << event_name << ", " << start_time << " - " << end_time << " : " << day_of_week << endl;
        int secs = (ltm->tm_hour * 3600) + (ltm->tm_min * 60) + ltm->tm_sec;
        // cout << "time: " << secs << " - " << dow[ltm->tm_wday] << " - " << ltm->tm_hour << " : " << ltm->tm_min << endl;

        if(start_time != "")
        {
          // clear variables
          parse_time.clear();
          hr_time.clear();
          min_time.clear();

          parse_time<<start_time;

          getline(parse_time, hr_time, ':' );
          getline(parse_time, min_time, ':' );

          hr_time_int = atoi(hr_time.c_str());
          min_time_int = atoi(min_time.c_str());
          start_secs = (hr_time_int * 3600) + (min_time_int * 60);

          // cout << start_secs << " - " << hr_time_int << " : " << min_time_int << endl;

          if(secs >= start_secs)
          {
            // It's past the start time
            event_in_event = 1;
          }
        }

        if(end_time != "")
        {
          // clear variables
          parse_time.clear();
          hr_time.clear();
          min_time.clear();

          parse_time<<end_time;

          getline(parse_time, hr_time, ':' );
          getline(parse_time, min_time, ':' );

          hr_time_int = atoi(hr_time.c_str());
          min_time_int = atoi(min_time.c_str());
          end_secs = (hr_time_int * 3600) + (min_time_int * 60);

          // cout << end_secs << " - " << hr_time_int << " : " << min_time_int << endl;

          if(secs >= end_secs)
          {
            // It's past the end time
            event_in_event = 0;
          }
        }

        if(day_of_week != "")
        {
          if(day_of_week.find(dow[ltm->tm_wday]) == std::string::npos)
          {
            // Day of the week not found in schedule
            event_in_event = 0;
          }
        }

        if(disabled == "true")
        {
          // Schedule is disabled
          event_in_event = 0;
        }
        in_schedule |= event_in_event;

      }

      start_time.clear();
      end_time.clear();
      day_of_week.clear();
      disabled.clear();

      break;
    /* Data */
    case YAML_ALIAS_EVENT:
      //printf("Got alias (anchor %s)\n", event.data.alias.anchor);
      break;
    case YAML_SCALAR_EVENT:

      if(in_events)
      {
        // we've found that we're in the events section so ok to read data

        if(in_read)
        {
          // we've previously found a data type to read so next bit of data goes in there
          switch(in_read)
          {
            case 1:
              // read in Event Name next
              event_name = reinterpret_cast<char*>(event.data.scalar.value);

              break;
            case 2:
              // read in Start Time next
              start_time = reinterpret_cast<char*>(event.data.scalar.value);

              break;
            case 3:
              // read in End Time next
              end_time = reinterpret_cast<char*>(event.data.scalar.value);

              break;
            case 4:
              // read in Day of Week next
              day_of_week = reinterpret_cast<char*>(event.data.scalar.value);

              break;
            case 5:
              // read in Disabled next
              disabled = reinterpret_cast<char*>(event.data.scalar.value);

              break;
          }
          in_read = 0;
        }
        else
        {
          // read scalar of variable names
          if(strcmp(reinterpret_cast<const char *>(event.data.scalar.value), "event_name") == 0)
          {
            // read in Event Name next
            in_read = 1;
          }
          if(strcmp(reinterpret_cast<const char *>(event.data.scalar.value), "start_time") == 0)
          {
            // read in Start Time next
            in_read = 2;
          }
          if(strcmp(reinterpret_cast<const char *>(event.data.scalar.value), "end_time") == 0)
          {
            // read in End Time next
            in_read = 3;
          }
          if(strcmp(reinterpret_cast<const char *>(event.data.scalar.value), "day_of_week") == 0)
          {
            // read in Day of Week next
            in_read = 4;
          }
          if(strcmp(reinterpret_cast<const char *>(event.data.scalar.value), "disabled") == 0)
          {
            // read in Disabled
            in_read = 5;
          }
        }
      }

      if(strcmp(reinterpret_cast<const char *>(event.data.scalar.value), "Events") == 0)
      {
        in_events = 1;
        // puts("Found Events");
      }
      break;
    }
    if(event.type != YAML_STREAM_END_EVENT)
      yaml_event_delete(&event);
  } while(event.type != YAML_STREAM_END_EVENT);
  yaml_event_delete(&event);
  /* END new code */

  /* Cleanup */
  yaml_parser_delete(&parser);
  fclose(fh);

/*

read config and find mapping scalar of Events
start of stream
 start of mapping
 - look for "Events"

 look for start of sequence
   read in mappings till end of sequence
   look for start of mapping
     read in scalars till end of mapping
     process event


STREAM START
Start Document
Start Mapping
Got scalar (value Events)

Start Sequence
Start Mapping
Got scalar (value event_name)
Got scalar (value Weekdays)
Got scalar (value start_time)
Got scalar (value 18:00)
Got scalar (value end_time)
Got scalar (value 22:00)
Got scalar (value day_of_week)
Got scalar (value Mo,Tu,We,Th,Fr)
End Mapping
Start Mapping
Got scalar (value event_name)
Got scalar (value Saturdays)
Got scalar (value start_time)
Got scalar (value 15:00)
Got scalar (value end_time)
Got scalar (value 22:00)
Got scalar (value day_of_week)
Got scalar (value Sa)
End Mapping
End Sequence
End Mapping
End Document
STREAM END

*/

  return in_schedule;
}

// effect sub functions

void fadeBuffer(uint8_t fade_val)
{
  for(int i = 0; i < NUM_LEDS * 3; i++)
  {
    if(buffer[i] > fade_val)
    {
      buffer[i] = buffer[i] - fade_val;
    }
    else if(buffer[i] > 0)
    {
      buffer[i] = 0;
    }
  }
}

uint8_t RandomColor(uint8_t seed)
{
  if((rand() % 255) < seed)
  {
    return rand() % 255;
  }
  else
  {
    return 0;
  }
}

void Rotate(uint8_t direction)
{
  uint8_t r, g, b;

  if(direction)
  {
    b = buffer[0];
    g = buffer[1];
    r = buffer[2];
    for(int i = 0; i < (NUM_LEDS - 1); i++)
    {
      buffer[i*3]   = buffer[(i+1)*3];
      buffer[i*3+1] = buffer[(i+1)*3+1];
      buffer[i*3+2] = buffer[(i+1)*3+2];
    }
    buffer[(NUM_LEDS-1)*3]   = b;
    buffer[(NUM_LEDS-1)*3+1] = g;
    buffer[(NUM_LEDS-1)*3+2] = r;
  }
  else
  {
    b = buffer[(NUM_LEDS-1)*3];
    g = buffer[(NUM_LEDS-1)*3+1];
    r = buffer[(NUM_LEDS-1)*3+2];
    for(int i = (NUM_LEDS - 1); i > 0; i--)
    {
      buffer[i*3]   = buffer[(i-1)*3];
      buffer[i*3+1] = buffer[(i-1)*3+1];
      buffer[i*3+2] = buffer[(i-1)*3+2];
    }
    buffer[0] = b;
    buffer[1] = g;
    buffer[2] = r;
  }
}

void Fill(int start_led, int end_led, float r1, float g1, float b1, float r2, float g2, float b2)
{
  float r, g, b, r_inc, g_inc, b_inc;
  int steps;

  steps = end_led - start_led;

  if(steps)
  {
    r_inc = (r2 - r1)/steps;
    g_inc = (g2 - g1)/steps;
    b_inc = (b2 - b1)/steps;

    r = r1;
    g = g1;
    b = b1;

    for(int i = start_led; i <= end_led; i++)
    {
      buffer[i*3] = b;
      buffer[i*3+1] = g;
      buffer[i*3+2] = r;
      r = r + r_inc;
      g = g + g_inc;
      b = b + b_inc;
    }
  }
  else
  {
    // we can't divide by zero so just set the start LED to color 1
    buffer[start_led*3] = b1;
    buffer[start_led*3+1] = g1;
    buffer[start_led*3+2] = r1;
  }
}



// effect functions

void FadeOut(void) {
  // fade out
  for (uint8_t loops=0; loops < 16; loops++)
  {
    fadeBuffer(FAST_FADE_VAL);
    displayBuffer();
    usleep(20);
  }
}

void WaitUntil(long num_seconds) {
  // fade out
  time_t until = time(0) + num_seconds;

  while(time(0) < until)
  {
    usleep(200);
    if(signaled)
    {
      break;
    }
  }
}

void Sparkle(long num_seconds)
{
  uint8_t red_val = 0;
  uint8_t green_val = 0;
  uint8_t blue_val = 0;

  cout << "Random Sparkle\n";

  time_t until = time(0) + num_seconds;

  while(time(0) < until)
  {
    int pwmnum = rand() % NUM_LEDS;
    red_val = RandomColor(128);
    green_val = RandomColor(128);
    blue_val = RandomColor(128);

    buffer[pwmnum*3] = blue_val;
    buffer[pwmnum*3+1] = green_val;
    buffer[pwmnum*3+2] = red_val;

    displayBuffer();
    fadeBuffer(FADE_VAL);
    usleep(20);
    if(signaled)
    {
      break;
    }
  }
}

void RandomTwoColorSparkle(long num_seconds)
{
  uint8_t r1, g1, b1, r2, g2, b2;

  r1 = RandomColor(128);
  g1 = RandomColor(128);
  b1 = RandomColor(128);
  b2 = RandomColor(128);
  g2 = RandomColor(128);
  r2 = RandomColor(128);

  cout << "Random Two Color Sparkle\n";

  time_t until = time(0) + num_seconds;

  while(time(0) < until)
  {
    int pwmnum = rand() % NUM_LEDS;

    if(rand() % 255 > 128)
    {
      buffer[pwmnum*3] = b1;
      buffer[pwmnum*3+1] = g1;
      buffer[pwmnum*3+2] = r1;
    }
    else
    {
      buffer[pwmnum*3] = b2;
      buffer[pwmnum*3+1] = g2;
      buffer[pwmnum*3+2] = r2;
    }

    displayBuffer();
    fadeBuffer(FADE_VAL);
    usleep(20);
    if(signaled)
    {
      break;
    }
  }
}

void RandomTwoColorFade(long num_seconds)
{
  uint8_t r1, g1, b1, r2, g2, b2;
  uint8_t direction = rand() % 2;

  cout << "Random Two Color Fade ";
  if(direction)
  {
    cout << "Right\n";
  }
  else
  {
    cout << "Left\n";
  }

  r1 = RandomColor(128);
  g1 = RandomColor(128);
  b1 = RandomColor(128);
  b2 = RandomColor(128);
  g2 = RandomColor(128);
  r2 = RandomColor(128);

  Fill(0,321,r1,g1,b1,r2,g2,b2);
  Fill(322,645,r2,g2,b2,r1,g1,b1);
  displayBuffer();

  time_t until = time(0) + num_seconds;

  while(time(0) < until)
  {
    displayBuffer();
    Rotate(direction);
    usleep(100);
    if(signaled)
    {
      break;
    }
  }
}

void Rainbow(long num_seconds)
{
  uint8_t direction = rand() % 2;

  cout << "Rainbow Cycle ";
  if(direction)
  {
    cout << "Right\n";
  }
  else
  {
    cout << "Left\n";
  }

  float inc = NUM_LEDS / 6;

  Fill(0,        int(inc),     255,0,  0,    255,255,0);
  Fill(int(inc), int(inc*2),   255,255,0,    0,  255,0);
  Fill(int(inc*2), int(inc*3), 0,  255,0,    0,  255,255);
  Fill(int(inc*3), int(inc*4), 0,  255,255,  0,  0,  255);
  Fill(int(inc*4), int(inc*5), 0,  0,  255,  255,0,  255);
  Fill(int(inc*5), (NUM_LEDS-1), 255,0,  255,  255,0,  0);

  displayBuffer();

  time_t until = time(0) + num_seconds;

  while(time(0) < until)
  {
    displayBuffer();
    Rotate(direction);
    usleep(100);
    if(signaled)
    {
      break;
    }
  }
}


void RandomWhite(long num_seconds)
{
  cout << "Random White\n";

  time_t until = time(0) + num_seconds;

  while(time(0) < until)
  {
    int pwmnum = rand() % NUM_LEDS;
    uint8_t pwmval = rand() % 255;
    buffer[pwmnum*3] = pwmval;
    buffer[pwmnum*3+1] = pwmval;
    buffer[pwmnum*3+2] = pwmval;
    displayBuffer();
    fadeBuffer(FADE_VAL);
    usleep(20);
    if(signaled)
    {
      break;
    }
  }
}


// main function

int main()
{
  srand (time(NULL));

  int current_effect = 0;
  int next_effect = 0;

  void(*prev_handler)(int);
  prev_handler = signal(SIGUSR1, signalHandler);
  prev_handler = signal(SIGINT, signalHandler);

  wiringPiSetup();
  if(wiringPiSPISetup(0, 6000000) < 0) {
    std::cerr << "wiringPiSPISetup failed" << std::endl;
  }

  uint8_t led_frame[4];
  uint8_t r, g, b, brightness;

  r = 0x00;
  g = 0x00;
  b = 0x00;

  // max brightness to reduce end of strip flicker
  brightness = 7;

  // set frame buffer
  for(int i = 0; i < NUM_LEDS; i++)
  {
    buffer[i*4] = b;
    buffer[i*4+1] = g;
    buffer[i*4+2] = r;
  }

  // main loop
  while(signaled != 2)
  {
    signaled = 0;

    while(!signaled)
    {
      time_t now = time(0);
      char* dt = ctime(&now);
      cout << dt;

      lights_on = InSchedule();

      if(lights_on)
      {
        // if we're on, pick a random effect different than the last one displayed
        while(next_effect == current_effect)
        {
          next_effect = 1 + (rand() % (EFFECTS - 1));
        }
        current_effect = next_effect;
      }
      else
      {
        // lights out
        next_effect = 0;
        current_effect = 0;
      }

      // display the current effect
      switch(current_effect)
      {
        case 0:
          // lights out
          FadeOut();
          WaitUntil(WAIT_DELAY);
          break;
        case 1:
          Rainbow(EFFECT_DELAY);
          FadeOut();
          break;
        case 2:
          RandomTwoColorFade(EFFECT_DELAY);
          FadeOut();
          break;
        case 3:
          RandomWhite(EFFECT_DELAY);
          FadeOut();
          break;
        case 4:
          Sparkle(EFFECT_DELAY);
          FadeOut();
          break;
        case 5:
          RandomTwoColorSparkle(EFFECT_DELAY);
          FadeOut();
          break;
      }
    }
  }

  // make sure the lights are off
  FadeOut();
  return(0);
}

