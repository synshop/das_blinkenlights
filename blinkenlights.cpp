#include <iostream>
#include <ctime>
#include <csignal>

#include <wiringPi.h>
#include <wiringPiSPI.h>
#include <stdint.h>
#include <unistd.h>


using namespace std;

#define NUM_LEDS 646
#define FADE_VAL 1
#define FAST_FADE_VAL 16

#define EFFECTS 4

uint8_t buffer[NUM_LEDS * 3];
uint8_t alt_buffer[NUM_LEDS * 3];

int signaled = 0;

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

void Sparkle(long num_loops)
{
  uint8_t red_val = 0;
  uint8_t green_val = 0;
  uint8_t blue_val = 0;

  cout << "Random Sparkle\n";

  for (long loop = 0; loop < num_loops; loop++)
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

void RandomTwoColorFade(long num_loops)
{
  uint8_t r1, g1, b1, r2, g2, b2;

  cout << "Random Two Color Fade\n";

  r1 = RandomColor(128);
  g1 = RandomColor(128);
  b1 = RandomColor(128);
  b2 = RandomColor(128);
  g2 = RandomColor(128);
  r2 = RandomColor(128);

  Fill(0,321,r1,g1,b1,r2,g2,b2);
  Fill(322,645,r2,g2,b2,r1,g1,b1);
  displayBuffer();

  for (long loop = 0; loop < num_loops; loop++)
  {
    displayBuffer();
    Rotate(1);
    usleep(100);
    if(signaled)
    {
      break;
    }
  }
}

void Rainbow(long num_loops)
{
  cout << "Rainbow Cycle\n";

  float inc = NUM_LEDS / 6;

  Fill(0,        int(inc),     255,0,  0,    255,255,0);
  Fill(int(inc), int(inc*2),   255,255,0,    0,  255,0);
  Fill(int(inc*2), int(inc*3), 0,  255,0,    0,  255,255);
  Fill(int(inc*3), int(inc*4), 0,  255,255,  0,  0,  255);
  Fill(int(inc*4), int(inc*5), 0,  0,  255,  255,0,  255);
  Fill(int(inc*5), (NUM_LEDS-1), 255,0,  255,  255,0,  0);

  displayBuffer();

  for (long loop = 0; loop < num_loops; loop++)
  {
    displayBuffer();
    Rotate(1);
    usleep(100);
    if(signaled)
    {
      break;
    }
  }
}


void RandomWhite(long num_loops)
{
  cout << "Random White\n";

  for (long loop = 0; loop < num_loops; loop++)
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
      cout << "time: " << dt << endl;

      next_effect = 0;
      while(next_effect == current_effect)
      {
        next_effect = rand() % EFFECTS;
      }
      cout << current_effect << ":" << next_effect << endl;
      current_effect = next_effect;

      switch(current_effect)
      {
        case 0:
          Rainbow(4000);
          FadeOut();
          break;
        case 1:
          RandomTwoColorFade(4000);
          FadeOut();
          break;
        case 2:
          RandomWhite(4000);
          FadeOut();
          break;
        case 3:
          Sparkle(4000);
          FadeOut();
          break;
      }
    }
  }

  FadeOut();
  return(0);
}

