
/****
  Select NodeMCu 1.0 (ESP-12E Module) Board

  The following code is tailored for our project's requirements. It includes functionalities to read raw sensor values and store them in the 'rawADC' array. 
  Additionally, a calibration function has been implemented. 
  Furthermore, a mapping function has been integrated into the calibrated sensor reading process.
  The crucial component, 'Line Error' calculation, has been incorporated along with some basic configurations for the multiplexer and motors, which can be customized according to specific needs.
  Overall, this code facilitates the creation of a line-following robot using ESP8266.

****/




//********************      common macros   *****************************//


  #define SET     1U
  #define RESET   0U                        // Define constants for readability and ease of use



//******************************     SENSORS DEFINE       ****************************//


  #define ADC_INPUT           A0            // multiplexer output pin
  #define SELECT_S0           16            // multiplexer channel S0 select pin
  #define SELECT_S1           5             // multiplexer channel S1 select pin
  #define SELECT_S2           4             // multiplexer channel S2 select pin
  #define NUM_SENSORS         5             // Number of sensors using in sensor array
  #define THRESHOLD           250           // theshold btw black and white surface (range 0-500), work for me "230"
  #define ADC_UPPER_LIMIT     1000          // Map upper limit
  #define ADC_LOWER_LIMIT     0             // Map lower limit



//*************************       SENSOR VARS       **********************************//


  volatile uint16_t rawADC[NUM_SENSORS];                       // holds raw sensor values of each sensor
  volatile uint16_t calibratedSensorsValues[NUM_SENSORS];      // holds calibrated values after calibration       
  volatile uint16_t minValues[NUM_SENSORS];                    // holds minimum value of each sensor
  volatile uint16_t maxValues[NUM_SENSORS];                    // holds maximun value of each sensor



//****************************      MOTOR VARS      ********************************//


  const uint16_t basespeedA = 150;
  const uint16_t basespeedB = 150;
  volatile uint16_t flag_on_line = RESET;                           // Static variable to retain flag value


// Motor A connections
  const int motorPin1A = D4;                                  // Connected to input 1 of L298N
  const int motorPin2A = D5;                                  // Connected to input 2 of L298N
  const int enablePinA = D3;                                  // Connected to enable pin for Motor A
  
// Motor B connections
  const int motorPin1B = D6;                                  // Connected to input 3 of L298N
  const int motorPin2B = D7;                                  // Connected to input 4 of L298N
  const int enablePinB = D8;                                  // Connected to enable pin for Motor B


//****************************      PID VARS      ********************************//


// Define PID constants
  double Kp = 0.35;
  double Ki = 0.0;                        // Adjust Kp And Kd Values according to your Specification (* Note * :- Don't Change Ki Value until you want to write a better algorithm for this project )
  double Kd = 0.35;

  float errorSum = 0.0;
  float lastError = 0.0;



// Setup function to initialize serial communication and sensor pins

  
  void setup() {
    Serial.begin(9600); 
    sensor_INIT();
    motor_INIT();


    leftMotorPWM(100);
    rightMotorPWM(-100);
    for(int i=0;i < 200;i++)
    {
      // spin bot clockwise
      // calibrate sensors
      calibrateSensors();
    }

    delay(100);

    leftMotorPWM(-100);
    rightMotorPWM(100);
    for(int i=0;i < 200;i++)
    {
      // spin bot counterclockwise
      // calibrate sensors
      calibrateSensors();
    }
    
    leftMotorPWM(0);
    rightMotorPWM(0);

    delay(3000);
  }


  // Loop function to continuously read and print sensor values
  void loop() {
    int lineError = getLineError();
    Serial.println(lineError);
    // delay(10);
  

    double error = lineError;                                          // Get line error

    double error_d = error - lastError;
    errorSum += error * Ki;                                            // Update integral term

    double motor_feed = Kp * error + Kd * error_d + Ki * errorSum;     // Calculate PID output

    leftMotorPWM(basespeedA + motor_feed);
    rightMotorPWM(basespeedB - motor_feed);

    lastError = error;                                                  // Update last error
  
    
    static char flag_on_line = RESET;                                   // Static variable to retain flag value
    for(int i = 0; i < NUM_SENSORS; i++) {
        if(calibratedSensorsValues[i] < THRESHOLD) {
            flag_on_line = SET;
        }
    }
   } 


/*****************************       Sensor Section        ****************************************/


  // Function to print the sensor values to the serial monitor

  void printSensors(volatile uint16_t array[NUM_SENSORS]) {
    for(int i = 0; i < NUM_SENSORS; i++) {
      Serial.print(i);
      Serial.print("-");
      Serial.print(array[i]);
      Serial.print("   ");
    }
    Serial.println(); 
  }


  // Function to initialize sensor pins

  void sensor_INIT(void) {
    pinMode(SELECT_S0, OUTPUT); 
    pinMode(SELECT_S1, OUTPUT); 
    pinMode(SELECT_S2, OUTPUT); 
    pinMode(ADC_INPUT, INPUT);
  }


  /*
        Introduction: Reads raw analog sensor values from multiple channels.
        Parameters: None
        Returns: None
        Note: The function selects each sensor channel one by one, reads the analog
              value from the ADC pin, and stores it in the rawADC array.
  */

  void getRawValue(void) {
    for(int i = 0; i < NUM_SENSORS; i++) {
      select_channel(i);                                         // Select sensor channel
      delay(2);                                                  
      rawADC[i] = analogRead(ADC_INPUT);                         // Read analog sensor value and store it
    }
  }

  /*

        Introduction: Initializes the minValues and maxValues arrays with raw sensor values.
        Parameters: None
        Returns: None
        Note: This function initializes the minValues and maxValues arrays with initial sensor readings.

  */

  void getMinMax(void) {
    getRawValue();                                               // Get raw sensor values
    for(int i = 0; i < NUM_SENSORS; i++) {
      minValues[i] = rawADC[i];                                  // Set initial min values
      maxValues[i] = rawADC[i];                                  // Set initial max values
    }
  }


  /*

        Introduction: Calibrates the sensor readings by updating minValues and maxValues.
        Parameters: None
        Returns: None
        Note: This function calibrates the sensor readings by updating the minValues and maxValues arrays based on current sensor readings.

  */


  void calibrateSensors(void) {
    static char flag_min_max_stored = RESET;
    if(flag_min_max_stored == RESET)
    {
      getMinMax();                                    // Initialize min and max values if not already done
      flag_min_max_stored = SET;                      // Set the flag to indicate that min and max values are stored
    }

    getRawValue();                                    
    for(int i = 0; i < NUM_SENSORS; i++) {
      if(minValues[i] > rawADC[i]) {
        minValues[i] = rawADC[i];
      }
      else if (maxValues[i] < rawADC[i]) {
        maxValues[i] = rawADC[i];
      }
    }
  }


  void getCalibratedSensorsADC(void)
  {
    getRawValue();
    for(int i=0;i<NUM_SENSORS;i++)
    {
      int deNom = maxValues[i] - minValues[i];
      if(deNom > 0)
      {
         calibratedSensorsValues[i] = (((rawADC[i] - minValues[i])*(ADC_UPPER_LIMIT - ADC_LOWER_LIMIT))/(deNom)) + ADC_LOWER_LIMIT;
      }
      else
      {
        // return 0;
      }
       
    }
  }


  void online(void) {
    for(int i = 0; i < NUM_SENSORS; i++) {
        if(calibratedSensorsValues[i] < THRESHOLD) {
            flag_on_line = SET;
        }
    }
  }

  int getLineError(void) {
    getCalibratedSensorsADC();
    online();

    int errorValue = (int)((calibratedSensorsValues[0]*2) + 
                           (calibratedSensorsValues[1]*1) + 
                           (calibratedSensorsValues[2]*0) - 
                           (calibratedSensorsValues[3]*1) - 
                           (calibratedSensorsValues[4]*2));

    return errorValue;

    if (flag_on_line){
      errorValue = errorValue;
    }
    else{
      errorValue = lastError;
    }

    return errorValue;
}



  //****************************      MULTIPLEXER SECTION      ********************************//


// Function to select sensor channel based on the provided number

  void select_channel(uint8_t num) {
    switch (num) {
      case 3:
        digitalWrite(SELECT_S0, LOW);
        digitalWrite(SELECT_S1, LOW);
        digitalWrite(SELECT_S2, LOW);
        break;
      case 0:
        digitalWrite(SELECT_S0, LOW);
        digitalWrite(SELECT_S1, LOW);
        digitalWrite(SELECT_S2, HIGH);
        break;
      case 1:
        digitalWrite(SELECT_S0, LOW);
        digitalWrite(SELECT_S1, HIGH);
        digitalWrite(SELECT_S2, LOW);
        break;
      case 4:
        digitalWrite(SELECT_S0, HIGH);
        digitalWrite(SELECT_S1, HIGH);
        digitalWrite(SELECT_S2, LOW);
        break;
      case 2:
        digitalWrite(SELECT_S0, HIGH);
        digitalWrite(SELECT_S1, LOW);
        digitalWrite(SELECT_S2, LOW);
        break;
      default:
        break;
    }
  }


  //********************************   MOTOR SECTION    ********************************//

void motor_INIT() {
  pinMode(motorPin1A, OUTPUT);
  pinMode(motorPin2A, OUTPUT);
  pinMode(enablePinA, OUTPUT); 
  pinMode(motorPin1B, OUTPUT);
  pinMode(motorPin2B, OUTPUT);
  pinMode(enablePinB, OUTPUT);

  // Set initial motor speed
  analogWrite(enablePinA, basespeedA);  
  analogWrite(enablePinB, basespeedB);
}

void leftMotorPWM(int pwm)
 {
  if(pwm == 0)
  {
    // stop left motor;
    // sig pin 1&2 high;
    // set pwm =0;
    digitalWrite(motorPin1A,  LOW);
    digitalWrite(motorPin2A,  LOW);
    analogWrite(enablePinA, 0);

  }
  else if(pwm > 0)
  {
    // clockwise
    // pin1 high, pin2 low
    // set pwm = pwm;
    digitalWrite(motorPin1A,  HIGH );
    digitalWrite(motorPin2A,  LOW );
    analogWrite(enablePinA, basespeedA);
  }
  else if(pwm < 0)
  {
    // counterclockwise
    // pin2 high, pin1 low;
    // set pwm = pwm;
    digitalWrite(motorPin1A,  LOW );
    digitalWrite(motorPin2A,  HIGH );
    analogWrite(enablePinA, basespeedA);
  }
 }

 void rightMotorPWM(int pwm)
 {
  if(pwm == 0)
  {
    // stop right motor;
    // sig pin 1&2 high;
    // set pwm =0;
    digitalWrite(motorPin1B,  LOW);
    digitalWrite(motorPin2B,  LOW);
    analogWrite(enablePinB, 0);
  }

  else if(pwm > 0)
  {
    // clockwise
    // pin1 high, pin2 low
    // sert pwm = pwm;
    digitalWrite(motorPin1B, LOW );
    digitalWrite(motorPin2B, HIGH );
    analogWrite(enablePinB, basespeedB);
  }
  else if(pwm < 0)
  {
    // counterclockwise 
    // pin2 high, pin1 low;
    // set pwm = pwm;
    digitalWrite(motorPin1B, HIGH );
    digitalWrite(motorPin2B, LOW );
    analogWrite(enablePinB, basespeedB);
  }
 }
