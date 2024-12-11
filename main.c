#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "rs232.h"
#include "serial.h"

#define BAUD_RATE 115200 // Communication baud rate
#define LINE_WIDTH 100   // Width of each line for text placement
#define SCALE_MIN 4      // Minimum allowed scaling factor
#define SCALE_MAX 10     // Maximum allowed scaling factor
#define LINE_COUNT 1027  // Number of lines in the font data file
#define CHAR_WIDTH 18.0F // Width of each character in the font
#define LINE_SPACING -5  // Vertical spacing between lines

// Function to send commands to the robot
void SendCommands(char *buffer);

// Struct to hold font data for each character
typedef struct
{
    float Xposition;
    float Yposition;
    int Zposition;
} DataEntry;

// Function to get a valid scaling factor from the user
float get_scale_factor()
{
    float scale;
    int ch; // Used to clear the input buffer
    while (1)
    {
        printf("Enter a scaling factor between %d and %d: ", SCALE_MIN, SCALE_MAX);
        if (scanf("%f", &scale) == 1 && scale >= SCALE_MIN && scale <= SCALE_MAX)
        {
            return scale / CHAR_WIDTH; // Scale factor for character width adjustment
        }
        printf("Invalid input! Please enter a value between %d and %d.\n", SCALE_MIN, SCALE_MAX);
        while ((ch = getchar()) != '\n' && ch != EOF)
            ; // Clear input buffer
    }
}

// Function to calculate the width of a word
float calculate_word_width(const char *word, float scaleFactor)
{
    return (float)strlen(word) * CHAR_WIDTH * scaleFactor; // Width based on scaled character width
}

// Function to check if a word fits in the remaining line space
int fits_in_line(double *remaining_space, float word_width)
{
    if (*remaining_space >= word_width)
    {
        *remaining_space -= word_width; // Update remaining space
        return 1;                       // Word fits
    }
    return 0; // Word doesn't fit
}

// Function to open a file and handle errors
FILE *open_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (!file)
    {
        printf("Error opening file: %s\n", filename);
    }
    return file;
}

// Function to find the stroke data for a specific character
DataEntry *find_character_data(char character, DataEntry *fontData, int *stroke_count)
{
    int ascii_val = (int)character; // Get ASCII value of the character
    for (int i = 0; i < LINE_COUNT; i++)
    {
        if (fontData[i].Xposition == 999 && fontData[i].Yposition == ascii_val)
        {
            *stroke_count = fontData[i].Zposition; // Get stroke count
            return &fontData[i + 1];               // Return pointer to the first stroke data
        }
    }
    return NULL; // Character data not found
}

// Function to generate G-code commands for a word
void generate_gcode_for_word(const char *word, DataEntry *fontData, float scaleFactor, float *current_Xpos, float current_Ypos)
{
    char buffer[4000] = ""; // Buffer for G-code commands
    for (int i = 0; word[i]; i++)
    { // Process each character in the word
        int stroke_count;
        DataEntry *charData = find_character_data(word[i], fontData, &stroke_count);
        if (charData)
        {
            for (int j = 0; j < stroke_count; j++)
            { // Generate G-code for each stroke
                float scaledX = (charData[j].Xposition * scaleFactor) + *current_Xpos;
                float scaledY = (charData[j].Yposition * scaleFactor) + current_Ypos;
                sprintf(buffer, "S%d\nG%c X%.2f Y%.2f\n",
                        charData[j].Zposition ? 1000 : 0,  // Pen state
                        charData[j].Zposition ? '1' : '0', // Move type
                        scaledX, scaledY);
                SendCommands(buffer); // Send G-code command
            }
        }
        else
        {
            printf("Character '%c' - Stroke data not found.\n", word[i]);
        }
        *current_Xpos += CHAR_WIDTH * scaleFactor; // Advance to next character position
    }
}

// Function to reset position for a new line
void reset_position(float *current_Xpos, float *current_Ypos, float scaleFactor, double *remaining_space)
{
    *current_Xpos = 0;                                        // Reset X-position
    *current_Ypos += LINE_SPACING - CHAR_WIDTH * scaleFactor; // Move to the next line
    *remaining_space = LINE_WIDTH;                            // Reset remaining space for the new line
    char buffer[100];
    sprintf(buffer, "G0 X%.2f Y%.2f\n", *current_Xpos, *current_Ypos);
    SendCommands(buffer); // Send command to move to the new line
}

int main()
{
    if (CanRS232PortBeOpened() == -1)
    {
        printf("Unable to open the COM port (specified in serial.h).\n");
        return 1;
    }

    printf("Initializing robot...\n");
    SendCommands("\n"); // Wake up robot
    PrintBuffer("\n");
    Sleep(100);
    WaitForDollar(); // Wait for the robot to signal readiness
    printf("Robot ready to draw.\n");

    // Set initial robot state
    SendCommands("G1 X0 Y0 F1000\n");
    SendCommands("M3\n");
    SendCommands("S0\n");

    // Load font data
    FILE *fontFile = open_file("SingleStrokeFont.txt");
    if (!fontFile)
        return 1;

    DataEntry fontData[LINE_COUNT];
    for (int i = 0; i < LINE_COUNT; i++)
    {
        fscanf(fontFile, "%f %f %d", &fontData[i].Xposition, &fontData[i].Yposition, &fontData[i].Zposition);
    }
    fclose(fontFile);

    // Get scale factor from user
    float scaleFactor = get_scale_factor();
    printf("Scale factor: %f\n", scaleFactor);

    // Open input file for text
    char inputFilename[200];
    printf("Enter the name of the text file: ");
    scanf("%199s", inputFilename);
    FILE *inputFile = open_file(inputFilename);
    if (!inputFile)
        return 1;

    // Initialize positions and space tracker
    double remaining_space = LINE_WIDTH;
    float current_Xpos = 0, current_Ypos = LINE_SPACING - CHAR_WIDTH * scaleFactor;

    // Process each word from the input file
    char word[100];
    while (fscanf(inputFile, "%99s", word) != EOF)
    {
        float wordWidth = calculate_word_width(word, scaleFactor);
        if (!fits_in_line(&remaining_space, wordWidth))
        {
            reset_position(&current_Xpos, &current_Ypos, scaleFactor, &remaining_space); // New line
        }
        generate_gcode_for_word(word, fontData, scaleFactor, &current_Xpos, current_Ypos); // G-code for word
        current_Xpos += CHAR_WIDTH * scaleFactor;                                          // Space after the word
        remaining_space -= CHAR_WIDTH * scaleFactor;
    }

    // Finish by returning to the origin
    SendCommands("G1 X0 Y0\n");
    CloseRS232Port();
    printf("COM port closed.\n");
    return 0;
}

// Function to send commands to the robot
void SendCommands(char *buffer)
{
    PrintBuffer(&buffer[0]); // Send buffer to robot
    WaitForReply();          // Wait for robot acknowledgment
    Sleep(100);              // Pause briefly
}
