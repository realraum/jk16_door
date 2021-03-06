#include <avr/io.h>
#include <avr/interrupt.h>

//********************************************************************//

#define HEARTBEAT_PIN 15 // blinking led indicating that system is active
#define HEARTBEAT_DURATION 10 // *10 ms, duration of heartbeat pulse
#define HEARTBEAT_DELAY 200   // *10 ms, 1/heartbeat-frequency
int heartbeat_cnt = 0;                    
        
#define LEDS_ON 0xFC
#define LEDS_OFF 0x00

#define LEDS_GREEN_COMMON_PIN 16
#define LEDS_RED_COMMON_PIN 17 
#define LED_DELAY 50 // *2 ms, between led shifts
int led_delay_cnt = 0;
byte next_led = 0;

#define LIMIT_OPENED_PIN 18 // A4: limit switch for open
#define LIMIT_CLOSED_PIN 19 // A5: limit switch for close

#define AJAR_PIN 14 // input pin for reed relais (door ajar/shut)
#define SHUT 10
#define AJAR 5
byte ajar_last_state = SHUT;
#define AJAR_LOW_PASS_TAU 200
byte ajar_low_pass_counter = 0;
byte ajar_low_pass_last_value = ajar_last_state;

#define MANUAL_OPEN_PIN 12  // keys for manual open and close
#define MANUAL_CLOSE_PIN 13 // 
#define DEBOUNCE_DELAY 6250  // * 16us = 100ms
#define DEBOUNCE_IDLE 0     // currently no debouncing
#define DEBOUNCE_OPEN 1     // debouncing open key
#define DEBOUNCE_CLOSE 2    // debouncing close key
#define DEBOUNCE_FINISHED 4 // debouncing finished
byte debounce_state;
int debounce_cnt = 0;

#define IDLE 0      // close and open may be called
#define OPENING 1   // opening, only 's' command is allowed
#define CLOSING 2   // closing, onyl 's' command is allowed
#define WAIT 3      // wait some time after open or close and hold last step
#define ERROR 4     // an error occured

#define CMD_OPEN 'o'
#define CMD_CLOSE 'c'
#define CMD_TOGGLE 't'
#define CMD_STATUS 's'
#define CMD_RESET 'r'

#define STEPPER_OFF 0x30
byte current_state = IDLE;  // current state of internal state machine
byte next_step = 0;         // step counter 0 .. 3
#define MOVING_TIMEOUT 1600 // *2 ms, in case limit switches don't work stop and report an error
int timeout_cnt = 0;        // counts up to MOVING_TIMEOUT

//********************************************************************//

void init_limits()
{
  pinMode(LIMIT_OPENED_PIN, INPUT);      // set pin to input
  digitalWrite(LIMIT_OPENED_PIN, HIGH);  // turn on pullup resistors  

  pinMode(LIMIT_CLOSED_PIN, INPUT);      // set pin to input
  digitalWrite(LIMIT_CLOSED_PIN, HIGH);  // turn on pullup resistors  
}

boolean is_opened()
{
  if(digitalRead(LIMIT_OPENED_PIN))
     return false;
     
  return true;
}

boolean is_closed()
{
  if(digitalRead(LIMIT_CLOSED_PIN))
     return false;
     
  return true;
}

//**********//

byte get_ajar_status()
{
  byte b = (digitalRead(AJAR_PIN) == LOW) ? SHUT : AJAR;
  ajar_low_pass_counter = (b == ajar_low_pass_last_value) ? ajar_low_pass_counter+1 : 0;
  ajar_low_pass_last_value = b;
  if(ajar_low_pass_counter >= AJAR_LOW_PASS_TAU) {
    ajar_low_pass_counter = 0;
    return b;
  }
  else
    return ajar_last_state;
}

void init_ajar()
{
  pinMode(AJAR_PIN, INPUT);      // set pin to input
  digitalWrite(AJAR_PIN, HIGH);  // turn on pullup resistors  
  ajar_last_state = get_ajar_status();
}

//**********//

void init_manual()
{
  pinMode(MANUAL_OPEN_PIN, INPUT);      // set pin to input
  digitalWrite(MANUAL_OPEN_PIN, HIGH);  // turn on pullup resistors  

  pinMode(MANUAL_CLOSE_PIN, INPUT);     // set pin to input
  digitalWrite(MANUAL_CLOSE_PIN, HIGH); // turn on pullup resistors  

  debounce_state = DEBOUNCE_IDLE;
  debounce_cnt = DEBOUNCE_DELAY;
}

boolean manual_open_pressed()
{
  if(digitalRead(MANUAL_OPEN_PIN))
     return false;
     
  return true;
}

boolean manual_close_pressed()
{
  if(digitalRead(MANUAL_CLOSE_PIN))
     return false;
     
  return true;
}

void start_debounce_timer()  // this breaks millis() function, but who cares
{
  debounce_cnt = DEBOUNCE_DELAY;

  TCCR0A = 0;         // no prescaler, WGM = 0 (normal)
  TCCR0B = 1<<CS00;   // 
  OCR0A = 255;        // 1+255 = 256 -> 16us @ 16 MHz
  //OCR0A = 255;        // 1+255 = 256 -> 12.8us @ 20 MHz
  TCNT0 = 0;          // reseting timer
  TIMSK0 = 1<<OCF0A;  // enable Interrupt

}

void stop_debounce_timer()
{
  // timer0
  TCCR0B = 0; // no clock source
  TIMSK0 = 0; // disable timer interrupt
}

ISR(TIMER0_COMPA_vect)
{
  if(((debounce_state & DEBOUNCE_OPEN) && manual_open_pressed()) ||
     ((debounce_state & DEBOUNCE_CLOSE) && manual_close_pressed())) {
    if(debounce_cnt) {
      debounce_cnt--;
      return;
    }
    debounce_state |= DEBOUNCE_FINISHED;
  }
  debounce_cnt = DEBOUNCE_DELAY;
}

boolean manual_open()
{
  if(manual_open_pressed()) {
    if(debounce_state & DEBOUNCE_CLOSE) {
      stop_debounce_timer();
      debounce_state = DEBOUNCE_IDLE;
      return false;
    }

    if(debounce_state == DEBOUNCE_IDLE) {
      debounce_state = DEBOUNCE_OPEN;
      start_debounce_timer();
    }
    else if(debounce_state & DEBOUNCE_FINISHED) {
      stop_debounce_timer();
      debounce_state = DEBOUNCE_IDLE;
      return true;
    }
  }
  else if(debounce_state & DEBOUNCE_OPEN) {
    stop_debounce_timer();
    debounce_state = DEBOUNCE_IDLE;
  }

  return false;
}

boolean manual_close()
{
  if(manual_close_pressed()) {
    if(debounce_state & DEBOUNCE_OPEN) {
      stop_debounce_timer();
      debounce_state = DEBOUNCE_IDLE;
      return false;
    }

    if(debounce_state == DEBOUNCE_IDLE) {
      debounce_state = DEBOUNCE_CLOSE;
      start_debounce_timer();
    }
    else if(debounce_state & DEBOUNCE_FINISHED) {
      stop_debounce_timer();
      debounce_state = DEBOUNCE_IDLE;
      return true;
    }
  }
  else if(debounce_state & DEBOUNCE_CLOSE) {
    stop_debounce_timer();
    debounce_state = DEBOUNCE_IDLE;
  }

  return false;
}

//********************************************************************//

void reset_stepper()
{
  next_step = 0;
  PORTB = STEPPER_OFF;
  timeout_cnt = 0;
}

void init_stepper()
{
  DDRB = 0x0F; // set PortB 3:0 as output
  reset_stepper();
}

byte step_table(byte step)
{
  switch(step) { // 0011 xxxx, manual keys pull-ups stay active
  case 0: return 0x33;
  case 1: return 0x36;
  case 2: return 0x3C;
  case 3: return 0x39;
  }
  return STEPPER_OFF;
}

//**********//

void reset_leds()
{
  led_delay_cnt = 0;
  next_led = 0;
  PORTD = LEDS_OFF;
  digitalWrite(LEDS_GREEN_COMMON_PIN, HIGH);
  digitalWrite(LEDS_RED_COMMON_PIN, HIGH);
}

void init_leds()
{
  DDRD = 0xFC;
  pinMode(LEDS_GREEN_COMMON_PIN, OUTPUT);
  pinMode(LEDS_RED_COMMON_PIN, OUTPUT);
  reset_leds();
}

byte led_table(byte led)
{
  switch(led) {  // xxxx xx00, leave RxD and TxD to 0
  case 0: return 0x04;
  case 1: return 0x08;
  case 2: return 0x10;
  case 3: return 0x20;
  case 4: return 0x40;
  case 5: return 0x80; 
  }
  return LEDS_OFF;
}

void leds_green()
{
  digitalWrite(LEDS_GREEN_COMMON_PIN, LOW);
  digitalWrite(LEDS_RED_COMMON_PIN, HIGH);
}

void leds_red()
{
  digitalWrite(LEDS_GREEN_COMMON_PIN, HIGH);
  digitalWrite(LEDS_RED_COMMON_PIN, LOW);
}

void leds_toggle()
{
  if(digitalRead(LEDS_GREEN_COMMON_PIN) == HIGH) {
    digitalWrite(LEDS_GREEN_COMMON_PIN, LOW);
    digitalWrite(LEDS_RED_COMMON_PIN, HIGH);
  }
  else {
    digitalWrite(LEDS_GREEN_COMMON_PIN, HIGH);
    digitalWrite(LEDS_RED_COMMON_PIN, LOW);
  }
}

//**********//

void start_step_timer()
{
  // timer 1: 2 ms, between stepper output state changes
  TCCR1A = 0;                    // prescaler 1:256, WGM = 4 (CTC)
  TCCR1B = 1<<WGM12 | 1<<CS12;   // 
  OCR1A = 124;        // (1+124)*256 = 32000 -> 2 ms @ 16 MHz
  //OCR1A = 155;        // (1+155)*256 = 40000 -> 2 ms @ 20 MHz
  TCNT1 = 0;          // reseting timer
  TIMSK1 = 1<<OCIE1A; // enable Interrupt
}

void start_wait_timer()
{
  // timer1: 250 ms, minimal delay between subsequent open/close
  TCCR1A = 0;         // prescaler 1:256, WGM = 0 (normal)
  TCCR1B = 1<<CS12;   // 
  OCR1A = 15624;      // (1+15624)*256 = 4000000 -> 250 ms @ 16 MHz
  //OCR1A = 19530;      // (1+19530)*256 = 5000000 -> 250 ms @ 20 MHz  
  TCNT1 = 0;          // reseting timer
  TIMSK1 = 1<<OCIE1A; // enable Interrupt
}

void start_error_timer()
{
  // timer1: 500 ms, blinking leds with 1 Hz
  TCCR1A = 0;                  // prescaler 1:256, WGM = 4 (CTC)
  TCCR1B = 1<<WGM12 | 1<<CS12; // 
  OCR1A = 31249;      // (1+31249)*256 = 8000000 -> 500 ms @ 16 MHz
  //OCR1A = 39061;      // (1+39061)*256 = 10000000 -> 500 ms @ 20 MHz
  TCNT1 = 0;          // reseting timer
  TIMSK1 = 1<<OCIE1A; // enable Interrupt
}

void stop_timer() // stop the timer
{
  // timer1
  TCCR1B = 0; // no clock source
  TIMSK1 = 0; // disable timer interrupt
}

ISR(TIMER1_COMPA_vect)
{
      // check if limit switch is active
  if((current_state == OPENING && is_opened()) ||
     (current_state == CLOSING && is_closed())) 
  {
    stop_timer();
    reset_leds();
    if(current_state == OPENING)
      leds_green();
    else
      leds_red();
    PORTD = LEDS_ON;
    current_state = WAIT;
    start_wait_timer();
    return;
  }
      
  if(current_state == OPENING || current_state == CLOSING) {
    timeout_cnt++;
    if(timeout_cnt >= MOVING_TIMEOUT) {
      reset_stepper();
      stop_timer();
      current_state = ERROR;
      Serial.println("Error: open/close took too long!");
      start_error_timer();
      leds_green();
      PORTD = LEDS_ON;
    }
  }

  if(current_state == OPENING) { // next step (open)
    PORTB = step_table(next_step);
    next_step++;
    if(next_step >= 4)
      next_step = 0;
  }
  else if(current_state == CLOSING) { // next step (close)
    PORTB = step_table(next_step);
    if(next_step == 0)
      next_step = 3;
    else
      next_step--;
  }
  else if(current_state == WAIT) { // wait after last open/close finished -> idle
    stop_timer();
    reset_stepper();
    current_state = IDLE;
    Serial.print("Status: ");
    if(is_opened())
      Serial.print("opened");
    else if(is_closed())
      Serial.print("closed");
    Serial.print(", idle");
    if(get_ajar_status() == SHUT)
      Serial.println(", shut");
    else
      Serial.println(", ajar");
    return;
  }
  else if(current_state == ERROR) {
    leds_toggle();
    return;
  }
  else { // timer is useless stop it
    stop_timer();
    return;
  }

  led_delay_cnt++;
  if(led_delay_cnt >= LED_DELAY) {
    led_delay_cnt = 0;    

    PORTD = led_table(next_led);

    if(current_state == OPENING) {
      if(next_led == 0)
        next_led = 5;
      else
        next_led--;
    }
    else if(current_state == CLOSING) {
      next_led++;
      if(next_led >= 6)
        next_led = 0;
    }
  }
}

//********************************************************************//

void reset_heartbeat()
{
  digitalWrite(HEARTBEAT_PIN, HIGH);
  heartbeat_cnt = 0;
}

void heartbeat_on()
{
  digitalWrite(HEARTBEAT_PIN, LOW);
}

void heartbeat_off()
{
  digitalWrite(HEARTBEAT_PIN, HIGH);
}

void init_heartbeat()
{
  pinMode(HEARTBEAT_PIN, OUTPUT);
  reset_heartbeat();
  // timer 2: ~10 ms, timebase for heartbeat signal
  TCCR2A = 1<<WGM21;                    // prescaler 1:1024, WGM = 2 (CTC)
  TCCR2B = 1<<CS22 | 1<<CS21 | 1<<CS20; // 
  OCR2A = 155;        // (1+155)*1024 = 159744 -> ~10 ms @ 16 MHz
  //OCR2A = 194;        // (1+194)*1024 = 199680 -> ~10 ms @ 20 MHz
  TCNT2 = 0;          // reseting timer
  TIMSK2 = 1<<OCIE2A; // enable Interrupt
  heartbeat_on();
}

// while running this gets called every ~10ms
ISR(TIMER2_COMPA_vect)
{
  heartbeat_cnt++;
  if(heartbeat_cnt == HEARTBEAT_DURATION)
    heartbeat_off();
  else if(heartbeat_cnt >= HEARTBEAT_DELAY) {
    heartbeat_on();
    heartbeat_cnt = 0;
  }
}

//********************************************************************//

void reset_after_error()
{
  stop_timer();
  reset_leds();

  leds_red();
  if(is_closed()) {
    current_state = IDLE;
    PORTD = LEDS_ON;
  }
  else {
    current_state = CLOSING;
    start_step_timer();
  }
  Serial.println("Ok, closing now");
}

void start_open()
{
  reset_stepper();
  reset_leds();
  leds_green();
  current_state = OPENING;
  start_step_timer();
}

void start_close()
{
  reset_stepper();
  reset_leds();
  leds_red();
  current_state = CLOSING;
  start_step_timer();
}

void print_status(byte as)
{
  Serial.print("Status: ");
  if(is_opened())
    Serial.print("opened");
  else if(is_closed())
    Serial.print("closed");
  else
    Serial.print("<->");

  switch(current_state) {
  case IDLE: Serial.print(", idle"); break;
  case OPENING: Serial.print(", opening"); break;
  case CLOSING: Serial.print(", closing"); break;
  case WAIT: Serial.print(", waiting"); break;
  default: Serial.print(", <undefined state>"); break;
  }
  if(as == SHUT)
    Serial.println(", shut");
  else
    Serial.println(", ajar");
}

//**********//

void setup()
{
  init_limits();
  init_ajar();
  init_stepper();
  init_leds();
  init_heartbeat();

  Serial.begin(9600);

  current_state = IDLE;

      // make sure door is locked after reset
  leds_red();
  if(is_closed())
    PORTD = LEDS_ON;
  else {
    current_state = CLOSING;
    start_step_timer();
  }
  Serial.println("init complete");
}

void loop()
{
  if(Serial.available()) {
    char command = Serial.read();

    if(current_state == ERROR && command != CMD_RESET) {
      Serial.println("Error: last open/close operation took too long!");
    }
    else if (command == CMD_RESET) {
      reset_after_error();
    }
    else if (command == CMD_OPEN) {
      if(current_state == IDLE) {
        if(is_opened())
          Serial.println("Already open");
        else {
          start_open();
          Serial.println("Ok");
        }
      }
      else
        Serial.println("Error: Operation in progress");
    }
    else if (command == CMD_CLOSE) {
      if(current_state == IDLE) {
        if(is_closed())
          Serial.println("Already closed");
        else {
          start_close();
          Serial.println("Ok");
        }
      }
      else
        Serial.println("Error: Operation in progress");
    }
    else if (command == CMD_TOGGLE) {
      if(current_state == IDLE) {
        if(is_closed())
          start_open();
        else
          start_close();
        Serial.println("Ok");
      }
      else
        Serial.println("Error: Operation in progress");
    }
    else if (command == CMD_STATUS)
      print_status(get_ajar_status());
    else
      Serial.println("Error: unknown command");
  }
  if(manual_open() && !is_opened() && (current_state == IDLE || current_state == ERROR)) {
    Serial.println("open forced manually");
    start_open();
  }
  if(manual_close() && !is_closed() && (current_state == IDLE || current_state == ERROR)) {
    Serial.println("close forced manually");
    start_close();
  }
  if(current_state == IDLE) {
    if(is_opened()) {
      leds_green();
      PORTD = LEDS_ON;
    }
    if(is_closed()) {
      leds_red();
      PORTD = LEDS_ON;
    }
  }
  byte a = get_ajar_status();
  if(a != ajar_last_state) {
    print_status(a);
    ajar_last_state = a;
  }
}
