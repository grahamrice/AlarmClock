//************libraries**************//
#include <Arduino.h>
#include <Wire.h>
#include <avr/interrupt.h>
//#include <TimeLib.h>
#include "RTClib.h"
//#include <LED_Display_Wrapper.h>

//************************************//
RTC_DS1307 rtc;

//LED_Display_Wrapper LEDdisplay = LED_Display_Wrapper();

//************Button*****************//
int BUTTON1=6; // Button SET MENU'
int BUTTON2=7; // Button +
int BUTTON3=8; // Button -
int BUTTON4=9; // SWITCH Alarm

//**************Alarm***************//
#define LED 13
#define buzzer 10
int snooze_time = 7;

//************Variables**************//
int hourupg;
int minupg;
int menu =0;
int setAll =0;

#define BLINK_S_LO 0x0001
#define BLINK_S_HI 0x0002
#define BLINK_M_LO 0x0004
#define BLINK_M_HI 0x0008
#define BLINK_H_LO 0x0010
#define BLINK_H_HI 0x0020
#define BLINK_S (BLINK_S_LO | BLINK_S_HI)
#define BLINK_M (BLINK_M_LO | BLINK_M_HI)
#define BLINK_H (BLINK_H_LO | BLINK_H_HI)
#define BLINK_CURSOR    0xC000
#define BLINK_CURSOR_HI 0x8000
#define BLINK_CURSOR_LO 0x4000
#define BLINK_BLUE      0x0100
#define BLINK_RED       0x0200
uint16_t blink_char = BLINK_CURSOR;

#define TIME_CURRENT      1
#define TIME_SET_H        2
#define TIME_SET_M        3
#define TIME_ALARM_H      4
#define TIME_ALARM_M      5
#define TIME_SNOOZE       6
#define TIME_ALARM_STATUS 7
#define TIME_AL_TONE      8
#define DISPLAY_OFF       0
uint8_t current_display = TIME_CURRENT;

#define FLAGS_DISPLAY 1
#define FLAGS_BUTTONS 2
#define FLAGS_SOUND   4
uint8_t interrupt_flags = 0;
uint8_t interrupt_counter = 0;
uint8_t interrupt_counter2 = 0; 

uint8_t button_time[4];
uint8_t button_last = 0;
//these will be issued as commands by the check_buttons function, which will take care of timing
#define BUTTON_MENU    0x01
#define BUTTON_DOWN    0x02
#define BUTTON_UP      0x04
#define BUTTON_ALARM   0x08
#define BUTTON_ALL     0x0F

#define BUTTON_MENU_H  0x10
#define BUTTON_DOWN_H  0x20
#define BUTTON_UP_H    0x40
#define BUTTON_ALARM_H 0x80
#define BUTTON_ALL_H   0xF0

#define TOUCH_THRESH 2
#define HOLD_THRESH  12
#define HOLD_HOLD    14

int alarmHours = 0, alarmMinutes = 0;  // Holds the current alarm time
int snooze_hours = 0, snooze_minutes = 0;

uint8_t menu_mode = 0;
#define MENU_OFF    0xff 
#define MENU_NONE   0
#define MENU_SET_HH 1
#define MENU_SET_MM 2
#define MENU_SAL_HH 3
#define MENU_SAL_MM 4
#define MENU_SAL_SN 5
#define MENU_SAL_TN 6
#define MENU_DSP_AL 7

#define RAW_LEDS_0 0x0003
#define RAW_LEDS_1 0x000C
#define RAW_LEDS_2 0x0030
#define RAW_LEDS_3 0x00C0
#define RAW_LEDS_4 0x0300
#define RAW_LEDS_S 0x0400
#define RAW_LEDS_W 0x0800

uint8_t alarm_status = 0;
#define ALARM_OFF 0
#define ALARM_SET 1
#define ALARM_SNZ 2
#define ALARM_TRG 4

uint8_t tone_status = 0;

/*--------------Interrupt driven display------------------*/

#define TIMER_RESET 62411

uint8_t sec_flash_counter = 0;

void print_number(int n, char * buf)
{
  if(n < 0) sprintf(buf,'  ');
  else if(n > 99) sprintf(buf,'**');
  else            sprintf(buf,'%02d',n);      
}

void display_time(int H, int M, int S)
{
  /* Will display numbers between 0 and 99 */
  /* Negative values mean nothing is displayed */
  int temp[3] = {H,M,S};
  char _str_buffer[7];  //6 chars and a null char...
  for(int i = 0; i < 3; i++)  print_number(temp[i],&_str_buffer[i*2]); 
  Serial.write(_str_buffer,7);
 // LEDdisplay.FillTextBuffer(_str_buffer);
}

void display_snooze_time(int ss)
{
  char _str_buffer[7];
  sprintf(_str_buffer,"SNZ %02d",ss);
Serial.write(_str_buffer,7);
 // LEDdisplay.FillTextBuffer(_str_buffer);
}

void display_alarm_status(bool s){
  char _str_buffer[7];
  if(s) sprintf(_str_buffer,"al  on");
  else  sprintf(_str_buffer,"al off");
Serial.write(_str_buffer,7);
  //LEDdisplay.FillTextBuffer(_str_buffer);
}

void update_display()
{ 
  uint16_t UI_Leds = 0;
  switch(sec_flash_counter){
    case 0: //turn on
            sec_flash_counter++;
            break;
    case 1: //turn off
            sec_flash_counter++;
            break;
    case 4: sec_flash_counter = 0;
            break;
    default: sec_flash_counter++;
             break;
  }     
        
        
  switch(current_display){
    case DISPLAY_OFF: //probs do nothing
                     break;
    case TIME_CURRENT:  DisplayDateTime (true, true, true);
                        if(sec_flash_counter == 2) UI_Leds |= (RAW_LEDS_1 | RAW_LEDS_3); 
                        UI_Leds |= (alarm_status & ALARM_SET) ? RAW_LEDS_W : 0;
                        break;
                        
    case TIME_SET_H:   if(sec_flash_counter < 3) DisplayDateTime (true, true, true);
                       else                      DisplayDateTime (false, true, true);
                       if(sec_flash_counter == 2) (RAW_LEDS_1 | RAW_LEDS_3);
                       UI_Leds |= (alarm_status & ALARM_SET) ? RAW_LEDS_W : 0;
                       UI_Leds |= RAW_LEDS_S;
                       break;
                       
    case TIME_SET_M:   if(sec_flash_counter < 3) DisplayDateTime (true, true, true);
                       else                      DisplayDateTime (true, false, true);
                       if(sec_flash_counter == 2) (RAW_LEDS_1 | RAW_LEDS_3);
                       UI_Leds |= (alarm_status & ALARM_SET) ? RAW_LEDS_W : 0;
                       UI_Leds |= RAW_LEDS_S;
                       break;
                       
    case TIME_ALARM_H: if(sec_flash_counter == 2) display_time(-1,alarmMinutes, -1);
                       else display_time(alarmHours,alarmMinutes, -1);
                       UI_Leds |= (alarm_status & ALARM_SET) ? RAW_LEDS_W : 0;
                       break;

    case TIME_ALARM_M: if(sec_flash_counter == 2) display_time(alarmHours,-1, -1);
                       else display_time(alarmHours,alarmMinutes, -1);
                       UI_Leds |= (alarm_status & ALARM_SET) ? RAW_LEDS_W : 0;
                       break;

    case TIME_SNOOZE: if(sec_flash_counter == 2) display_time(-1,-1, snooze_time);
                      else display_snooze_time(snooze_time);
                      if(sec_flash_counter < 3) UI_Leds |= RAW_LEDS_W;
                      break;
    case TIME_ALARM_STATUS: display_alarm_status((alarm_status & ALARM_SET) == ALARM_SET);
                            if(sec_flash_counter == 2)
                            {
                              UI_Leds |= (alarm_status & ALARM_SET) ? 0 : RAW_LEDS_W;
                            }else{
                              UI_Leds |= (alarm_status & ALARM_SET) ? RAW_LEDS_W : 0;
                            }                            
                            break;
                       
  }

 // LEDdisplay.writeDigitRaw(6, UI_Leds);
//  LEDdisplay.writeDisplay();
}

void sound_alarm()
{
  if(((alarm_status & ALARM_SET) == ALARM_SET) && ((alarm_status & ALARM_TRG) == ALARM_TRG))
  {
    tone_status++; //this is gonna get more complex
    if(tone_status & 1) analogWrite(buzzer,0x80);
    else analogWrite(buzzer,0);  
  }
  else analogWrite(buzzer,0);  
}

//button control must do lots of neat tricks for 4 touch buttons
// such as setting the alarm, will save automatically if there is no touch after 10 secs
// touch alarm to view alarm setting. Press mode to toggle alarm on/off (will flash blue 80% duty for on, 20% duty off, full on when set on and out of setting. Setting times out)
// hold mode to set the clock time
// touch all 4 to toggle display on/off
// hold alarm to set alarm, hh, mm, snooze time (and alarm sound?). Hold to enter mode and touch menu to cycle through (loopback too). Auto-completes with no touch in 7 secs or touch alarm to complete. Entering this mode will enable alarm
// touch all 4 to turn off alarm whilst alarm is sounding, hold all 4 whilst snoozing
// alarm light flsshes when snoozing
// touch 1 (<4) to snooze alarm
// display cannot be toggled off from alarm first sounding until it is disabled (stays on during snooze)

int check_buttons()
{
  uint8_t buttons = 0, mask = 0;
  int result = 0;

  if(digitalRead(BUTTON1)) buttons |= BUTTON_MENU;
  if(digitalRead(BUTTON2)) buttons |= BUTTON_DOWN;
  if(digitalRead(BUTTON3)) buttons |= BUTTON_UP;
  if(digitalRead(BUTTON4)) buttons |= BUTTON_ALARM;
  
  for(int j = 0; j < 4; j++)
  {
    mask = 1 << j;
    if((buttons & mask) == mask) button_time[j]++; //if button pressed, increment counter
    else if(button_time[j] != 0)                    //if not has it been pressed before?
    {
      if(button_time[j] > HOLD_THRESH ){ result |= (mask << 4); //we've been holding it, so issue that command
                                         button_time[j] = 0; } //don't hold and touch at the same time
      if(button_time[j] > TOUCH_THRESH)  result |= mask; //(else) we've touched it, so issue that command

      button_time[j] = 0; //button no longer pressed, so clear hold time. 
    }

    if(button_time[j] > HOLD_HOLD) //still pressed and we've reached the hold threshold
    {
      result |= (mask << 4);
      button_time[j]= (-1) * HOLD_HOLD; //if we're going to carry on holding then we'll count upwards so we don't trigger a touch again;
    }

    if(button_time == -1) //still pressed and we've reached the hold threshold again, issue hold command.
    {
      result |= (mask << 4);
      button_time[j] = (-1) * HOLD_HOLD;
    }
      
  }

  if(result != 0)
  {
    Serial.print("Button press: ");
    Serial.println(result);
  }

  return result;
}

//ISR(_VECTOR(14)) //interrupt every 50ms
ISR(TIMER1_OVF_vect)
{
  TCNT1 = TIMER_RESET;
  
  interrupt_flags |= FLAGS_BUTTONS; //buttons every 50ms
  
  if(++interrupt_counter == 4){ //display every 200ms
    interrupt_counter = 0;
    interrupt_flags |= FLAGS_DISPLAY;
    interrupt_counter2++;  
  }
  
  if(interrupt_counter2 == 2) //sound every 400ms
  {
    interrupt_counter2 = 0;
    interrupt_flags |= FLAGS_SOUND;
  }
}

void setup_rtc()
{
  if (! rtc.begin()) {
    Serial.write("RTC not found\n",15);
    
  }
  if (! rtc.isrunning()) {
    Serial.write("RTC NOT running!\n",18);
    rtc.adjust(DateTime(F(__DATE__), F(__TIME__)));
  }else{
    Serial.println("RTC is running!\n");
    DateTime now = rtc.now();
    // Set the date and time at compile time
    Serial.print(now.hour());
    Serial.print(":");
    Serial.println(now.minute());
  }
  
}

void setup()
{
  pinMode(BUTTON1,INPUT); //menu // https://www.arduino.cc/en/Tutorial/InputPullupSerial
  pinMode(BUTTON2,INPUT); //dec
  pinMode(BUTTON3,INPUT); //inc
  pinMode(BUTTON4,INPUT); //alarm
  pinMode(LED,OUTPUT);
  pinMode(buzzer, OUTPUT); // Set buzzer as an output

  Serial.begin(115200);
  while(!Serial);
  
  Serial.println("Setting up rtc");
  delay(500);

  setup_rtc();

  delay(500);

  int menu=0;

  //String _message = "Set the time      ";
  //LEDdisplay.ScrollText( _message );

  for(int j = 0; j < 4; j++) button_time[j] = 0;
  alarm_status = ALARM_OFF;
  
  //LEDdisplay.BLINK();

  TCCR1A = 0;
  TCCR1B = 0;

  noInterrupts(); //setup timer for periodic interrupts now that everything is ready

  TCNT1 = TIMER_RESET;      //for 50ms overflow = 3125*256/16MHz
  TCCR1B |= (1 << CS12);    // 256 prescaler 
  TIMSK1 |= (1 << TOIE1);   // enable timer overflow interrupt

  interrupts();
}

void loop()
{ 
  int buttons = 0;
  static int menu_timeout = -1;
  static int led_counter = 0;
  static int bc, dc;
  
  if((interrupt_flags & FLAGS_DISPLAY) == FLAGS_DISPLAY) { update_display(); dc++;}

  if((interrupt_flags & FLAGS_BUTTONS) == FLAGS_BUTTONS) { buttons = check_buttons(); bc++;}

  if((interrupt_flags & FLAGS_SOUND) == FLAGS_SOUND) sound_alarm();
  
  interrupt_flags = 0;

  //check_alarm();

  if( buttons != 0 )
  {
    Serial.print("\nButtons:");
    Serial.print(buttons);
    DateTime current = rtc.now();
    if((alarm_status & ALARM_TRG) == ALARM_TRG)
    {
      if(menu_mode == MENU_OFF) menu_mode = MENU_NONE; //alarm = display back on
      
      if((buttons == BUTTON_ALL)||(buttons == BUTTON_ALL_H)) alarm_status &= ALARM_SET;
      else if((buttons == BUTTON_MENU)||(buttons == BUTTON_DOWN)||(buttons == BUTTON_UP)||(buttons == BUTTON_ALARM))
      { //go into snooze mode
        if((alarm_status & ALARM_SNZ) == 0)
        {
          snooze_hours = alarmHours;
          snooze_minutes = alarmMinutes;  
        }
        alarm_status |= ALARM_SNZ;
        alarm_status &= ~ALARM_SET;
        add_snooze();
      }
    }else if((alarm_status & ALARM_SNZ) == ALARM_SNZ) 
    { //turn off when not sounding
      if(buttons == BUTTON_ALL_H)
      {
        snooze_hours = 0;
        snooze_minutes = 0;
        alarm_status &= ALARM_SET;
      }
    }else
    {
      switch(menu_mode)
      {
              case MENU_OFF  :   if(buttons == BUTTON_ALL) menu_mode = MENU_NONE;
                                 break; 
              case MENU_NONE :   if(buttons == BUTTON_MENU_H)
                                  {
                                    menu_mode = MENU_SET_HH;
                                    menu_timeout = 7000;
                                  }else if(buttons == BUTTON_ALARM)
                                  {
                                    menu_mode = MENU_DSP_AL;                                  
                                    menu_timeout = 3500;
                                  }else if(buttons == BUTTON_ALARM_H)
                                  {
                                    menu_mode = MENU_SAL_HH;
                                    menu_timeout = 7000;
                                  }else if(buttons == BUTTON_ALL) menu_mode = MENU_OFF;
                                  break;
              case MENU_SET_HH : if(buttons == BUTTON_MENU)
                                  {
                                    menu_mode = MENU_SET_MM;
                                  }else if(buttons == BUTTON_DOWN)
                                  {
                                    //ffs there is no nice rtc library, do this properly later
                                    rtc.adjust(current); 
                                  }else if(buttons == BUTTON_UP)
                                  {
                                    
                                  }
                                  menu_timeout = 7000; 
                                  break;
              case MENU_SET_MM : if(buttons == BUTTON_MENU)
                                  {
                                    menu_mode = MENU_NONE;
                                    menu_timeout = -1; 
                                  }else if(buttons == BUTTON_DOWN)
                                  {
                                    //ffs there is no nice rtc library, do this properly later
                                    rtc.adjust(current); 
                                    menu_timeout = 7000; 
                                  }else if(buttons == BUTTON_UP)
                                  {
                                    menu_timeout = 7000; 
                                  }
                                  break;
              case MENU_SAL_HH : if((buttons == BUTTON_MENU)||(buttons == BUTTON_ALARM))
                                  {
                                    menu_mode = MENU_SAL_MM;
                                  }else if(buttons == BUTTON_DOWN)
                                  {
                                    alarmHours--;
                                  }else if(buttons == BUTTON_UP)
                                  {
                                    alarmHours++;
                                  }
                                  menu_timeout = 5000;
                                  break;
              case MENU_SAL_MM : if((buttons == BUTTON_MENU)||(buttons == BUTTON_ALARM))
                                  {
                                    menu_mode = MENU_SAL_SN;
                                  }else if(buttons == BUTTON_DOWN)
                                  {
                                    alarmMinutes--;
                                  }else if(buttons == BUTTON_UP)
                                  {
                                    alarmMinutes++;
                                  }
                                  menu_timeout = 5000;
                                  break;
              case MENU_SAL_SN : if((buttons == BUTTON_MENU)||(buttons == BUTTON_ALARM))
                                  {
                                    //menu_mode = MENU_SAL_TN; //tone options later
                                    menu_mode = MENU_SAL_HH;
                                  }else if(buttons == BUTTON_DOWN)
                                  {
                                    snooze_time--;
                                  }else if(buttons == BUTTON_UP)
                                  {
                                    snooze_time++;
                                  }
                                  menu_timeout = 5000;
                                  break;
              case MENU_SAL_TN : menu_mode = MENU_SAL_HH;
                                  break;
              case MENU_DSP_AL : if(buttons == BUTTON_MENU) alarm_status ^= ALARM_SET; //toggle alarm setting
                                  else if(buttons == BUTTON_ALARM) menu_mode = MENU_NONE;
                                  break;
              default : menu_mode = MENU_NONE;         
      }
    }
  }

  
  if(menu_timeout >= 0)
  {
    if(menu_timeout == 0) menu_mode = MENU_NONE;
    
    switch(menu_mode)
    {
      case MENU_OFF    : current_display = DISPLAY_OFF;
                         break;
      case MENU_NONE   : current_display = TIME_CURRENT;
                         break;
      case MENU_SET_HH : current_display = TIME_SET_H;
                         break;
      case MENU_SET_MM : current_display = TIME_SET_M;
                         break;
      case MENU_SAL_HH : current_display = TIME_ALARM_H;
                         break;
      case MENU_SAL_MM : current_display = TIME_ALARM_M;
                         break;
      case MENU_SAL_SN : current_display = TIME_SNOOZE;
                         break;
      case MENU_SAL_TN : current_display = TIME_AL_TONE;
                         break;
      case MENU_DSP_AL : current_display = TIME_ALARM_STATUS;
                         break;    
    }
  }

  menu_timeout--;
  delay(1);

  if(++led_counter == 500)
  {
    led_counter = 0;
    digitalWrite(LED,!digitalRead(LED));
  }
}

void DisplayDateTime (bool h, bool m, bool s)
{
  static int old_secs = 0;
// We show the current date and time
  DateTime now = rtc.now();
  display_time( (h) ? now.hour() : -1, (m) ? now.minute() : -1, (s) ? now.second() : -1);
  if(old_secs != now.second()){
    Serial.println(now.second());
    old_secs = now.second();
  }
}

void add_snooze()
{
  snooze_minutes += snooze_time;
  if(snooze_minutes >= 60){
    snooze_minutes -= 60;
    snooze_hours += 1;
    if(snooze_hours >= 24) snooze_hours -= 24;
  }
}

void check_alarm()
{
  DateTime now = rtc.now();
  if ( now.hour() == alarmHours && now.minute() == alarmMinutes && now.second() == 0)
  {
    alarm_status |= ALARM_TRG;
  }else if(( now.hour() == snooze_hours) && (now.minute() == snooze_minutes) && (now.second() == 0))
  {
    alarm_status |= ALARM_TRG;
  }
}
