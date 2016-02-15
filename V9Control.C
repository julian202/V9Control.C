#define AINBUFSIZE 31
#define AOUTBUFSIZE 31
#define BINBUFSIZE 31
#define BOUTBUFSIZE 31
#define CINBUFSIZE 31
#define COUTBUFSIZE 31
#define DINBUFSIZE 31
#define DOUTBUFSIZE 31
// define name
#define Name "Testing"
#use packet.lib

//declare global variables
char motor_valve_status[4];
int bad_read;
int current_valve_status[24];
long last_fired_time[24];
long readings[32];
unsigned int current_analog_out[4];
int limits;
char message[6];
char crc[2];
int m,n,x;
unsigned long crc_1;
unsigned long crc_0;
char string[6];
char weight[20];
int MV_position_checked[4];
int numGaugeReads;
int numGaugeIgnores;
// send the value of 'character'(0-255) out serial port B
void output_character(int character)
{
   if (character>255) return;
   if (character==10)
   {
      serBputc(10);
      serBputc(13);
   }
   else
   {
      serBputc(character);
      serBputc(13);
   }
}
// send the character srting of the value of 'string'
cofunc void output_string(long string1)
//string = 0-65535
{
   char out_string[16];
   ltoa(string1, out_string);
   while (serBputs(out_string)<strlen(out_string));
   DelayMs(10);
   while (serBputc(13)==0);
   return;
}
// Recieve next character in the buffer if there is one. If there is nothing in
// the buffer it will wait the number of ticks in 'wait' for something to come
// in. If wait equals 0 it will wait forever but yield for other tasks. if
// nothing comes in during this time it will return -1. If echo equals 1 it
// will sent the character back out the commport as an echo.
cofunc int input_character(long wait, int echo)
{
   int character;
   character = -1;
   if (wait==0)
   {
      do
      {
         character=serBgetc();
         if (character<0) yield;
      }
      while (character<0);
   }
   else if (wait>=1)
   {
      wait+=TICK_TIMER;
      do
      {
         character=serBgetc();
         if (character<0) yield;
      }
      while ((character<0)&&(TICK_TIMER<wait));
   }
   if (echo==1)
   {
      output_character(character);
   }
   return character;
}
cofunc int input_character_C(long wait)
{
   int character;
   character = -1;
   if (wait==0)
   {
      do
      {
         character=serCgetc();
         if (character<0) yield;
      }
      while (character<0);
   }
   else if (wait>=1)
   {
      wait+=TICK_TIMER;
      do
      {
         character=serCgetc();
         if (character<0) yield;
      }
      while ((character<0)&&(TICK_TIMER<wait));
   }

   return character;
}
cofunc int input_character_A(long wait)
{
   int character;
   character = -1;
   if (wait==0)
   {
      do
      {
         character=serAgetc();
         if (character<0) yield;
      }
      while (character<0);
   }
   else if (wait>=1)
   {
      wait+=TICK_TIMER;
      do
      {
         character=serAgetc();
         if (character<0) yield;
      }
      while ((character<0)&&(TICK_TIMER<wait));
   }
   return character;
}
cofunc int input_character_D(long wait)
{
   int character;
   character = -1;
   if (wait==0)
   {
      do
      {
         character=serDgetc();
         if (character<0) yield;
      }
      while (character<0);
   }
   else if (wait>=1)
   {
      wait+=TICK_TIMER;
      do
      {
         character=serDgetc();
         if (character<0) yield;
      }
      while ((character<0)&&(TICK_TIMER<wait));
   }
   return character;
}
cofunc long input_string(int length, long wait, int echo)
//length is the number of characters to recieve.
//wait is the number of milliseconds to wait between characters
//if echo is 1 then input will be echoed back out
{
   char string_in[16];
   string_in[0] = 0;
   string_in[1] = 0;
   string_in[2] = 0;
   string_in[3] = 0;
   while (serBread(string_in,length,wait)<length);
   if (echo==1)
   {
      wfd output_string(atol(string_in));
   }
   return atol(string_in);

}
cofunc void move_valve(int valve, int pause, int latch, int direction)
// Valve=0-23, direction='O'(79) or 'C'(67)
{
   unsigned int enable;

   WrPortI(PBDR, &PBDRShadow, (PBDRShadow | 224));// make sure no other valves are currently on
   enable=(valve & 7) << 2; // prepare lower 3 bits for proper place in PB
   WrPortI(PBDR, &PBDRShadow, (PBDRShadow & 227) | enable); // set main address

   if (valve<=7)
   {
      enable=5; // PB5 is the enable port for lower 8 valves
   }
   else if (valve<=15)
   {
      enable=6; // PB6 is the enable port for middle 8 valves
   }
   else
   {
      enable=7; // PB7 is the enable port for upper 8 valves
   }
   if (direction == 'O' || direction == 'o')
   {
      BitWrPortI(PBDR, &PBDRShadow, 0, 0); // DIR (PB0) low
      BitWrPortI(PBDR, &PBDRShadow, 0, enable); // lower enable bit
      if (latch == 0 && direction == 'O')
      {
      	waitfor(DelayMs(pause));
      	BitWrPortI(PBDR, &PBDRShadow, 1, enable); // raise enable bit
      }
   }
   else if (direction == 'C' || direction == 'c')
   {
      BitWrPortI(PBDR, &PBDRShadow, 1, 0); // DIR (PB0) low
      BitWrPortI(PBDR, &PBDRShadow, 0, enable); // lower enable bit
      if (latch==0 && direction == 'C')
      {
         waitfor(DelayMs(pause)); // wait jiffies
         BitWrPortI(PBDR, &PBDRShadow, 1, enable); // raise enable bit
      }
   }
   current_valve_status[valve]=direction;
}
cofunc void pulse_valve(int valve, int pause)
{
	// Keep track of the original position
	char origPos;
   origPos = current_valve_status[valve];
   // If originally opened, close
   if (origPos == 'O' || origPos == 'o')
   	wfd move_valve(valve, pause, 0, 'C');
   // Open
   wfd move_valve(valve, pause, 0, 'O');
   // If originally closed, close
   if (origPos == 'C' || origPos == 'c')
   	wfd move_valve(valve, pause, 0, 'C');
}
void move_motor_valve(int valve, char direction)
// valve=0..3, direction='O', 'C', or 'S'
{
   unsigned int enable;

   // set enable to bit number of valve = 4, 5, 6, or 7
   enable=4+valve;
   if (direction=='O' || direction == 'F')
   {
      BitWrPortI(PEDR, &PEDRShadow, 1, enable); // DIR (PEx) high
   }
   else if (direction=='C' || direction == 'B')
   {
      BitWrPortI(PEDR, &PEDRShadow, 0, enable); // DIR (PEx) low
   }
   if (direction=='S')
   {
      BitWrPortI(PFDR, &PFDRShadow, 0, enable); // lower PFx (disable valve movement)
   }
   else
   {
      BitWrPortI(PFDR, &PFDRShadow, 1, enable); // raise PFx (enable valve movement)
   }
   motor_valve_status[valve]=direction;
}
unsigned int read_channel(unsigned int channel)
// i=0..31
{
   unsigned int low_byte, high_byte, reading;
   int read_status;
   unsigned long time;
   if (bad_read!=0) return (0);
   BitWrPortI(PFDR, &PFDRShadow, 1, 3); // raise PF3 to disable ADC - should already be high
   WrPortI(PGDR, &PGDRShadow, channel+64); // set channel and disable ADC output
   BitWrPortI(PFDR, &PFDRShadow, 0, 3); // lower PF3 to start ADC
   time=TICK_TIMER+2;
   while (((read_status=BitRdPortI(PFDR,2))==0) && TICK_TIMER<time);
   if (read_status==0)
   {
      bad_read+=1;
      return (0);
   }
   BitWrPortI(PFDR, &PFDRShadow, 1, 3); // raise PF3 so it stops on next EOC
   // wait for read_status to go low
   while (((read_status=BitRdPortI(PFDR,2))==1) && TICK_TIMER<time);
   if (read_status==1)
   {
      bad_read+=1;
      return (0);
   }
   BitWrPortI(PGDR, &PGDRShadow, 0, 6); // lower PG6 to enable ADC output
   low_byte=RdPortI(PADR); // low byte
   BitWrPortI(PGDR, &PGDRShadow, 1, 5); // raise PG5 to swap bytes
   high_byte=RdPortI(PADR); // high byte
   BitWrPortI(PGDR, &PGDRShadow, 0, 5); // lower PG5 again
   BitWrPortI(PGDR, &PGDRShadow, 1, 6); // raise PG6 again
   reading=low_byte+(high_byte << 8);
   return (reading);
}
cofunc long get_reading(int channel, int average, int ignore)
// call read_channel(i) multiple times and average the results
{
   unsigned long reading;
   unsigned int x;
   unsigned int next_reading;
   int times;
   unsigned long time;
   time=TICK_TIMER+10;
   bad_read=0;
   if (ignore>0)
   {
      for (times=0; times<ignore; times++)
      {
         x=read_channel(channel);
         if (TICK_TIMER>=time)
         {
            yield;
            time=TICK_TIMER+10;
         }
      }
   }
   reading=0;
   for (times=1; times<=average; times++)
   {
      next_reading=read_channel(channel);
      reading+=next_reading;
      if (TICK_TIMER>=time)
         {
            yield;
            time=TICK_TIMER+10;
         }
   }
   reading/=(average-bad_read);
   if (reading>65535) reading=65535;
   if (reading<0) reading=0;
   return ((long)reading);
}
cofunc void change_analog_out(int channel, int value)
{
   int times, enable, change;
   change=0;
   // make sure no other valves are currently on
   WrPortI(PBDR, &PBDRShadow, PBDRShadow | 224);
   if (current_analog_out[channel-1]<value)
   {
      BitWrPortI(PBDR, &PBDRShadow, 1, 0); // raise PB0 to signify UP direction
      change=value-current_analog_out[channel-1];
   }
   else if (current_analog_out[channel-1]>value)
   {
      BitWrPortI(PBDR, &PBDRShadow, 0, 0); // lower PB0 to signify DOWN direction
      change=current_analog_out[channel-1]-value;
   }
   enable=((channel-1) << 3); // x=0, 8, 16, 24
   WrPortI(PBDR, &PBDRShadow, (PBDRShadow & 0xe3) | enable); // set address of stepper line
   for (times=0; times<change; times++)
   {
      BitWrPortI(PFDR, &PFDRShadow, 0, 0); // lower PF0
      BitWrPortI(PFDR, &PFDRShadow, 1, 0); // raise PF0

   }
   current_analog_out[channel-1]=value;
   return;
}
cofunc void zero_analog_out(int channel)
{
   unsigned int enable;
   enable=((channel-1) << 1) + 1; // x=1, 3, 5, or 7
   enable<<=2; // x=4, 12, 20, or 28
   // make sure no other valves are currently on
   WrPortI(PBDR, &PBDRShadow, PBDRShadow | (224));
   WrPortI(PBDR, &PBDRShadow, (PBDRShadow & 227) | enable); // set address of clearer line
   BitWrPortI(PFDR, &PFDRShadow, 0, 0); // lower PF0
   BitWrPortI(PFDR, &PFDRShadow, 1, 0); // raise PF0
   current_analog_out[channel-1]=0-20;
   wfd change_analog_out(channel,20);
   current_analog_out[channel-1]=0;

}
cofunc int get_gen_limits(int gen)
{
   int x;
   int limit;
   long time;
   time = TICK_TIMER+1000;
   if(gen==0)
   {
      serCrdFlush();
      serCputc('L');
      wfd limit = input_character_C(1000);
   }
   else if(gen==1)
   {
      serArdFlush();
      serAputc('L');
      wfd limit = input_character_A(1000);
   }
   else if(gen==2)
   {
      serDrdFlush();
      serDputc('L');
      wfd limit = input_character_D(1000);
   }
   else if(gen==3)
   {
      serCrdFlush();
      serCputc('>');
      serCputc('L');
      wfd limit = input_character_C(1000);
   }
   else if(gen==4)
   {
      serCrdFlush();
      serCputc('}');
      serCputc('L');
      wfd limit = input_character_C(1000);
   }
   else if(gen==5)
   {
      serCrdFlush();
      serCputc(']');
      serCputc('L');
      wfd limit = input_character_C(1000);
   }
   if(limit=='0'||limit=='U'||limit=='D')
   {
      return limit;
   }
   else
   {
      return 'E';
   }
}
cofunc void start_gen(int generator,int channel,long gen_speed)
{
   int Low_byte,High_byte;
   Low_byte = (int)(gen_speed&255);
   High_byte = (int)(gen_speed>>8);
   if(generator==0)
   {
      serCrdFlush();
      serCputc(channel);
      serCputc(Low_byte);
      serCputc(High_byte);
      DelayMs(10);
      serCrdFlush();
   }
   else if(generator==1)
   {
      serArdFlush();
      serAputc(channel);
      serAputc(Low_byte);
      serAputc(High_byte);
      DelayMs(10);
      serArdFlush();
   }
   else if(generator==2)
   {
      serDrdFlush();
      serDputc(channel);
      serDputc(Low_byte);
      serDputc(High_byte);
      DelayMs(10);
      serDrdFlush();
   }
   else if(generator==3)
   {
      serCrdFlush();
      serCputc('>');
      serCputc(channel);
      serCputc(Low_byte);
      serCputc(High_byte);
      DelayMs(10);
      serCrdFlush();
   }
   else if(generator==4)
   {
      serCrdFlush();
      serCputc('}');
      serCputc(channel);
      serCputc(Low_byte);
      serCputc(High_byte);
      DelayMs(10);
      serCrdFlush();
   }
   else if(generator==5)
   {
      serCrdFlush();
      serCputc(']');
      serCputc(channel);
      serCputc(Low_byte);
      serCputc(High_byte);
      DelayMs(10);
      serCrdFlush();
   }
}
cofunc void stop_gen(int generator)
{
   if(generator==0)
   {
      serCputc('S');
      DelayMs(10);
      serCrdFlush();
   }
   else if(generator==1)
   {
      serAputc('S');
      DelayMs(10);
      serArdFlush();
   }
   else if(generator==2)
   {
      serDputc('S');
      DelayMs(10);
      serDrdFlush();
   }
   else if(generator==3)
   {
      serCputc('>');
      serCputc('S');
      DelayMs(10);
      serCrdFlush();

   }
   else if(generator==4)
   {
      serCputc('}');
      serCputc('S');
      DelayMs(10);
      serCrdFlush();
   }
   else if(generator==5)
   {
      serCputc(']');
      serCputc('S');
      DelayMs(10);
      serCrdFlush();
   }
}
void getMBCRC(char *buf, int bufLen, char *crc) { // Function name and parameter list returning a void
   // *buf pointer to character array used to calculate CRC
   // bufLen number of characters to calculate CRC for
   // *crc pointer to the array that contains the calculated CRC
   crc_0 = 0xffff; // Declare and initialize variables
   crc_1 = 0x0000; // Declare and initialize variables
    // Declare and initialize variables
   for (m=0; m<bufLen; m++)
   { // Loop through characters of input array
      crc_0 ^= ((unsigned long)buf[m] & 0x00ff); // XOR current character with 0x00ff
      for (n=0;n<8;n++)
      { // Loop through characters bits
         crc_1 = (crc_0 >> 1) & 0x7fff; // shift result right one place and store
         if (crc_0 & 0x0001)
         { // if pre-shifted value bit 0 is set
            crc_0 = (crc_1 ^ 0xa001); // XOR the shifted value with 0xa001
         }
         else
         { // if pre-shifted value bit 0 is not set
         crc_0 = crc_1;
         } // set the pre-shifted value equal to the shifted value
      } // End for loop - Loop through characters bits
   } // End for loop - Loop through characters of input array
   crc[1] = (unsigned char)((crc_0/256) & 0x00ff); // Hi byte
   crc[0] = (unsigned char)(crc_0 & 0x00ff); // Lo byte
   return; // Return to calling function
} // End of CRC calculation function
cofunc unsigned int Cgetchar1sec()
{
   int i;
   unsigned long t0;
   t0=TICK_TIMER;
   // return next character - wait no longer than 1 second until one comes in
   while (TICK_TIMER-t0<1024)
   {
      i=serDgetc();
      if(i>=0)
      {
         return i;

      }
      yield; // allow another task to take over while we wait
   }
   return -1;
}
cofunc long get_temp(int channel)
{
   int i,fail;
   long temp1;
   channel -= '0';
   if(channel < 5){
	   while(serArdUsed()>0)
	   {
	      wfd x=input_character_A(1000);
	   }
	   message[0]=(char)(channel);
	   message[1]=(char)3;
	   message[2]=(char)3;
	   message[3]=(char)232;
	   message[4]=(char)0;
	   message[5]=(char)1;
	   getMBCRC(message, 6, crc);
	   x=0;
	   serAputc(message[0]);
	   DelayMs(10);
	   serAputc(message[1]);
   	DelayMs(10);
	   serAputc(message[2]);
	   DelayMs(10);
	   serAputc(message[3]);
	   DelayMs(10);
	   serAputc(message[4]);
	   DelayMs(10);
	   serAputc(message[5]);
	   DelayMs(10);
	   serAputc(crc[0]);
	   DelayMs(10);
	   serAputc(crc[1]);
	   for(i=1;i<6;i++)
	   {
	      wfd x=input_character_A(1000);
	      if (x<0)
	      {
	         temp1='E';
	         break;
	      }
	      if (i==4)
	      {
	         //serBputc(x+32);
	         temp1 = x*256;
	      }
	      else if (i==5)
	      {
	         temp1 += x;
         }
	   }
	   return temp1;
	   serArdFlush();
   }
   else
   {
      while(serDrdUsed()>0)
   	{
	      wfd x=input_character_D(1000);
	   }
	   message[0]=(char)(channel-4);
	   message[1]=(char)3;
	   message[2]=(char)3;
	   message[3]=(char)232;
	   message[4]=(char)0;
	   message[5]=(char)1;
	   getMBCRC(message, 6, crc);
	   x=0;
	   serDputc(message[0]);
	   DelayMs(10);
	   serDputc(message[1]);
	   DelayMs(10);
	   serDputc(message[2]);
	   DelayMs(10);
	   serDputc(message[3]);
	   DelayMs(10);
	   serDputc(message[4]);
	   DelayMs(10);
	   serDputc(message[5]);
	   DelayMs(10);
	   serDputc(crc[0]);
	   DelayMs(10);
	   serDputc(crc[1]);
	   for(i=1;i<6;i++)
	   {
	      wfd x=input_character_D(1000);
	      if (x<0)
	      {
	         temp1='E';
	         break;
	      }
	      if (i==4)
	      {
   	      temp1 = x*256;
	      }
	      else if (i==5)
   	   {
	         temp1 += x;
	      }
	   }
	   return temp1;
	   serDrdFlush();
	}
}
cofunc void set_temp(int channel,long setpoint)
{
   int Low_byte,High_byte;
   Low_byte = (int)(setpoint&255);
   High_byte = (int)(setpoint>>8);
   channel -= '0';
   if(channel < 5){
	   message[0]=(channel);
	   message[1]=6;
	   message[2]=3;
	   message[3]=234;
	   message[4]=High_byte;
	   message[5]=Low_byte;
	   getMBCRC(message, 6, crc);
	   serAputc(message[0]);
	   DelayMs(10);
	   serAputc(message[1]);
	   DelayMs(10);
	   serAputc(message[2]);
	   DelayMs(10);
	   serAputc(message[3]);
	   DelayMs(10);
	   serAputc(message[4]);
	   DelayMs(10);
	   serAputc(message[5]);
	   DelayMs(10);
	   serAputc(crc[0]);
	   DelayMs(10);
	   serAputc(crc[1]);
   }
	else
	{
	   message[0]=(channel - 4);
	   message[1]=6;
	   message[2]=3;
	   message[3]=234;
	   message[4]=High_byte;
	   message[5]=Low_byte;
	   getMBCRC(message, 6, crc);
	   serDputc(message[0]);
	   DelayMs(10);
	   serDputc(message[1]);
	   DelayMs(10);
	   serDputc(message[2]);
	   DelayMs(10);
	   serDputc(message[3]);
	   DelayMs(10);
	   serDputc(message[4]);
	   DelayMs(10);
	   serDputc(message[5]);
	   DelayMs(10);
	   serDputc(crc[0]);
	   DelayMs(10);
	   serDputc(crc[1]);
   }
}
cofunc void set_standby(int channel)
{
	channel -= '0';
	if(channel < 5)
   {
	   message[0]=(channel);
	   message[1]=6;
	   message[2]=15;
	   message[3]=164;
	   message[4]=0;
	   message[5]=2;
	   getMBCRC(message, 6, crc);
	   serAputc(message[0]);
	   DelayMs(10);
	   serAputc(message[1]);
	   DelayMs(10);
	   serAputc(message[2]);
	   DelayMs(10);
	   serAputc(message[3]);
	   DelayMs(10);
	   serAputc(message[4]);
	   DelayMs(10);
	   serAputc(message[5]);
	   DelayMs(10);
	   serAputc(crc[0]);
	   DelayMs(10);
	   serAputc(crc[1]);
   }
   else
   {
	   message[0]=(channel-4);
	   message[1]=6;
	   message[2]=15;
	   message[3]=164;
	   message[4]=0;
	   message[5]=2;
	   getMBCRC(message, 6, crc);
	   serDputc(message[0]);
	   DelayMs(10);
	   serDputc(message[1]);
	   DelayMs(10);
	   serDputc(message[2]);
	   DelayMs(10);
	   serDputc(message[3]);
	   DelayMs(10);
	   serDputc(message[4]);
	   DelayMs(10);
	   serDputc(message[5]);
	   DelayMs(10);
	   serDputc(crc[0]);
	   DelayMs(10);
	   serDputc(crc[1]);
   }
}
cofunc void set_normal(int channel)
{
   channel -= '0';
	if(channel < 5)
   {
	   message[0]=(channel);
	   message[1]=6;
	   message[2]=15;
	   message[3]=164;
	   message[4]=0;
	   message[5]=3;
	   getMBCRC(message, 6, crc);
	   serAputc(message[0]);
	   DelayMs(10);
	   serAputc(message[1]);
	   DelayMs(10);
	   serAputc(message[2]);
	   DelayMs(10);
	   serAputc(message[3]);
	   DelayMs(10);
	   serAputc(message[4]);
	   DelayMs(10);
	   serAputc(message[5]);
	   DelayMs(10);
	   serAputc(crc[0]);
	   DelayMs(10);
	   serAputc(crc[1]);
   }
   else
   {
	   message[0]=(channel-4);
	   message[1]=6;
	   message[2]=15;
	   message[3]=164;
	   message[4]=0;
	   message[5]=3;
	   getMBCRC(message, 6, crc);
	   serDputc(message[0]);
	   DelayMs(10);
	   serDputc(message[1]);
	   DelayMs(10);
	   serDputc(message[2]);
	   DelayMs(10);
	   serDputc(message[3]);
	   DelayMs(10);
	   serDputc(message[4]);
	   DelayMs(10);
	   serDputc(message[5]);
	   DelayMs(10);
	   serDputc(crc[0]);
	   DelayMs(10);
	   serDputc(crc[1]);
   }
}
cofunc void get_weight(int scale)
{
   int c;
   long start_time;
   start_time = TICK_TIMER+500;
   if(scale==0)
   {
      serCopen(9600);
      serCputs("PWR 1");
      serCputc(13);
      serCputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serCgetc();
      }
      if(start_time<TICK_TIMER)
      {
         serCrdFlush();
         serCopen(9600);
         return;
      }
      DelayMs(10);
      serCputs("SI");
      serCputc(13);
      serCputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serCgetc();
         if(c<=57 && c>=45)
         {
            serBputc(c);
         }
      }
      if(c==10 || c==0){
         serBputc(13);
      }
      serCrdFlush();
      serCopen(9600);
   }
   else if(scale==1)
   {
      serAopen(9600);
      serAputs("PWR 1");
      serAputc(13);
      serAputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serAgetc();
      }
      if(start_time<TICK_TIMER)
      {
         serArdFlush();
         serAopen(9600);
         return;
      }
      DelayMs(10);
      serAputs("SI");
      serAputc(13);
      serAputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serAgetc();
         if(c<=57 && c>=45)
         {
            serBputc(c);
         }
      }
      if(c==10 || c==0){
         serBputc(13);
      }
      serArdFlush();
      serAopen(9600);
   }
   else if(scale==2)
   {
      serDopen(9600);
      serDputs("PWR 1");
      serDputc(13);
      serDputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serDgetc();
      }
      if(start_time<TICK_TIMER)
      {
         serDrdFlush();
         serDopen(9600);
         return;
      }
      DelayMs(10);
      serDputs("SI");
      serDputc(13);
      serDputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serDgetc();
         if(c<=57 && c>=45)
         {
            serBputc(c);
         }
      }
      if(c==10 || c==0){
         serBputc(13);
      }
      serArdFlush();
      serDopen(9600);
   }
}
cofunc void zero_scale(int scale)
{
   int c;
   long start_time;
   start_time = TICK_TIMER+1000;
   if(scale==0)
   {
   	serCclose();
      serCopen(9600);
      serCputs("PWR 1");
      serCputc(13);
      serCputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serCgetc();
      }
      if(start_time<TICK_TIMER)
      {
         serCrdFlush();
         serCopen(9600);
         return;
      }
      DelayMs(10);
      serCputs("Z");
      serCputc(13);
      serCputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serCgetc();
      }
      if(c==10 || c==0){
         serBputc(13);
      }
      serCrdFlush();
      serCopen(9600);
   }
   else if(scale==1)
   {
   	serAclose();
      serAopen(9600);
      serAputs("PWR 1");
      serAputc(13);
      serAputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serAgetc();
      }
      if(start_time<TICK_TIMER)
      {
         serArdFlush();
         serAopen(9600);
         return;
      }
      DelayMs(10);
      serAputs("Z");
      serAputc(13);
      serAputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serAgetc();
      }
      if(c==10 || c==0){
         serBputc(13);
      }
      serArdFlush();
      serAopen(9600);
   }
   else if(scale==2)
   {
   	serDclose();
      serDopen(9600);
      serDputs("PWR 1");
      serDputc(13);
      serDputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serDgetc();
      }
      if(start_time<TICK_TIMER)
      {
         serDrdFlush();
         serDopen(9600);
         return;
      }
      DelayMs(10);
      serDputs("Z");
      serDputc(13);
      serDputc(10);
      c=0;
      while(c!=10 && start_time>TICK_TIMER)
      {
         c=serDgetc();
      }
      if(c==10 || c==0){
         serBputc(13);
      }
      serDrdFlush();
      serDopen(9600);
   }
}
cofunc void start_stepper(int direction,int channel,long value)
{
   serAopen(38400);
   serAputc('@');
   serAputc(channel-1);
   serAputc('.');
   serAputc(13);
   DelayMs(10);
   if(direction=='F')
   {
      serAputc('@');
      serAputc(channel-1);
      serAputc('+');
      serAputc(13);
   }
   else if(direction=='B')
   {
      serAputc('@');
      serAputc(channel-1);
      serAputc('-');
      serAputc(13);
   }
   ltoa(value,string);
   serAputc('@');
   serAputc(channel-1);
   serAputc('M');
   serAputs(string);
   serAputc(13);
   serAputc('@');
   serAputc(channel-1);
   serAputc('S');
   serAputc(13);
}
cofunc void stop_stepper(int hard_soft,int channel)
{
   serAopen(38400);
   if(hard_soft=='H')
   {
      serAputc('@');
      serAputc(channel-1);
      serAputc('.');
      serAputc(13);
   }
   else if(hard_soft=='S')
   {
      serAputc('@');
      serAputc(channel-1);
      serAputc(',');
      serAputc(13);
   }
}
cofunc char stepper_limit(int channel)
{
   int dir;
   int limit;
   serAopen(38400);
   serArdFlush();
   serAputc('@');
   serAputc(channel-1);
   serAputs("V+");
   serAputc(13);
   wfd dir = input_character_A(500);
   serArdFlush();
   serAputc('@');
   serAputc(channel-1);
   serAputs("VL");
   serAputc(13);
   wfd limit = input_character_A(500);

   if(limit == '1' && dir == '1')
   	return 'U';
   else if(limit == '1' && dir == '0')
   	return 'D';
   else
   	return '0';
}
int serXgetc(char port)
{
	switch (port)
   {
   	case 'A': return serAgetc();
      case 'B': return serBgetc();
      case 'C': return serCgetc();
      case 'D': return serDgetc();
      default: return -1;
   }
}
void serXputc(char port, char c)
{
	switch (port)
   {
   	case 'A': serAputc(c); break;
      case 'B': serBputc(c); break;
      case 'C': serCputc(c); break;
      case 'D': serDputc(c); break;
   }
}
// Reads the specified number of characters from serial B and relays them
// to the specified serial port.
// If readNotWrite == 1, the process is reversed:
// the characters are read from the specified serial port and relayed to
// serial B.
//  - Mix
cofunc int PassNumChars(char port, int numChars, int readNotWrite)
{
	// Initialize values
	int i, c;
   long time;
   c = -1;
   time = TICK_TIMER;
   // Pass each character
   for (i = 0; i < numChars; i++)
   {
      // Timeout, report failure
      if (TICK_TIMER - time >= 1500) return 0;
      // Read from port
      if (readNotWrite == 1) c = serXgetc(port);
      // Read from COM
      else c = serBgetc();
      // Try again for invalid character
      if (c < 0) { i--; continue; }
      // Pass the character through to COM
      if (readNotWrite == 1) serBputc(c);
      // Pass the character through to port
      else serXputc(port, c);
      // Give some time
      //DelayMs(10);
   }
   // Success
   return 1;
}
// Reads characters from serial B until the stop character and relays them
// to the specified serial port.
// If readNotWrite == 1, the process is reversed:
// the characters are read from the specified serial port and relayed to
// serial B.
// If includeStop == 1, the stop character is also relayed.
//  - Mix
cofunc int PassToChar(char port, char stopChar, int readNotWrite, int includeStop)
{
	// Initialize values
   int c;
   long time;
   c = -1;
   time = TICK_TIMER;
   // Pass each character
   while (c != stopChar)
   {
      // Timeout, report failure
      if (TICK_TIMER - time >= 1500) return 0;
      // Read from port
      if (readNotWrite == 1) c = serXgetc(port);
      // Read from COM
      else c = serBgetc();
      // Try again for invalid character
      if (c < 0) continue;
      if (includeStop != 0 || c != stopChar)
      {
	      // Pass the character through to COM
	      if (readNotWrite == 1) serBputc(c);
	      // Pass the character through to port
	      else serXputc(port, c);
      }
      // Give some time
      //DelayMs(10);
   }
   // Success
   return 1;
}
main()
{//open main
   //declare main's variables
   int character;
   int command;
   int valve;
   int channel;
   int reads;
   int read_write;
   int first_time;
   int generator;
   int gen;
   unsigned int i;
   unsigned int valves;
   long position;
   long valve_limit[4];
   char digits[3];
   long value;
   long gen_speed;
   long temp;
   long new_temp;
   long newer_temp;
   long passTemp;

   long motor_speed;
   char lim_ret;
   char nextCommand;
   long fireTimer;

   long start_time;

   char origPos;

   int numChars, port, stopChar, success, numCharsR, stopCharR;
   int j2, j;
   unsigned int k1, k2;
   nextCommand = 'O';
   fireTimer = TICK_TIMER;
   numGaugeReads = 50;
   numGaugeIgnores = 10;

   //setup rabbit I/O's
   WrPortI(SPCR, &SPCRShadow, 128);    // set PA to input
   WrPortI(PBDDR, &PBDDRShadow, 255);  // PB all out
   WrPortI(PBDR, &PBDRShadow, 224);    // disable all valve enables
   WrPortI(PEFR, &PEFRShadow, 0);      // PE to I/O
   WrPortI(PEDDR, &PEDDRShadow, 255);  // PE all out
   WrPortI(PECR, &PECRShadow, 0);      // Fast output
   WrPortI(PEDR, &PEDRShadow, 1);      // all bits initialize to low except PE0 high to disable digital input
   WrPortI(PFFR, &PFFRShadow, 0);      // PF to I/O
   WrPortI(PFDDR, &PFDDRShadow, 251);  // PF all out except for PF2
   WrPortI(PFCR, &PFCRShadow, 0);      // Fast output
   WrPortI(PFDCR, &PFDCRShadow, 0);    // High/Low output (no open drain)
   WrPortI(PFDR, &PFDRShadow, 9);      // low to disable all motor valves, +8 to disable ADC, +1 to disable AOut enable
   WrPortI(PGFR, &PGFRShadow, 0);      // PG to I/O
   WrPortI(PGDDR, &PGDDRShadow, 127);  // PG all out except for PG7, which is input
   WrPortI(PGCR, &PGCRShadow, 0);      // Fast output
   WrPortI(PGDCR, &PGDCRShadow, 0);    // High/Low output (no open drain)
   WrPortI(PGDR, &PGDRShadow, 64);     // disable ADC
   //open main communication
   serBopen(115200);
   serBdatabits(PARAM_8BIT);
   serBflowcontrolOff();
   serBparity(PARAM_NOPARITY);
   serCopen(9600);
   serCdatabits(PARAM_8BIT);
   serCflowcontrolOff();
   serCparity(PARAM_NOPARITY);
   serAopen(9600);
   serAdatabits(PARAM_8BIT);
   serAflowcontrolOff();
   serAparity(PARAM_NOPARITY);
   serDopen(57600);
   serDdatabits(PARAM_8BIT);
   serDflowcontrolOff();
   serDparity(PARAM_NOPARITY);
   //set current analog output values(1-4) to 0
   // turn on the 24 volt supply
   BitWrPortI(PEDR, &PEDRShadow, 0, 1); // PE1 low
   BitWrPortI(PEDR, &PEDRShadow, 1, 3); // PE3 high
   first_time=1;
   gen=0;

   for(;;)
   {//open for 1
      costate
      {//open costate 1
         // close all valves
         if (first_time==1)
         {
            DelayMs(1250);
            wfd zero_analog_out(1);
            wfd zero_analog_out(2);
            wfd zero_analog_out(3);
            wfd zero_analog_out(4);
            for (valves=0; valves<=23; valves++)
            {
               wfd move_valve(valves, 200, 0, 'C');
               last_fired_time[valves] = 0;
            }
            for (valves=0; valves <= 3; valves++)
            {
            	MV_position_checked[valves] = 1;
            }
            first_time=0;
         }
         wfd command=input_character(0,1);
         switch(command)
         {// open switch 1
            //move valve open
            case 'O':
            case 'o':
            //move valve closed
            case 'C':
            case 'c':
            //stop motor valve
            case 'S':
            //motor valve speed forward
            case 'F':
            //motor valve speed backward
            case 'B':
            case 'D':
               //get valve number
               wfd valve=input_character(1000,0);
               //if none sent break
               if (valve<0) break;
               //if valve 'A'-'X'(0-23)
               if ((valve>='A') && (valve<='X'))
               {//open if 1
                  // if the command was to stop break
                  if (command=='S')break;
                  if (command=='F')break;
                  if (command=='B')break;
                  //move valve in the direction indicated
                  wfd move_valve(valve-'A',200,0,command);
                  break;
               }
               // If between 1 & 8 and command is upper case
               if (valve >= '1' && valve <= '8' && command <= 'Z')
               {
               	// Update whether or not we need to check this valve position
                  // while moving this valve
               	if (valve >= '5' && valve <= '8')
                  {
                     // Shift to an actual MV postion
                     valve = valve - 4;
                  	// This valve position is UNCHECKED
                  	MV_position_checked[valve - '1'] = 0;
                  }
                  else if (MV_position_checked[valve - '1'] == 0)
                  {
                  	// This valve position is CHECKED
                  	MV_position_checked[valve - '1'] = 1;
                  }
                  pwm_set(valve - '1', 0, 0);
                  //if command was to stop
                  if (command=='S')
                  {
                     //stop valve
                     if(motor_valve_status[valve-'1']== 'F' ||
                        motor_valve_status[valve-'1']== 'B')
                     {
                        pwm_set(valve-'1',0,0);
                        motor_valve_status[valve-'1']='S';
                     }
                     else
                     {
                        move_motor_valve(valve-'1','S');
                     }
                     break;
                  }
                  //if command was to open
                  if (command == 'O' || command == 'F')
                  {
                  	// Check the current position if we have to
                  	if (MV_position_checked[valve - '1'] == 1)
                     {
                     	//get current position of valve
                     	position=readings[((valve-'0')*3)+13];
                     	//get current open limit of valve
                     	valve_limit[valve-'1']=readings[((valve-'0')*3)+14]+100;
                        //valve_limit[valve-'1']=9400;
                        //if limit has been reached break

                        //serBputs(" position ");
                        //wfd output_string(position);
                        //serBputs(" limit ");
                        //wfd output_string(valve_limit[valve-'1']);
                     	if (position>=valve_limit[valve-'1'])break;
                     }
                     // If we are setting the forward speed...
                     if (command == 'F')
                     {
                     	// Get the speed
                     	wfd motor_speed = input_string(4, 1000, 0);
                        // If the speed read did not error, set it
                        if (motor_speed >= 0)
                        {
                        	if (motor_speed > 1023) motor_speed = 1023;
                           pwm_set(valve - '1', (int)motor_speed, 0);
                        }
                     }
                  }
                  //if command was to close
                  else if (command=='C' || command == 'B')
                  {
                  	// Check the current position if we have to
                  	if (MV_position_checked[valve - '1'] == 1)
                     {
                     	//get current position of valve
                     	position=readings[((valve-'0')*3)+13];
                     	//get current close limit
                     	valve_limit[valve-'1']=readings[((valve-'0')*3)+15]-100;
                        //valve_limit[valve-'1']=8000;
                     	//if limit has been reached break

                        //serBputs(" position ");
                        //wfd output_string(position);
                        //serBputs(" limit ");
                        //wfd output_string(valve_limit[valve-'1']);
                     	if (position<=valve_limit[valve-'1'])break;

                        //if (position<=readings[((valve-'0')*3)+14]-10)break;
                     }
                     // If we are setting the backward speed...
                     if (command == 'B')
                     {
                     	// Get the speed
                     	wfd motor_speed = input_string(4, 1000, 0);
                        // If the speed read did not error, set it
                        if (motor_speed >= 0)
                        {
                        	if (motor_speed > 1023) motor_speed = 1023;
                           pwm_set(valve - '1', (int)motor_speed, 0);
                        }
                     }

                  }
                  else if (command=='D' )
                  {
                     wfd j=input_character(1000,0);  // j is + or - (direction).
                     //now get target:
	                  // get low byte of target
	                  wfd j2=input_character(1000,0);
	                  if (j2<0) break; // timeout
	                  k1=j2;
	                  // get high byte of target
	                  wfd j2=input_character(1000,0);
	                  if (j2<0) break; // timeout
	                  k2=j2;
	                  k2<<=8;
	                  k2+=k1;
	                  serBputs(" Going to k2: ");
	                  wfd output_string(k2);
                     //DelayMs(2000);
                  	// Check the current position if we have to
                  	if (MV_position_checked[valve - '1'] == 1)
                     {
                     	//get current position of valve
                     	position=readings[((valve-'0')*3)+13];
                     	//get current close limit
                     	//valve_limit[valve-'1']=readings[((valve-'0')*3)+15]-100;
                        valve_limit[valve-'1']=k2;
                     	//if limit has been reached break

                        //serBputs(" position    ");
                        //wfd output_string(position);
                       // serBputs(" limit ");
                        //wfd output_string(valve_limit[valve-'1']);

                        if (j=='+')
           					{
                           if (position>=valve_limit[valve-'1'])break;
                           //start moving valve as indicated
                 	         move_motor_valve(valve-'1','O'); //valve=0..3, command='O', 'C', or 'S'
	                        //void move_motor_valve(int valve, char direction)
	                        //valve=0..3, direction='O', 'C', or 'S'

                        }
                        else if (j=='-')
                        {
                           if (position<=valve_limit[valve-'1'])break;
                           //start moving valve as indicated
                 	         move_motor_valve(valve-'1','C'); //valve=0..3, command='O', 'C', or 'S'
	                        //void move_motor_valve(int valve, char direction)
	                        //valve=0..3, direction='O', 'C', or 'S'
                        }



                        //if (position<=readings[((valve-'0')*3)+14]-10)break;
                     }

                  }
                  if (command!='D' )
                  {
                  	//start moving valve as indicated
	                  move_motor_valve(valve-'1',command); //valve=0..3, command='O', 'C', or 'S'
                 		//void move_motor_valve(int valve, char direction)
							//valve=0..3, direction='O', 'C', or 'S'
                  }
               }
               break;
            //case '?': open serial port with specified baud rate
            case '?':
            	// Get serial port
               wfd channel = input_character(1000, 0);
               if (channel < 'A' || channel > 'D') break;
               // Get baud rate
               wfd read_write = input_character(1000, 0);
               if (read_write < '1' || read_write > '4') break;
            	switch (channel)
               {
               	case 'A':
                  switch (read_write)
                  {
	                  case '1': serAopen(9600); break;
	                  case '2': serAopen(19200); break;
	                  case '3': serAopen(57600); break;
	                  case '4': serAopen(115200); break;
                  }
	               break;
	               case 'B':
                  switch (read_write)
                  {
	                  case '1': serBopen(9600); break;
	                  case '2': serBopen(19200); break;
	                  case '3': serBopen(57600); break;
	                  case '4': serBopen(115200); break;
               	}
	               break;
	               case 'C':
                  switch (read_write)
                  {
	                  case '1': serCopen(9600); break;
	                  case '2': serCopen(19200); break;
	                  case '3': serCopen(57600); break;
	                  case '4': serCopen(115200); break;
                  }
	               break;
                  case 'D':
                  switch (read_write)
                  {
	                  case '1': serDopen(9600); break;
	                  case '2': serDopen(19200); break;
	                  case '3': serDopen(57600); break;
	                  case '4': serDopen(115200); break;
                  }
	               break;
               }
            break;
            // output solenoid valve status
            case 'V':
               wfd valve=input_character(1000,0);
               if (valve<0) break;
               if (valve>=65 && valve<=89)
               {//open if 5
                  output_character(current_valve_status[valve-65]);
               }//close if 5
               else if(valve>=49 && valve<=52)
               {
                  output_character(motor_valve_status[valve-49]);
               }
               break;
             // get position
            case 'Z': //e.g. type Z1 for position of valve 1.
            	wfd valve=input_character(1000,0);
             	position=readings[((valve-'0')*3)+13];
               serBputs(" position ");
               wfd output_string(position);
               break;
            //get  a reading and return it
            case 'Y':
               serBputs("Will return pos:.. ");
               break;
            case 'R':
               // get channel designator
               wfd channel=input_character(1000,0);
               //if no channel sent break
               if (channel<0) break;
               //if channel= ADC channels 'A'-'`' (0-31)
               if ((channel>=65) && (channel<=96))
               {//open if 6
                  ////output channel reading as a string
                 // serBputs("channel ");
                  //serBputc(channel);
                  //serBputs(" is ");
                  //wfd output_string(channel);
                 // serBputs("Reading is ");
                  wfd output_string(readings[channel-65]);
                  break;
               }//close if 6
               //if channel= analog output values '1'-'4'(0-3)
               else if ((channel>=49) && (channel<=52))
               {//open else if 6.2
                  //output value as string
                  wfd output_string(current_analog_out[channel-49]);
                  break;
               }//close else if 6.2
               // if channel= door switch input @ (64)
               else if (channel==64)
               {//open else if 6.3
                  //output a '1' or '0' as string
                  wfd output_string(BitRdPortI(PGDR,7));
                  break;
               }//close else if 6.3
               //if channel= anything else
               else
               {//open else 6
                  // output '?'(63)
                  output_character(63);
                  break;
               }//close else 6
               break;
            //change analog output
            case 'A':
               //get analog output channel
               wfd channel=input_character(1000,0);
               if(channel == 'R'){
						wfd channel=input_character(1000,0);
                  channel -= '1';
	               wfd output_string((long)current_analog_out[channel]);
                  break;
               }
               //make sure the channel is valid
               if ((channel<49) || (channel>52))break;
               wfd value=input_string(4,1000,0);
               if (value == 0)
               {
                  wfd zero_analog_out(channel-48);
               }
               else
               {
                  //make sure the total value is not higher than 4000
                  if (value > 4000) value = 4000;
                  //change analog output to new value
                  wfd change_analog_out(channel-48,(int)value);
               }
               break;
            //Return rabbit name
            case 'W':
               serBputs(Name);
               serBputc(13);
               break;
            //generator controls
            case 'G':
               wfd channel = input_character(1000,0);
               wfd generator = input_character(1000,0);
               if ((generator<'1')||(generator>'6')) break;
               switch(channel)
               {
                  //read limits
                  case 'L':
                     generator-='1';
                     wfd limits = get_gen_limits(generator);
                     output_character(limits);
                     break;
                  //generator forward
                  case 'F':
                  //generator backward
                  case 'B':
                     generator-='1';
                     wfd gen_speed = input_string(4,1000,0);
                     wfd start_gen(generator,channel,gen_speed);
                     break;
                  //generator stop
                  case 'S':
                     generator-='1';
                     wfd stop_gen(generator);
                     break;
               }
               break;
            //athena tempurature controls
            case 'T':
               temp = 0;
               wfd read_write = input_character(1000,0);
               wfd channel =  input_character(1000,0);
               if (channel<'1'||channel>'<') break;
               switch(read_write)
               {
                  //return a zone's current temperature
                  case 'T':
                     wfd temp = get_temp(channel);
                     wfd output_string(temp);
                     break;
                  //change a zone's set point temperature
                  case 'S':
                     wfd new_temp = input_string(4,1000,0);
                     wfd set_temp(channel,new_temp);
                     break;
                  //set a zone to standby mode
                  case 'B':
                     wfd set_standby(channel);
                     break;
                  //set a zone to normal mode
                  case 'N':
                     wfd set_normal(channel);
                     break;
               }
               break;
            case 'H':
            	wfd read_write = input_character(1000,0);
               switch(read_write)
               {
						case 'N':
							wfd new_temp = input_string(4,1000,0);
			         	serAputc(225);
	   	            serAputc((int)new_temp & 255);
	      	         serAputc((int)new_temp>>8);
                     break;
	               case 'B':
							serAputc(32);
                     break;
               }
               break;
            //Scale control.
            case 'M':
               wfd read_write = input_character(1000,0);
               wfd channel = input_character(1000, 0);
               switch(read_write)
               {
                  case 'R':
                     wfd get_weight(channel-'1');
                     break;
                  case 'Z':
                     wfd zero_scale(channel-'1');
                     break;
               }
               break;
            //GASRON Generator Stepper Motors
            case 'Q':
               wfd read_write = input_character(1000,0);
               wfd channel = input_character(1000,0);
               switch(read_write)
               {
                  case 'F':
                  case 'B':
                     wfd value = input_string(5,1000,0);
                     if(value>50000) value=50000;
                     if(value<1)break;
                     wfd start_stepper(read_write,channel,value);
                     break;
                  case 'S':
                  case 'H':
                     wfd stop_stepper(read_write,channel);
                     break;
                  case 'L':
                     wfd lim_ret=stepper_limit(channel);
                     output_character(lim_ret);
                     break;
               }
               break;
            //OASIS Temperature Controller
            case 'I':
	            wfd channel = input_character(1000,0);
            	switch(channel){
	               case 'T':
		            	wfd newer_temp = input_string(3,1000,0);
		               serAputc(225);
		               serAputc((int)newer_temp & 255);
			      	   serAputc((int)newer_temp>>8);
		               break;
                  case 'S':
		               serAputc(193);
                     wfd read_write = input_character_A(1000);
                     temp = read_write;
                     wfd read_write = input_character_A(1000);
                     temp += (read_write << 8);
                     wfd output_string(character);
		               break;
                  case 'R':
		               serAputc(200);
                     wfd read_write = input_character_A(1000);
							temp = read_write;
                     wfd read_write = input_character_A(1000);
                     temp += (read_write << 8);
                     wfd output_string(character);
		               break;
                     }
            // What is this old 'P' command about?
//            case 'P':
//	            serCputc('P');
//			      DelayMs(10);
//			      serCrdFlush();
//               break;
				//
            // Pulse the valve
            // This command allows us to pulse valves at a faster speed
            // then two move valve commands by taking a speed input
            // Ex: "PA050"
            case 'P':
            	// Get the valve number
               wfd valve = input_character(1000, 0);
               if (valve < 0) break;
               // Get the amount of time to hold the valve
               wfd value = input_string(3,1000,0);
               if (valve >= 'A' && valve <= 'X')
               	wfd pulse_valve(valve - 'A', (int)value);
            	break;
            case 'X':
               wfd channel = input_character(1000,0);
               switch(channel){
               	case 'R':
                  	serCopen(9600);
		            	wfd move_valve(23,200,0,'O');
                     nextCommand = 0;
						   start_time = TICK_TIMER+500;
		               while(nextCommand!=10 && start_time>TICK_TIMER)
					      {
		               	nextCommand=serCgetc();
					         if(nextCommand<=57 && nextCommand>=45)
					         {
					            serBputc(nextCommand);
					         }
					      }
                     serBputc(13);
					      serCrdFlush();
                     serCopen(57600);
                     break;
						case 'Z':
							wfd move_valve(22,200,0,'O');
                  case 'P':
							wfd move_valve(21,200,0,'O');
	               }
               break;
				// Controls how gauge count readings are collected - Mix
            case 'E':
               wfd nextCommand = input_character(1000, 0);
               if (nextCommand < 0) break;
               switch (nextCommand)
               {
               	// Change the number of readings to average together
	               case 'R':
	                  wfd temp = input_string(3, 1000, 0);
	                  if (temp < 1) break;
	                  numGaugeReads = (int)temp;
	                  break;
                  // Change the number of readings to ignore before readings
	               case 'I':
	                  wfd temp = input_string(3, 1000, 0);
	                  if (temp < 0) break;
	                  numGaugeIgnores = (int)temp;
	                  break;
                  // Get the number of readings to take and ignore
	               case 'V':
	                  wfd output_string(numGaugeReads);
	                  serBputc(',');
	                  wfd output_string(numGaugeIgnores);
	                  break;
               }
            	break;
            // Passes characters through to a serial port,
            // optionally receiving characters back.
            //
            // '>' sends the specified number of characters:
            // 	Send: ">"
				//				+ port letter
            //				+ num characters (2 digits)
            //				+ signal
            //
            // 	Three characters on A: ">A03XXX"
            // 	One character on D: ">D01X"
            //		Five characters on C: ">C05XXXXX"
            //
            // ']' sends characters up to, and not including, a stop character:
            // 	Send: "]"
            //				+ port letter
            //				+ stop character
            //				+ signal
            //
            //		Until '/' on A: "]A/XXX/"
            //		Until '.' on D: "]D.X."
            //		Until ';' on C: "]C:XXXXX:"
            //
            //	Receive commands can be tacked on before the signal to send.
            //
            //	'<' receives the specified number of characters after sending
            //		Send: ">" or "]"
            //				+ port letter
            //				+ num characters (on send) or stop character (on send)
            //				+ "<"
            //				+ num characters (on receive)
            //				+ signal
            //
            //		Send 3 characters on A and receive 4: ">A03<04XXX"
            //		Send 1 character on D and receive 2: ">D01<02X"
            //		Send characters until ':' on C and receive 6: "]C:<06XXXXX:"
            //
            //	'[' receives the specified number of characters after sending
            //		Send: ">" or "]"
            //				+ port letter
            //				+ num characters (on send) or stop character (on send)
            //				+ "["
            //				+ stop character (on receive)
            //				+ signal
            //
            //		Send 3 characters on A and receive until '/': ">A03[/XXX"
            //		Send 1 character on D and receive until '.': ">D01[.X"
            //		Send characters until ':' on C and receive until ':': "]C:[:XXXXX:"
            case ']':
            case '>':
               // Get the port
               wfd port = input_character(1000, 0);
               if (port < 'A' || port > 'D') break;
	            // Get the number of characters to send
               if (command == '>')
               {
	               wfd passTemp = input_string(2, 1000, 0);
	               if (passTemp < 1) break;
	               numChars = (int)passTemp;
               }
               // Get the stop character
               else
               {
               	wfd stopChar = input_character(1000, 0);
                  if (stopChar < 0) break;
               }
               // Check if the next character is '<' or '['
               nextCommand = serBpeek();
               if (nextCommand < 0) break;
               // Flush any info
	            switch (port)
	            {
	               case 'A': serAwrFlush(); serArdFlush(); break;
	               //case 'B': serBwrFlush(); serBrdFlush(); break;  // We talk through this
	               case 'C': serCwrFlush(); serCrdFlush(); break;
	               case 'D': serDwrFlush(); serDrdFlush(); break;
	            }
               // Send / receive
               if (nextCommand == '<' || nextCommand == '[')
               {
                  // Echo the '<' or '[' command and remove it from the buffer
               	wfd nextCommand = input_character(1000, 0);
                  // Get the number of characters to send
	               if (nextCommand == '<')
	               {
	                  wfd passTemp = input_string(2, 1000, 0);
	                  if (passTemp < 1) break;
	                  numCharsR = (int)passTemp;
	               }
	               // Get the stop character
	               else
	               {
	                  wfd stopCharR = input_character(1000, 0);
	                  if (stopCharR < 0) break;
	               }
                  // Send the signal
                  if (command == '>') wfd success = PassNumChars(port, numChars, 0);
                  else wfd success = PassToChar(port, stopChar, 0, 0);
                  // Handle errors
                  if (success == 0) { serBputs("ERROR"); serBputc(13); break; }
                  // Get the response
                  if (nextCommand == '<') wfd success = PassNumChars(port, numCharsR, 1);
                  else wfd success = PassToChar(port, stopCharR, 1, 1);
                  // Handle errors
                  if (success == 0) { serBputs("ERROR"); serBputc(13); break; }
                  // Sign off communications
                  serBputc(13);
                  break;
               }
               // Send
               else
               {
                  // Send the signal
                  if (command == '>') wfd success = PassNumChars(port, numChars, 0);
                  else wfd success = PassToChar(port, stopChar, 0, 0);
                  // Handle errors
                  if (success == 0) { serBputs("ERROR"); serBputc(13); break; }
                  // Sign off communications
                  serBputc(13);
                  break;
               }
            	break;
         }//close switch 1
      }//close costate 1
      costate
      {//open costate 2
         for (valve='1'; valve<='4'; valve++)
         {//open for 3
         	// If we don't need to check this valve, skip it
         	if (MV_position_checked[valve - '1'] == 0) continue;
            if (motor_valve_status[valve - '1']!='S')
            {//open if 7
               if (motor_valve_status[valve - '1']=='O')
               {//open if 8
                  position=readings[((valve - '0')*3)+13];
                  /*serBputs(" position    ");
                  wfd output_string(position);
                  serBputs(" limit ");
                  wfd output_string(valve_limit[valve-'1']); */
                  //wfd output_string(position);
                  if (position>=valve_limit[valve - '1']) //valve_limit[valve-'1']=readings[((valve-'0')*3)+15]-100; etc..
                  {//open if 9
                     move_motor_valve(valve - '1','S');
                  }//close if 9
               }//close if 8
               else if (motor_valve_status[valve - '1']=='C')
               {//open else if 8.1
                  position=readings[((valve - '0')*3)+13];
                  /*serBputs(" position    ");
                  wfd output_string(position);
                  serBputs(" limit ");
                  wfd output_string(valve_limit[valve-'1']);*/
                  //wfd output_string(position);
                  if (position<=valve_limit[valve - '1'])
                  {//open if 10
                     move_motor_valve(valve - '1','S');
                  }//close if 10
               }//close else if 8.1
            }//close if 7
         }//close for 3
      }//close costate 2
      costate
      {//open costate 3

         for (reads=0; reads<32; reads++)   //32 channels
         {//open for 4
            wfd readings[reads]=get_reading(reads,numGaugeReads,numGaugeIgnores);
            yield;
         }//close for 4
      }
   }//close for 1
}//close main