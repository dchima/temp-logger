/*Temperature sensor and Data Logging
    by Chima Daniel Nnadika
    201077064
*/

#include "mbed.h"
#include "N5110.h"


//Address for ADD0 connected to GND
#define TMP102_ADD 0x48
#define TMP102_R_ADD 0x91
#define TMP102_W_ADD 0x090

//Register Addresses
#define TEMP_REG 0x00
#define CONFIG_REG 0x01
#define THIGH_REG 0x02
#define TLOW_REG 0x03

//initialize timer interrupts to call analysis functions
Ticker getdat; 
Ticker plotdat;
Ticker homedat;

//set serial I/O for debbugging
Serial serial(USBTX,USBRX);
//set I2C object for temperature data and clock (SDA, SLC)
I2C tmp102(p28, p27);

//set I/O buttons, Switches and LEDS
InterruptIn sw(p18);
DigitalIn buttonA(p16, PullUp);
DigitalIn buttonB(p17, PullUp);
BusOut leds(LED1, LED2, LED3, LED4);

//Power up LCD display and declare method
N5110 lcd(p7,p8,p9,p10,p11,p13,p21);
LocalFileSystem local("local"); // create local filesystem

//global variables used between functions to store time and temperature
//data for plotting and printing to LCD display
float tempo0, tempo1, tempo2, tempo3,tempo4, tempo5, tempo6; //temperature storage variables for plot
float temparray[84]; //temperature plot array
int j = 0, sw_state; // plot counter and switch variabele
char tempBuffer[14],  tempBuffer2[14], timeBuffer[14], dateBuffer[14]; //data buffers to store temperature and time
volatile bool isPushed = false; //boolean for storing button push instance
int mode = 0; //mode counter

//file logging function prototype
void logToFile(char t1[], float t2);


// hang in infinite loop flashing error code
void error(int code){
    while(1) {
        leds = 0;
        wait(0.25);
        leds = code;
        wait(0.25);
    }
}

//Initialize TMP102
void initTMP102(){
    tmp102.frequency(400000); // set bus speed to 400 kHz
    int ack; // used to store acknowledgement bit
    char data[2]; // array for data
    char reg = CONFIG_REG; // register address
    //////// Read current status of configuration register ///////
    ack = tmp102.write(TMP102_W_ADD,&reg,1); // send the slave write address and the configuration register address
    if (ack)
    error(1); // if we don't receive acknowledgement, flash error message
    ack = tmp102.read(TMP102_R_ADD,data,2); // read default 2 bytes from configuration register and store in buffer
    if (ack)
    error(2); // if we don't receive acknowledgement, flash error message
    ///////// Configure the register //////////
    // set conversion rate to 1 Hz
    data[1] |= (1 << 6); // set bit 6
    data[1] &= ~(1 << 7); // clear bit 7
    //////// Send the configured register to the slave ////////////
    char tx_data[3] = {reg,data[0],data[1]}; // create 3-byte packet for writing (p12 datasheet)
    ack = tmp102.write(TMP102_W_ADD,tx_data,3); // send the slave write address, config reg address and contents
    if (ack)
    error(3); // if we don't receive acknowledgement, flash error message
}

float temperature(){
    int ack; // used to store acknowledgement bit
    char data[2]; // array for data
    char reg = TEMP_REG; // temperature register address
    
    ack = tmp102.write(TMP102_W_ADD,&reg,1); // send temperature register address
    if (ack)
        error(5); // if we don't receive acknowledgement, flash error message
    ack = tmp102.read(TMP102_R_ADD,data,2); // read 2 bytes from temperature register and store in array
    if (ack)
        error(6); // if we don't receive acknowledgement, flash error message
    int temperature = (data[0] << 4) | (data[1] >> 4);
    return temperature*0.0625;
}


void logToFile(char *t1, float t2){
    leds = 15; // turn on LEDs for feedback
    FILE *fp = fopen("/local/log.csv", "a"); // open 'log.txt' for appending
    // if the file doesn't exist it is created, if it exists, data is appended to the end
    fprintf(fp,"=\"%s\" , %.2f\n",t1,t2); // print string to file
    fclose(fp); // close file
    leds = 0; // turn off LEDs to signify file access has finished
}

//This function gets the current time and temperature
//and send it t the logtofile function for logging
//
void timeTemp (){
    char buffer[32]; // buffer used to store time string
    float Temp, realtemp;
    int sw_state = sw.read();
    time_t seconds = time(NULL); // get current time
    strftime(buffer, 32 ,"%R %d/%m/%y", localtime(&seconds));
        
    initTMP102();
    Temp = temperature(); //ges temperature value from sensor
    realtemp = Temp-6; //adjusts read temperature to measured
    // print over serial
    //serial.printf("%s , %.2f %d\n",buffer,Temp,sw_state);
    while(sw_state){ //start log only when switch is on a high (1)
        logToFile(buffer,realtemp); //send data to logtofile function
        break;
    }
}

//this function gets current data for the home display
void homeData(){
    //data for temp on home screen
    float Temp, realtemp;
    initTMP102();
    Temp = temperature();
    realtemp = Temp-6;
    sprintf(tempBuffer,"%.2f%cC",realtemp,(char)167); //gets data instring format for lcd
    sprintf(tempBuffer2,"%.2f%cF",(realtemp*1.8)+32,(char)167); //gets farenheit value for lcd
    
    //the tempo variables are used to draw bar on lcd
    //showing instantaneous temperature changes
    tempo0 = (47*realtemp)/60; //ratio of data to screensize for temperature bar
    tempo5 = tempo4;
    tempo4 = tempo3;
    tempo3 = tempo2;
    tempo2 = tempo1;
    tempo1 = tempo0;
    //get time data for home screen
    time_t seconds = time(NULL); // get current time
    strftime(timeBuffer, 14 , "%X", localtime(&seconds));
    strftime(dateBuffer, 14 , "%D", localtime(&seconds));
    //get logging state for gui
    sw_state = sw.read();
}

//This function displays the data on the home screen 
void homeDisplay(){
    //display temp on home screen
    lcd.clear();
    lcd.normalMode();
    
    //print temperature (celcius and farenheit)
    lcd.printString(tempBuffer,40,4);
    lcd.printString(tempBuffer2,37,5);
    
    //display temperature bar
    int i;
    for (i = 47; i > (47-tempo0); i--) {
        lcd.setPixel(12, i); lcd.setPixel(11, i);  
    }
    
    for (i = 47; i > (47-tempo1); i--){
         lcd.setPixel(10, i); lcd.setPixel(9, i);
        }
        
    for(i = 47; i > (47-tempo2); i--){
        lcd.setPixel(8, i); lcd.setPixel(7, i);
    }

    for(i = 47; i > (47-tempo3); i--){
            lcd.setPixel(6, i); lcd.setPixel(5, i);
    }

    for(i = 47; i > (47-tempo4); i--){
        lcd.setPixel(4, i); lcd.setPixel(3, i);
    }

    for (i = 47; i > (47-tempo5); i--){
        lcd.setPixel(2, i); lcd.setPixel(1, i);
    }

    //display time
    lcd.printString(dateBuffer,36,0);
    lcd.printString(timeBuffer,36,1);
    
    //display logging 
    if(sw_state){
        lcd.drawRect(20,5,10,10,FILL_BLACK); 
    }else{lcd.drawRect(20,5,10,10,FILL_TRANSPARENT);}
    lcd.refresh();
}  

//this function gets the plot data for the display
void plotData(){
    float Temp;
    float realtemp;
    Temp = temperature();
    realtemp = Temp-6;
    tempo6 = realtemp;
    
    //we offset the data to start plotting  from the
    //20th pixel in the display.
    if(j > 63){
        for(int i = 20; i <= 82; i++){
            temparray[i] = temparray[i+1]; //shift array values
        }
        //append new data value to array
        temparray[83] = realtemp/60;
        j = 64;
    }
    else{
        //initialize and append
        temparray[j+20] =  realtemp/60;
        j++;
    }
}

//this function displays the plot on the plot screen
void plotDisplay(){
       lcd.clear();
       
       //set environment
        lcd.printString("log",2,0);
        int i;
        for(i = 0; i <= 83; i++){
            lcd.setPixel(i,0);
            lcd.setPixel(i,47);
        }
        for(i = 0; i <= 47; i++){
            lcd.setPixel(20,i);
            lcd.setPixel(0,i);
            lcd.setPixel(83,i);
        }
        
        //plot data (84 elements)
        lcd.plotArray(temparray);
        
        //draw temperature level identifier
        if(tempo6>26){
        lcd.drawCircle(74,7,5,FILL_BLACK);
        }
        else if (tempo6<=26 && tempo6>=13){
            lcd.drawCircle(74,7,5,FILL_TRANSPARENT);
        }
        else if(tempo6<13){
            lcd.drawCircle(77,7,1,FILL_TRANSPARENT);
            lcd.drawCircle(71,7,1,FILL_TRANSPARENT);
            lcd.drawCircle(74,5,1,FILL_TRANSPARENT);
            lcd.drawCircle(74,9,1,FILL_TRANSPARENT);   
        }
        
        lcd.refresh();
}

//this function displays the "About" screen
void aboutDisplay(){
    lcd.clear();
    lcd.printString("CODE BY",20,0);
    lcd.printString("CHIMA NNADIKA",3,1);
    lcd.printString("201077064",15,3);
    lcd.printString("UNI OF LEEDS",8,5);
    lcd.refresh();
}

//this function displays the home detail screen
void homeDetail(){
    lcd.clear();
    
    //draws and explains the logging identifier
    lcd.drawRect(2,1,10,10,FILL_BLACK); 
    lcd.drawRect(2,13,10,10,FILL_TRANSPARENT); 
    lcd.printString("LOGGING ON",16,0);
    lcd.printString("LOGGING OFF",16,2);
    lcd.printString("TOGGLE S/W",16,4);
    lcd.printString("1 LOG /MIN",16,5);
    lcd.refresh();  
}

//this function displays the plot details page
void plotDetail(){
    lcd.clear();
    
    //draws the temperature level identifiers
    lcd.drawCircle(5,4,4,FILL_BLACK);
    lcd.drawCircle(5,12,4,FILL_TRANSPARENT);
    lcd.drawCircle(8,19,1,FILL_TRANSPARENT);
    lcd.drawCircle(2,19,1,FILL_TRANSPARENT);
    lcd.drawCircle(5,21,1,FILL_TRANSPARENT);
    lcd.drawCircle(5,17,1,FILL_TRANSPARENT); 
    // explains the level identfier meanings
    lcd.printString("HOT",16,0);
    lcd.printString("WARM",16,1);
    lcd.printString("COLD",16,2);
    lcd.printString("MAXPLOT:60 DEG",0,5);
    
    //points to current temperature level
    if(tempo6>26){lcd.printString("<<",44,0);}
    else if(tempo6<=26 && tempo6>=13){lcd.printString("<<",44,1);}
    else if(tempo6<13){lcd.printString("<<",44,2);}
    //prints temperature in current screen
    lcd.printString(tempBuffer,40,4);
    lcd.refresh();
}
int main(){
    //Aquire data via ticker interrupts
    getdat.attach(&timeTemp, 60.0);
    homedat.attach(&homeData, 0.2);
    plotdat.attach(&plotData, 0.71);
    
    //initialise screen and set home
    lcd.init();
    lcd.setContrast(0.5);
    lcd.inverseMode();
    wait(0.5);
    lcd.normalMode();
    wait(0.5);
    lcd.inverseMode();
    wait(0.5);
    lcd.normalMode();
    lcd.printString("TEMPERATURE",0,0);
    lcd.printString("SENSOR/LOGGER",0,1);
    lcd.printString("BUTTON A",0,2);
    lcd.printString(">>NEXT PAGE",0,3);
    lcd.printString("BUTTON B",0,4);
    lcd.printString(">>PAGE DETAILS",0,5);
    lcd.refresh();
    wait(5);
    
    
    while(1) {
        lcd.setBrightness(0.5);
        lcd.clear();
        
        //detect button state 
        //and switch between modes depending on button push
        if(buttonA == 0 && (mode == 0 | mode == 4)){
            while(!buttonA){} //wait for button release
            mode = 1;    
        }
        else if(buttonA == 0 && (mode == 1 | mode == 3)){
            while(!buttonA){}
            mode = 2;
        }
        else if(buttonA == 0 && mode == 2){
            while(!buttonA){}
            mode = 0;           
        }else if(buttonB == 0 && mode == 1){
            while(!buttonB){}
            mode = 3;
        }else if(buttonB == 0 && mode == 3){
            while(!buttonB){}
            mode = 1;   
        }else if(buttonB == 0 && mode == 0){
            while(!buttonB){}
            mode = 4;
        }else if(buttonB == 0 && mode == 4){
            while(!buttonB){}
            mode = 0; 
        }       
        
        
        //display states
        if(mode == 1){
            plotDisplay();   
        }
        else if(mode == 2){
            aboutDisplay();
        }
        else if(mode == 3){ //activated on Button B push while on plot display
            plotDetail();   //plot details page expands on plot display
        }else if(mode == 4){ //activated on Button B push while on home display
            homeDetail();   //home detail page expands on home display
        }else{
            homeDisplay();
        }       
    }
}