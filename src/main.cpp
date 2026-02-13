#include "Arduino.h"
#include "LedControl.h"
#include "Delay.h"
#include "Cijfers.h"

#define MATRIX_A 0
#define MATRIX_B 1

// Values are 260/330/400
#define ACC_THRESHOLD_LOW 282
#define ACC_THRESHOLD_HIGH 348

// Matrix
#define PIN_DATAIN 5
#define PIN_CLK 4
#define PIN_LOAD 6

// Accelerometer
#define PIN_X A1
#define PIN_Y A2

#define PIN_BUZZER 8
#define PIN_BUTTON 2

// This takes into account how the matrixes are mounted
#define ROTATION_OFFSET 90

#define DELAY_FRAME 100

#define MODE_HOURGLASS 0

#define SETUPEXIT 1000 // 1 seconde button indrukken om setup te verlaten
#define BUTTONDELAY 300 // 100 miliseconde per button delay. 
#define BUTTONMARGIN 250

// miliseconde per zandkorrel. Er zijn er 60 
long delaySeconds;

// De voldende tijden zijn beschikbaar: 30 seconden, 60 seconden, 120 seconden, 300 seconden (5 minuten) en 600 seconden (10 minuten)
int modes[] = {30, 60, 120, 300, 600};
int currentMode = 1; //  Start met de eerste mode = 60 sectonden

// OBSOLETE int mode = MODE_HOURGLASS;
int gravity;
int particlesTop;
int particlesBottom = 0;

bool moved = false;
bool dropped = false;

LedControl lc = LedControl(PIN_DATAIN, PIN_CLK, PIN_LOAD, 2);
NonBlockDelay d;
// OBSOLUTE int resetCounter = 0;
bool alarmWentOff = true;


long getDelayDrop()
{
    // Get delay between particle drops (in seconds)
    return delaySeconds;
}

coord getDown(int x, int y)
{
    coord xy;
    xy.x = x - 1;
    xy.y = y + 1;
    return xy;
}
coord getLeft(int x, int y)
{
    coord xy;
    xy.x = x - 1;
    xy.y = y;
    return xy;
}
coord getRight(int x, int y)
{
    coord xy;
    xy.x = x;
    xy.y = y + 1;
    return xy;
}

bool canGoLeft(int addr, int x, int y)
{
    if (x == 0)
        return false;
    return !lc.getXY(addr, getLeft(x, y));
}
bool canGoRight(int addr, int x, int y)
{
    if (y == 7)
        return false;
    return !lc.getXY(addr, getRight(x, y));
}
bool canGoDown(int addr, int x, int y)
{
    if (y == 7)
        return false;
    if (x == 0)
        return false;
    if (!canGoLeft(addr, x, y))
        return false;
    if (!canGoRight(addr, x, y))
        return false;
    return !lc.getXY(addr, getDown(x, y));
}

void goDown(int addr, int x, int y)
{
    lc.setXY(addr, x, y, false);
    lc.setXY(addr, getDown(x, y), true);
}
void goLeft(int addr, int x, int y)
{
    lc.setXY(addr, x, y, false);
    lc.setXY(addr, getLeft(x, y), true);
}
void goRight(int addr, int x, int y)
{
    lc.setXY(addr, x, y, false);
    lc.setXY(addr, getRight(x, y), true);
}

int countParticles(int addr)
{
    int c = 0;
    for (byte y = 0; y < 8; y++)
    {
        for (byte x = 0; x < 8; x++)
        {
            if (lc.getXY(addr, x, y))
            {
                c++;
            }
        }
    }
    return c;
}

bool moveParticle(int addr, int x, int y)
{
    if (!lc.getXY(addr, x, y))
    {
        return false;
    }

    bool can_GoLeft = canGoLeft(addr, x, y);
    bool can_GoRight = canGoRight(addr, x, y);

    if (!can_GoLeft && !can_GoRight)
    {
        return false;
    }

    bool can_GoDown = canGoDown(addr, x, y);

    if (can_GoDown)
    {
        goDown(addr, x, y);
    }
    else if (can_GoLeft && !can_GoRight)
    {
        goLeft(addr, x, y);
    }
    else if (can_GoRight && !can_GoLeft)
    {
        goRight(addr, x, y);
    }
    else if (random(2) == 1)
    {
        goLeft(addr, x, y);
    }
    else
    {
        goRight(addr, x, y);
    }
    return true;
}

void fill(int addr, int maxcount)
{
    int n = 8;
    byte x, y;
    int count = 0;
    for (byte slice = 0; slice < 2 * n - 1; ++slice)
    {
        byte z = slice < n ? 0 : slice - n + 1;
        for (byte j = z; j <= slice - z; ++j)
        {
            y = 7 - j;
            x = (slice - j);
            lc.setXY(addr, x, y, (++count <= maxcount));
        }
    }
}

int getGravity()
{

    // --------------------------------------------------
    // Variabelen voor de berekende richtingen
    // xCalc en yCalc krijgen alleen de waarden: -1, 0 of 1
    // --------------------------------------------------
    int xCalc = 0;
    int yCalc = 0;

    // --------------------------------------------------
    // Lees de ruwe ADC-waarden van de ADXL335
    // Waarden liggen typisch tussen 0 en 1023
    // --------------------------------------------------
    int x = analogRead(PIN_X);
    int y = analogRead(PIN_Y);

    // --------------------------------------------------
    // Zet de X-as om naar -1, 0 of 1
    // -1 : onder de lage drempel
    //  0 : binnen de deadzone
    //  1 : boven de hoge drempel
    // --------------------------------------------------
    if (x < ACC_THRESHOLD_LOW)
    {
        xCalc = -1;
    }
    else if (x <= ACC_THRESHOLD_HIGH)
    {
        xCalc = 0;
    }
    else
    {
        xCalc = 1;
    }

    // --------------------------------------------------
    // Zet de Y-as om naar -1, 0 of 1
    // Zelfde logica als bij de X-as
    // --------------------------------------------------
    if (y < ACC_THRESHOLD_LOW)
    {
        yCalc = -1;
    }
    else if (y <= ACC_THRESHOLD_HIGH)
    {
        yCalc = 0;
    }
    else
    {
        yCalc = 1;
    }

    // --------------------------------------------------
    // Bepaal de richting op basis van xCalc en yCalc
    // Alleen de vier hoofdassen worden herkend
    // --------------------------------------------------
    if (xCalc == 1 && yCalc == 0)
    {
        return 0; // rechts
    }
    if (xCalc == 0 && yCalc == 1)
    {
        return 90; // omhoog
    }
    if (xCalc == -1 && yCalc == 0)
    {
        return 180; // links
    }
    if (xCalc == 0 && yCalc == -1)
    {
        return 270; // omlaag
    }

    // --------------------------------------------------
    // Geen geldige richting (bijv. diagonalen of stilstand)
    // --------------------------------------------------
    return -1;
}
int getTopMatrix()
{
    return (getGravity() == 90) ? MATRIX_A : MATRIX_B;
}
int getBottomMatrix()
{
    return (getGravity() != 90) ? MATRIX_A : MATRIX_B;
}

void resetTime()
{
    for (byte i = 0; i < 2; i++)
        lc.clearDisplay(i);
    fill(getTopMatrix(), 60);
    d.Delay(getDelayDrop() * 0);
    d.Delay(1000);

    delaySeconds = (1000L * modes[currentMode]) / 60; // 1000 miliseconde per seconde, gedeeld door 60 zandkorrels
    Serial.print("Current mode: ");
    Serial.println(modes[currentMode]); 
    Serial.print("Delay per particle: ");
    Serial.println(delaySeconds); 

}
bool updateMatrix()
{
    int n = 8;
    bool somethingMoved = false;
    byte x, y;
    bool direction;
    for (byte slice = 0; slice < 2 * n - 1; ++slice)
    {
        direction = (random(2) == 1);
        byte z = slice < n ? 0 : slice - n + 1;
        for (byte j = z; j <= slice - z; ++j)
        {
            y = direction ? (7 - j) : (7 - (slice - j));
            x = direction ? (slice - j) : j;
            if (moveParticle(MATRIX_B, x, y))
            {
                somethingMoved = true;
            };
            if (moveParticle(MATRIX_A, x, y))
            {
                somethingMoved = true;
            }
        }
    }
    return somethingMoved;
}
boolean dropParticle()
{
    if (d.Timeout())
    {
        d.Delay(getDelayDrop());
        if (gravity == 0 || gravity == 180)
        {
            if ((lc.getRawXY(MATRIX_A, 0, 0) && !lc.getRawXY(MATRIX_B, 7, 7)) ||
                (!lc.getRawXY(MATRIX_A, 0, 0) && lc.getRawXY(MATRIX_B, 7, 7)))
            {
                lc.invertRawXY(MATRIX_A, 0, 0);
                lc.invertRawXY(MATRIX_B, 7, 7);
                tone(PIN_BUZZER, 440, 10);
                return true;
            }
        }
    }
    return false;
}
void alarm()
{
    Serial.println("Alarm!");
    for (int i = 0; i < 10; i++)
    {
        tone(PIN_BUZZER, 600 - (20 * i), 110);
        delay(180);
    }
}
void alarmStartup()
{
    Serial.println("Starting sound!");
    for (int i = 0; i < 10; i++)
    {
        tone(PIN_BUZZER, 440 + (20 * i), 110);
        delay(20);
    }
}

/**
 * Setup
 */
void setup()
{
    pinMode(PIN_BUTTON, INPUT_PULLUP); // Activeert de interne weerstand

    Serial.begin(9600);
    Serial.println("Starting Zandloper");
    alarmStartup();
    randomSeed(analogRead(A0));

    for (byte i = 0; i < 2; i++)
    {
        lc.shutdown(i, false);
        lc.setIntensity(i, 1);
        lc.clearDisplay(i);
    }

    resetTime();
}
// Functie om een getal te splitsen en te tonen
void toonGetal(int getal)
{
    int tiental = getal / 10;
    int eenheid = getal % 10;

    for (int i = 0; i < 8; i++)
    {
        // Tiental op display 0, eenheid op display 1
        lc.setRow(1, i, cijfers[tiental][i]);
        lc.setRow(0, i, cijfers[eenheid][i]);
    }
}
void displayMode(int mode)
{

    if (mode <= 1)
    { // 30 of 60 seconden
        int tiental = modes[mode] / 10;
        int eenheid = modes[mode] % 10;

        for (int i = 0; i < 8; i++)
        {
            // Tiental op display 0, eenheid op display 1
            lc.setRow(1, i, cijfers[tiental][i]);
            lc.setRow(0, i, cijfers[eenheid][i]);
        }
    }
    else
    { // 2, 5 of 10 minuten = > 1 minuut
        int minutes = modes[mode] / 60;

        int tiental = minutes;
        // int eenheid = modes[mode] % 10;

        for (int i = 0; i < 8; i++)
        {
            // Tiental op display 0, eenheid op display 1
            if (minutes < 10)
            { // 2 of 5 minuten = 1 minuut met " teken erachter
                lc.setRow(1, i, cijfers[tiental][i]);
                lc.setRow(0, i, cijfers[ACCENT][i]); // " teken
            }
            else
            { // 10 minuten = 1 met " teken erachter
                lc.setRow(1, i, cijfers[CIJFER_1][i]);
                lc.setRow(0, i, cijfers[CIJFER_0ACCENT][i]); // 0 met " teken erachter
            }
        }
    }
}

// Functie om de button delay te meten. Geeft terug hoe lang er op de knop werd gedrukt in miliseconden. De functie wacht tot de button wordt losgelaten voordat hij de tijd teruggeeft.
long getButtonDelay()
{
    long buttonDelay = millis();

    while (digitalRead(PIN_BUTTON) == LOW  && (millis() - buttonDelay) < SETUPEXIT)
    {
        // Wacht tot de button wordt losgelaten
        yield(); // Laat andere taken toe terwijl we wachten
    }
    for (byte i = 0; i < 2; i++)
        lc.clearDisplay(i);
    while (digitalRead(PIN_BUTTON) == LOW)
    {
        // Wacht tot de button wordt ingedrukt
        yield(); // Laat andere taken toe terwijl we wachten
    }   
    return millis() - buttonDelay;
}

void setupZandloper()
{
    // We komen van een sensor LOW en wachten tot deze HIGH wordt. Dit voorkomt dat we direct weer terug gaan naar de setup als we al in de setup zitten.
    while (digitalRead(PIN_BUTTON) == LOW)
    {
        yield(); // Laat andere taken toe terwijl we wachten
    }
    Serial.println("Setting up Zandloper");

    bool blnSetupMode = true;

    while (blnSetupMode)
    {
        int milisPerMode = (millis() / 30) % 20;
        if (milisPerMode >= 10) 
            milisPerMode = 19 - milisPerMode; 


        lc.setIntensity(0, milisPerMode);
        lc.setIntensity(1, milisPerMode);

        displayMode(currentMode); 
        long buttonDelay = getButtonDelay() ;

        if (buttonDelay> SETUPEXIT)
            blnSetupMode = false ;

        // Check of de button delay binnen de marge van de button delay ligt. Zo ja, ga naar de volgende mode.
        if (buttonDelay >= BUTTONDELAY - BUTTONMARGIN && buttonDelay <= BUTTONDELAY + BUTTONMARGIN)
        {
            currentMode++;
            if (currentMode >= sizeof(modes) / sizeof(modes[0]))
                currentMode = 0;
        }
    }

    // De instelling is nu gedaan
    resetTime();
    alarmWentOff = true ;

}

/**
 * Main loop
 */
void loop()
{
    delay(DELAY_FRAME);

    gravity = getGravity();

    if (gravity != -1)
        lc.setRotation((ROTATION_OFFSET + gravity) % 360);

    moved = updateMatrix();
    dropped = dropParticle();

    particlesTop = countParticles(getTopMatrix());
    particlesBottom = countParticles(getBottomMatrix());

    if (!moved && !dropped && !alarmWentOff && ((particlesTop == 60 && gravity == 0) || (particlesBottom == 60 && gravity == 180)))
    {
        alarmWentOff = true;
        alarm();
    }

    if (dropped)
    {
        alarmWentOff = false;
    }


    if (digitalRead(PIN_BUTTON) == LOW)
    {
        setupZandloper();
    }
}
