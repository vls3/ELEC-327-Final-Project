#include <msp430.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_LED 4
#define NUM_NOTES 8
#define MAX_LENGTH 5
#define N 120
#define SAMPLE_RATE 100000
#define TWIDDLE 0
//#define MAX_DELAY 100
/*
 * Define the frequencies for each musical note.
 */

#define A 440
#define E 758
#define C 956
#define G 784

//Define buzzer frequencies.
#define A_buzz 1136 //440 Hz
#define B_buzz 1012 //494 Hz
#define C_buzz 956 // 523.25 Hz
#define D_buzz 851 //587.33 Hz
#define E_buzz 758 //659.25 Hz
#define F_buzz 716 // 698.46 Hz
#define F_s_buzz 675
#define G_buzz 638 //784 Hz
#define rest 0
#define default_brightness 0xE3
#define LED_OFF 0xE0

struct RGB
{
        unsigned char red;
        unsigned char green;
        unsigned char blue;
        unsigned char brightness;

};

//Define basic LED colors.

static struct RGB red_LED = {0xFF, 0x00, 0x00, default_brightness};
static struct RGB green_LED = {0x00, 0xFF, 0x00, default_brightness};
static struct RGB blue_LED = {0x00, 0x00, 0xFF, default_brightness};
static struct RGB purple_LED = {0xFF, 0x00, 0xFF, default_brightness};
static struct RGB yellow_LED = {0xFF, 0xFF, 0x00, default_brightness};
static struct RGB cyan_LED = {0x00, 0xFF, 0xFF, default_brightness};
static struct RGB white_LED = {0xFF, 0xFF, 0xFF, default_brightness};
static struct RGB off_LED = {0x00, 0x00, 0x00, 0xE0};


//Define winning song.
//const int twinkle_length = 14;
//static int twinkle[14] = {A, G, E, D, E, 0, D, E, A, B, A, G, E, C};

int samples[N] = {0};
int buzzer_tones[NUM_NOTES] = {A_buzz, B_buzz, C_buzz, D_buzz, E_buzz, F_buzz, F_s_buzz, G_buzz};
unsigned int frequency_A, frequency_C, frequency_E, frequency_G, freq_guess;
unsigned int highest_freq, final_guess;


///A  G E D E - DE ABAGE -

static int MAX_DELAY = 50;
static unsigned int timer_period = 750;
int current_note_offset = 0;

//static int buzzer_tones[4] = {A_buzz, C_buzz, E_buzz, G_buzz};
//static int button_pressed = -1;
int num_delays = 0;

//Define start/stop signals.
//static unsigned char start[] = {0x00, 0x00, 0x00, 0x00};
//static unsigned char end[] = {0xFF, 0xFF, 0xFF, 0xFF};

static void flashOneLED(struct RGB color, unsigned char brightness, int LED_num);
static void flashPattern(struct RGB LED);
static int *generateRandomButtonSequence(int length);
void playAnimation(struct RGB LEDs[], int length);
void playSequence(int *sequence, int length);
static void startPattern();
static void endPattern();
void PlaySound(int note, int duration);
void playSong(int song[], int length, int duration);


/**
 * main.c
 */
int main(void)
{
        int i, j;

        //Define the win/loss animations.
        struct RGB loss[16] = {red_LED, purple_LED, red_LED, purple_LED,
                                      red_LED, red_LED, red_LED, red_LED,
                                      yellow_LED, yellow_LED, yellow_LED, yellow_LED,
                                      red_LED, red_LED, red_LED, red_LED };

       struct RGB win[16] = {green_LED, green_LED, green_LED, green_LED,
                             yellow_LED, green_LED, yellow_LED, green_LED,
                           blue_LED, cyan_LED, blue_LED, cyan_LED,
                           purple_LED, cyan_LED, purple_LED, cyan_LED};

       //Define the colors for each LED.
       struct RGB LED_colors[4] = {green_LED, yellow_LED, purple_LED, blue_LED};

        // Configure SPI Pins
        P1SEL |= BIT2 + BIT4 + BIT6 + BIT7; // SIMO SPI on 1.2, CLK on 1.4; 1.6 and 1.7 are up/down buttons.
        P1DIR |= BIT2 + BIT4 + BIT6 + BIT7;
        P1SEL2 = BIT2 + BIT4;
        P1REN = BIT6 + BIT7; //Enable Pullup/down
        P1OUT = BIT6 + BIT7; //Select Pullup

        //Configure Watchdog timer
        BCSCTL3 |= LFXT1S_2;
        WDTCTL = WDT_ADLY_16;                     //WDT 16 ms, ACL, interval timer mode, stop WDT
        IE1 |= WDTIE;                             //Enable interrupt for WDT
        //IFG2 &= ~P2IFG;


        /*
         * UCB SPI setup
         */
        UCA0CTL1 |= UCSWRST; // DISABLE SPI
        UCA0CTL0 |= UCCKPH + UCMSB + UCMST + UCSYNC;
        UCA0CTL1 |= UCSSEL_1; // Source from ACLK/SMCLK?

        UCA0BR0 = 0x02;
        UCA0BR1 = 0;

        UCA0CTL1 &= ~UCSWRST; // Enable SPI


        /*
         * Timer A1 setup for PWM Piezzo buzzer.
         */
        TA1CCR0 = timer_period;
        TA1CCR1 = 0;                     //Timer A0 will count up to the timer period
        TA1CCR2 = timer_period / 2;                                //Volume
        TA1CCTL1 |= OUTMOD_7;
        TA1CTL = TASSEL_2 + MC_1 + ID_1;    //Source from SMCLK which is ~1 MHz


        /*
         * Set pin directions. P2.5 and 2.1 for buzzer
         */
        //P2DIR &= ~BIT0;
        P2DIR |= BIT1;
        P2SEL |= BIT1;
        P2REN = BIT0 + BIT2 + BIT3 + BIT4 + BIT5; //Enable Pullup/down
        P2OUT = BIT0 + BIT2 + BIT3 + BIT4; //Select Pullup
        P1REN |= BIT0;

        /*
         * Define data structures for the animation sequences.
         */

        //Set up with ADC for audio sampling.
//        ADC10CTL0 = MSC + ADC10SHT_3 + ADC10ON + ADC10IE + REFON + SREF_1;     //Sampling rate at 2x ADCLK (5 MHz), enable ADC10, enable interrupts
//        //ADC10CTL1 = INCH_0 + ADC10SSEL_1;  //Enable analog temperature sensor at P1.0
//       // ADC10CTL1 = INCH_1 | SHS_0 | ADC10DIV_1 | ADC10SSEL_3 | CONSEQ_2 ;
//        ADC10CTL1 = CONSEQ_0 | INCH_0 | SHS_0;
//        // Input channel 1 , trigger using ADC10SC bit , clock division by 2,
//        //internal ADC clock , single channel single conversions
//        //100 kHz?
//        //ADC10CTL1 = SHS_0 + INCH_0 + 0x1011 + CONSEQ_2; //Enable sampling at 1.0
//        ADC10AE0 = BIT1;    //Analog input enable
//        //ADC10SA = &samples[0]; //Tell ADC10 where to store the samples.
//
//        //Number of samples
//        ADC10DTC1 = N;

        /*
         * Tweaking ADC10 setup
         */

        // Disable ADC before configuration.
        ADC10CTL0 &= ~ENC;

        // Turn ADC on in single line before configuration.
        ADC10CTL0 = ADC10ON;

        // Make sure the ADC is not running per 22.2.7
        while(ADC10CTL1 & ADC10BUSY);

        // Repeat conversion.
        //ADC10DTC0 = ADC10CT;
        // Only one conversion at a time.
        ADC10DTC1 = N;
        // Put results at specified place in memory.
        ADC10SA = &samples[0];

        ADC10AE0 = BIT1;

        // 8 clock ticks, Use Reference, Reference on, ADC On, Multi-Sample Conversion, Interrupts enabled.
        ADC10CTL0 |= ADC10SHT_3 | SREF_1 | REFON  | ADC10IE | MSC;
        // Set channel, Use SMCLK, 1/8 Divider, Repeat single channel.
        ADC10CTL1 = INCH_0 | CONSEQ_2 | SHS_0;




        __bis_SR_register(GIE);

        while (1) {

            //In this mode, a button press makes the buzzer play the corresponding note.
            bool start_game = false;
            bool start_goertzel = false;
            MAX_DELAY = 50;

            while (!start_game && !start_goertzel) {
                //Detect four-button press game-start.
                if ((start_game = !(P2IN & BIT0) && !(P2IN & BIT2) && !(P2IN & BIT3) && !(P2IN & BIT4))) {
                        //Detect a button release.
                        while ((!(P2IN & BIT0) && !(P2IN & BIT2) && !(P2IN & BIT3) && !(P2IN & BIT4)));

                }

                //Detect whether the up/down buttons were pressed together. If so, start the Goertzel module.
                else if ((start_goertzel = !(P1IN & BIT6) && !(P1IN & BIT7))) {
                        while (!(P1IN & BIT6) && !(P1IN & BIT7));
                }

                //Detect whether up button was pressed.
                else if (!(P1IN & BIT6)) {
                    //Shift all notes up by one.
                    current_note_offset = (current_note_offset + 1 > NUM_NOTES) ? 0 : current_note_offset + 1;
                    PlaySound(buzzer_tones[NUM_NOTES - 1], 1);
                }

                //Detect whether down button was pressed.
                else if (!(P1IN & BIT7)) {
                    //Shift all notes down by one.
                    current_note_offset = (current_note_offset - 1 < 0) ? NUM_NOTES : current_note_offset - 1;
                    PlaySound(buzzer_tones[0], 1);
                }
                //Play A
                else if (!(P2IN & BIT0)) { // P2.0, Switch 0
                        //last_press = 0;
                    flashOneLED(LED_colors[0], default_brightness,0);

                    //A button was registered, now wait for release.
                    while (!(P2IN & BIT0));
                    flashOneLED(off_LED,LED_OFF, 5);


                    //Play C
                } else if (!(P2IN & BIT2)) {
                    //P2.2, Switch 1
                    flashOneLED(LED_colors[1], default_brightness,1);

                    //A button was registered, now wait for release.
                    while (!(P2IN & BIT2));
                    flashOneLED(off_LED,LED_OFF, 5);

                    //Play E
                } else if (!(P2IN & BIT3)) {
                    //P2.3, Switch 2

                    flashOneLED(LED_colors[2], default_brightness, 2);

                    //A button was registered, now wait for release.
                    while (!(P2IN & BIT3));
                    flashOneLED(off_LED,LED_OFF, 5);

                } //Play A
                else if (!(P2IN & BIT4)) {//P2.4, Switch 3
                    flashOneLED(LED_colors[3], default_brightness, 3);

                    while (!(P2IN & BIT4)) ;
                    flashOneLED(off_LED,LED_OFF, 5);


                }


            }

            //Start the tone-detection game.
            while (start_game) {

                    //Randomly determine a tone to test the player.
                    int tone = rand() % NUM_LED;

                    //Play the tone.
                    PlaySound(buzzer_tones[(current_note_offset + tone) % NUM_NOTES], 1);

                    //Wait for either a button press or MAX_DELAY to be reached.
                    //__bis_SR_register(LPM3_bits);


                    int button_pressed = -1;
                    num_delays = 0;

                        //Wait for a button press or time-out.
                        while (button_pressed == -1 && num_delays < MAX_DELAY) {
                            __bis_SR_register(LPM3_bits);

                                if (!(P2IN & BIT0)) { // P2.0, Switch 0
                                        //last_press = 0;
                                    flashOneLED(LED_colors[0], default_brightness,0);
                                    button_pressed = 0;


                                    //A button was registered, now wait for release.
                                    while (!(P2IN & BIT0));
                                    flashOneLED(off_LED,LED_OFF, 5);



                                } else if (!(P2IN & BIT2)) {
                                    //P2.2, Switch 1
                                    flashOneLED(LED_colors[1], default_brightness,1);
                                    button_pressed = 1;


                                    //A button was registered, now wait for release.
                                    while (!(P2IN & BIT2));
                                    flashOneLED(off_LED,LED_OFF, 5);
//


                                } else if (!(P2IN & BIT3)) {
                                    //P2.3, Switch 2

                                    flashOneLED(LED_colors[2], default_brightness, 2);
                                    button_pressed = 2;

                                    //A button was registered, now wait for release.
                                    while (!(P2IN & BIT3));
                                    flashOneLED(off_LED,LED_OFF, 5);

                                } else if (!(P2IN & BIT4)) {//P2.4, Switch 3
                                    flashOneLED(LED_colors[3], default_brightness, 3);
                                    button_pressed = 3;

                                    while (!(P2IN & BIT4)) ;
                                    flashOneLED(off_LED,LED_OFF, 5);


                                } else {
                                        //No button was pressed; check for delays.
                                        button_pressed = -1;
                                        num_delays++;
                                }
                        }


                        //Continue the game if no mistakes were made.
                        if (button_pressed == tone) {
                            //Play the winning sequence.
                            playAnimation(win, 16);
                            int loss_song[9] = {E_buzz, B_buzz, A_buzz, A_buzz, F_s_buzz, D_buzz, F_s_buzz, E_buzz, E_buzz};
                            playSong(loss_song, 9, 12);
                            continue;
                        }

                        //If the wrong button or no button was pressed, start over.
                        if (button_pressed != tone || num_delays > MAX_DELAY) {
                                //Play the losing sequence.
                                playAnimation(loss, 16);
                                int win_song[14] = {A_buzz, G_buzz, E_buzz, D_buzz, E_buzz, 0, D_buzz, E_buzz, A_buzz, B_buzz, A_buzz,
                                                    G_buzz, E_buzz, C_buzz};
                                playSong(win_song, 14, 12);
                                continue;
                        }


            }



           //Otherwise, start the goertzel module.
           while (start_goertzel) {


            //Loop to wait for button press to indicate particular tone tuning.
            // Enable conversion.
            ADC10CTL0 |= ENC;
            // Start conversion
            ADC10CTL0 |= ADC10SC;

            //ADC10CTL0 |= ENC + ADC10SC;     // Enable Conversion and conversion start
            __bis_SR_register(CPUOFF + GIE);

            highest_freq = A;
            //Do Goertzel to determine closest frequency component
            frequency_A = goertzel_mag(N, A, SAMPLE_RATE, &samples[0]);
            //__bis_SR_register(LPM3_bits);
            freq_guess = frequency_A;
           // __bis_SR_register(LPM3_bits);
            frequency_C = goertzel_mag(N, C, SAMPLE_RATE, &samples[0]);
            //__bis_SR_register(LPM3_bits);
            if (frequency_C > freq_guess) {
                freq_guess = frequency_C;
                highest_freq = C;
            }

            //__bis_SR_register(LPM3_bits);
            frequency_E = goertzel_mag(N, E, SAMPLE_RATE, &samples[0]);
            //__bis_SR_register(LPM3_bits);
            if (frequency_E > freq_guess) {
                freq_guess = frequency_E;
                highest_freq = E;
            }

           // __bis_SR_register(LPM3_bits);
            frequency_G = goertzel_mag(N, G, SAMPLE_RATE, &samples[0]);
            //__bis_SR_register(LPM3_bits);
            if (frequency_G > freq_guess) {
                //freq_guess = frequency_G;
                highest_freq = G;
            }
            final_guess = highest_freq;


            //#define A 440
            //#define E 758
            //#define C 956
            //#define G 638

            //__bis_SR_register(LPM3_bits);

            //For each of the target tones, do Goertzel. Determine component with highest magnitude.

            //Flash the corresponding LED


           }



        }

        return 0;
}


int goertzel_mag(int numSamples, int TARGET_FREQUENCY, int SAMPLING_RATE, int* data)
{
    int     k,i;

    unsigned int   q0, q1, q2, magnitude;

    //Pre-compute the 4 coefficients for the 4 tones.

    /*
     * coeff = 2 * cos(2*pi/numSamples) * (0.5 + numSamples*TARGET_FREQUENCY / SAMPLING_RATE)
     */
    //k = (0.5 + ((numSamples * TARGET_FREQUENCY) / SAMPLING_RATE));
    q0=0;
    q1=0;
    q2=0;

    for (i = 0; i < numSamples; i++)
    {
        if (TARGET_FREQUENCY == A)
            q0 = (q1 << 1) - q2 + data[i] - TWIDDLE; //About 2
        else if (TARGET_FREQUENCY == C)
            //coeff = 3.299; //About 10/3 or 40/12 or 27/8
            q0 = (((q1 << 4) + (q1 << 3) + (q1 << 1) + q1) >> 3) - q2 + data[i] - TWIDDLE;

        else if (TARGET_FREQUENCY == E)
            //coeff = 2.8153; //About 20/7, approximate as 20/8
            q0 = (((q1 << 4) + (q1 << 2)) >> 3) - q2 + data[i] - TWIDDLE;
        else if (TARGET_FREQUENCY == G)
            q0 = (((q1 << 2) + q1) >> 1) - q2 + data[i] - TWIDDLE; //About 5/2

        q2 = q1;
        q1 = q0;
    }

    //Compute the magnitude squared.
    magnitude = q1*q1 + q2*q2;
    int product = q1*q2;
    if (TARGET_FREQUENCY == A)
        magnitude -= (product << 1); //About 2
        else if (TARGET_FREQUENCY == C)
            //coeff = 3.299; //About 10/3 or 40/12 or 27/8
            magnitude -= ((product << 4) + (product << 3) + (product << 1) + product) >> 3;

        else if (TARGET_FREQUENCY == E)
            //coeff = 2.8153; //About 20/7, approximate as 20/8
            magnitude -= ((product << 4) + (product << 2)) >> 3;
        else if (TARGET_FREQUENCY == G)
            magnitude -= ((product << 2) + product) >> 1; //About 5/2
    return magnitude >> 2;
}

/*
 * Generate a random button sequence of length (length).
 */
static int *
generateRandomButtonSequence(int length)
{
        int *sequence = malloc(sizeof(int) * length);
        int *sequence_ptr = sequence;

        int i;
        for (i = 0; i < length; i++) {
                *sequence_ptr = rand() % NUM_LED;
                sequence_ptr++;

        }

        return sequence;

}

/*
 *
 * Requires:
 *      LEDs in a pointer to an array of RGB structs.
 *      length is the length of the array.
 * Effects:
 *      Plays an animation that involves the specified LEDs.
 */

void
playAnimation(struct RGB LEDs[], int length)
{
        struct RGB LED;
        int i, j;

        for (i = 0; i < length; i+= NUM_LED) {
            //Every NUM_LED iterations, signal the start of the pattern.
            startPattern();
            for (j = 0; j < NUM_LED; j++)
                flashPattern(LEDs[i]);

            endPattern();

        }



}

/*
 * Requires:
 *      sequence is a pointer to an array of ints, representing which LEDs [0, NUM_LED]
 *      to flash.
 *      length is the length of the sequence.
 * Effects:
 *      Flashes the pattern of LEDs with accompanying buzzer tones.
 */
void
playSequence(int *sequence, int length)
{

        int i;

        //Start signal
        startPattern();

        for (i = 0; i < length; i++) {
                __bis_SR_register(LPM3_bits);

                //Detect which LED should be flashed.
                switch(sequence[i]) {
                        case 0: {
                                flashOneLED(green_LED, default_brightness, 0);
                                flashOneLED(off_LED, LED_OFF, NUM_LED);
                                break;
                        }

                        case 1: {
                                flashOneLED(yellow_LED, default_brightness, 1);
                                flashOneLED(off_LED, LED_OFF, NUM_LED);
                                break;
                        }
                        case 2: {
                                flashOneLED(purple_LED, default_brightness, 2);
                                flashOneLED(off_LED, LED_OFF, NUM_LED);
                                break;
                        }

                        default: {
                                flashOneLED(blue_LED, default_brightness, 3);
                                flashOneLED(off_LED, LED_OFF, NUM_LED);
                                break;
                        }

                }
        }


        //PlaySound(rest, 1);
        endPattern();


}

/*
 * Requires:
 *      LED is a RGB struct describing the state of an LED.
 *      brightness is the desired brightness of the LED.
 *
 * Effect:
 *      Sends the LED configuration through SPI.
 */
static void
flashPattern(struct RGB LED)
{
        IE2 |= UCA0TXIE;
        __bis_SR_register(LPM3_bits);


        //Set LED brightness.
        UCA0TXBUF = LED.brightness;
        __bis_SR_register(LPM3_bits);

        //Set LED blue value
        UCA0TXBUF = LED.blue;
        __bis_SR_register(LPM3_bits);

        //Set LED green value
        UCA0TXBUF = LED.green;
        __bis_SR_register(LPM3_bits);

        //Set LED red value
        UCA0TXBUF = LED.red;
        __bis_SR_register(LPM3_bits);

        IE2 &= ~UCB0TXIE;

}

/*
 * Requires:
 *      Nothing
 * Effects:
 *      Signals the beginning of the LED SPI sequence.
 */
static void
startPattern()
{
        int i;
        IE2 |= UCA0TXIE;

        //Start signal
        for (i = 0; i < 4; i++) {
                __bis_SR_register(LPM3_bits);
                UCA0TXBUF = 0x00;

        }

        IE2 &= ~UCB0TXIE;
}

/*
 * Requires:
 *      Nothing
 * Effects:
 *      Signals the end of the LED SPI sequence.
 */
static void
endPattern()
{
        int i;
        IE2 |= UCA0TXIE;
        //End signal
        for (i = 0; i < 4; i++) {
                __bis_SR_register(LPM3_bits);
                UCA0TXBUF = 0xFF;
        }

        IE2 &= ~UCB0TXIE;


}

/*
 * Requires:
 *      LED is an RGB structure.
 *      brightness is the desired brightness of the specified LED.
 *      LED_num is the number LED [0, NUM_LED] to be flashed.
 * Effects:
 *      Flashes the indicated LED and plays the corresponding buzzer tone.
 */
static void
flashOneLED(struct RGB LED, unsigned char brightness, int LED_num)
{
        int j;
        startPattern();

        for (j = 0; j < NUM_LED; j++) {
                IE2 |= UCA0TXIE;



                /*
                * Determine the RGB vector for this LED.
                */

                __bis_SR_register(LPM3_bits);


                IE2 &= ~UCA0TXIE;

                //Only LED_num will display this color
                unsigned int this_brightness = (j == LED_num) ? brightness : 0xE0;


                IE2 |= UCA0TXIE;

                //Set LED brightness.
                UCA0TXBUF = this_brightness;

                __bis_SR_register(LPM3_bits);

                //Set LED blue value
                UCA0TXBUF = LED.blue;
                __bis_SR_register(LPM3_bits);

                //Set LED green value
                UCA0TXBUF = LED.green;
                __bis_SR_register(LPM3_bits);

                //Set LED red value
                UCA0TXBUF = LED.red;
                __bis_SR_register(LPM3_bits);

                IE2 &= ~UCA0TXIE; //Disable interrupt

        }

        endPattern();

        //Play a buzzer sound if the specified LED index is on the board.
        if (brightness != LED_OFF && LED_num < NUM_LED)
                PlaySound(buzzer_tones[(LED_num + current_note_offset) % NUM_NOTES], 1);


}

/*
 * Given a sequence of notes, play the notes with the Piezzo buzzer.
 *
 * Requires:
 *      song is the array of notes.
 *      length is the length of the song array.
 *      duration is how long each note in the array should be held for.
 *
 */
void playSong(int song[], int length, int duration)
{
    int j;
    for (j = 0; j < length; j++)
        PlaySound(song[j], duration);
}

/*
 * Requires:
 *      note is the tone to be played.
 *      duration is how long the tone should be played.
 * Effects:
 *      Plays the specified note with the piezzo buzzer.
 */
void PlaySound(int note, int duration) {
        if (note != rest) {
                TA1CCR0 = note;                     //Timer A0 will count up to the timer period
                TA1CCR1 = note;              //Volume

        }

        int i;
        for (i = 0; i < duration; i++)
                __delay_cycles(50000);

        TA1CCR0 = 0;
        //TA1CCR2 = 0;
        TA1CCR1 = 0;
        //__delay_cycles(1000);             //Increasing this value with slow the note tempo.


}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=USCIAB0TX_VECTOR
__interrupt void USCIAB0TX_ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(USCIAB0TX_VECTOR))) USCIAB0TX_ISR (void)
#else
#error Compiler not supported!
#endif
{
        IFG2 &= ~UCA0TXIFG;
        __bic_SR_register_on_exit(LPM3_bits);        // Return to active mode
}

// Watchdog Timer interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=WDT_VECTOR
__interrupt void watchdog_timer(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(WDT_VECTOR))) watchdog_timer (void)
#else
#error Compiler not supported!
#endif
{
        //num_delays++;

        __bic_SR_register_on_exit(LPM3_bits);
}

#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=TIMER1_A0_VECTOR
__interrupt void Timer1A0(void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(WDT_VECTOR))) watchdog_timer (void)
#else
#error Compiler not supported!
#endif
{


        __bic_SR_register_on_exit(LPM3_bits);

}

// ADC10 interrupt service routine
#if defined(__TI_COMPILER_VERSION__) || defined(__IAR_SYSTEMS_ICC__)
#pragma vector=ADC10_VECTOR
__interrupt void ADC10_ISR (void)
#elif defined(__GNUC__)
void __attribute__ ((interrupt(ADC10_VECTOR))) ADC10_ISR (void)
#else
#error Compiler not supported!
#endif
{
        __bic_SR_register_on_exit(LPM3_bits);        // Return to active mode
}
