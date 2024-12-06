#include <stdio.h>
#include <stdlib.h>
#include <string.h>
// #include <conio.h>
// #include <windows.h>
#include "rs232.h"
#include "serial.h"

#define bdrate 115200  /* 115200 baud */
#define LINE_WIDTH 100 // Width of each line

void SendCommands(char *buffer);

#define LINE_COUNT 1027

// Define struct to hold data from each line
typedef struct
{
    float Xposition;
    float Yposition;
    int Zposition;
} DataEntry;

float get_scale_factor()
{
    float scale;
    int cha;

    do
    {
        printf("Enter a scaling factor between 4 and 10: ");
        if (scanf("%f", &scale) != 1)
        { // Check if the input is not a float
            printf("Invalid input! Please enter a value between 4 and 10.\n");

            // Clear the buffer to remove invalid input
            while ((cha = getchar()) != '\n' && cha != EOF)
                ;
        }
        else if (scale < 4 || scale > 10)
        { // Check if the number is out of range
            printf("Invalid input! Please enter a value between 4 and 10.\n");
        }
    } while (scale < 4 || scale > 10);

    return scale / 18;
}
// Function to calculate the width of a word
float word_width(const char *word, float scaleFactor)
{
    int length = (int)strlen(word);             // Calculate the length of the word
    return (float)length * 18.0F * scaleFactor; // Width calculation
}
// Function to check if the word fits in the remaining space on the line
int space_remaining(double *remaining_space, double word_width)
{
    if (*remaining_space >= word_width)
    {
        *remaining_space -= word_width; // Subtract word width from remaining space

        return 1; // Word fits
    }
    else
    {
        return 0; // Word doesn't fit
    }
}
// Function to open a file and return the file pointer
FILE *open_file(const char *filename)
{
    FILE *file = fopen(filename, "r");
    if (file == NULL)
    {
        printf("Error opening file: %s\n", filename);
    }
    return file;
}
// Function to locate character's stroke data
DataEntry *find_character_data(char character, DataEntry *SingleStrokeData, int *stroke_count)
{
    int ascii_val = (int)character;
    for (int i = 0; i < LINE_COUNT; i++)
    {
        if (SingleStrokeData[i].Xposition == 999 && SingleStrokeData[i].Yposition == ascii_val)
        {
            *stroke_count = SingleStrokeData[i].Zposition; // Number of strokes for this character
            return &SingleStrokeData[i + 1];               // Return pointer to the stroke data start
        }
    }
    return NULL; // Character not found
}
// Function to convert stroke data to G-code commands for a word
void convert_word_to_gcode(const char *word, DataEntry *SingleStrokeData, float scaleFactor, float *current_Xpos, float current_Ypos)
{
    char buffer[4000] = ""; // Buffer to hold G-code commands for the entire word
    char temp[200];         // Temporary buffer for individual commands

    // Process each character in the word
    for (int i = 0; word[i] != '\0'; i++)
    {
        int stroke_count;
        DataEntry *charData = find_character_data(word[i], SingleStrokeData, &stroke_count);

        if (charData != NULL)
        {
            for (int j = 0; j < stroke_count; j++)
            {
                float scaledX = (charData[j].Xposition * scaleFactor) + *current_Xpos;
                float scaledY = (charData[j].Yposition * scaleFactor) + current_Ypos;

                if (charData[j].Zposition == 0)
                {
                    sprintf(temp, "S0\nG0 X%.2f Y%.2f\n", scaledX, scaledY); // Pen up
                }
                else
                {
                    sprintf(temp, "S1000\nG1 X%.2f Y%.2f\n", scaledX, scaledY); // Pen down
                }

                strcat(buffer, temp); // Append the command to the word buffer
            }
        }
        else
        {
            printf("Character: %c - Stroke data not found.\n", word[i]);
        }

        *current_Xpos += 18 * scaleFactor; // Increment X-position for the next character
    }

    // Send all the G-code for the word at once
    SendCommands(buffer);
}
int main()
{
    // char mode[]= {'8','N','1',0};
    char buffer[100];

    // If we cannot open the port then give up immediately
    if (CanRS232PortBeOpened() == -1)
    {
        printf("\nUnable to open the COM port (specified in serial.h) ");
        exit(0);
    }

    // Time to wake up the robot
    printf("\nAbout to wake up the robot\n");

    // We do this by sending a new-line
    sprintf(buffer, "\n");
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer(&buffer[0]);
    Sleep(100);

    // This is a special case - we wait  until we see a dollar ($)
    WaitForDollar();

    printf("\nThe robot is now ready to draw\n");

    // These commands get the robot into 'ready to draw mode' and need to be sent before any writing commands
    sprintf(buffer, "G1 X0 Y0 F1000\n");
    SendCommands(buffer);
    sprintf(buffer, "M3\n");
    SendCommands(buffer);
    sprintf(buffer, "S0\n");
    SendCommands(buffer);

    // Open the font data file
    FILE *file = fopen("SingleStrokeFont.txt", "r");
    if (file == NULL)
    {
        printf("Error opening file.\n");
        return 1;
    }
    // Load font data into an array of structs
    DataEntry SingleStrokeData[LINE_COUNT];
    for (int i = 0; i < LINE_COUNT; i++)
    {
        fscanf(file, "%f %f %d", &SingleStrokeData[i].Xposition, &SingleStrokeData[i].Yposition, &SingleStrokeData[i].Zposition);
    }
    fclose(file);

    // Get scaling factor from the user
    float scaleFactor = get_scale_factor();
    printf("Scale factor: %f\n", scaleFactor);

    // Prompt for and open the input text file
    char filename[200];
    printf("Enter the name of the text file: ");
    scanf("%199s", filename);

    FILE *file2 = open_file(filename);
    if (file2 == NULL)
    {
        return 1;
    }

    double remaining_space = LINE_WIDTH;
    float current_Xpos = 0;                     // Initialize X-position tracker
    float current_Ypos = -5 - 18 * scaleFactor; // Initialize Y-position

    // Read and process each word using fscanf
    char word[100];
    while (fscanf(file2, "%99s", word) != EOF)
    {
        // Calculate the width of the current word
        double wordWidth = word_width(word, scaleFactor);

        // Check if the word fits in the remaining space
        if (!space_remaining(&remaining_space, wordWidth))
        {
            // Move to a new line
            current_Xpos = 0;                         // Reset X-position to the start of the line
            current_Ypos += -5 - 18 * scaleFactor;    // Move down to the next line
            remaining_space = LINE_WIDTH - wordWidth; // Reset remaining space for the new line

            // Add G-code to move to the new line position
            char moveToNewLine[100];
            sprintf(moveToNewLine, "G0 X%.2f Y%.2f\n", current_Xpos, current_Ypos);
            SendCommands(moveToNewLine);
        }

        // Generate G-code for the current word
        convert_word_to_gcode(word, SingleStrokeData, scaleFactor, &current_Xpos, current_Ypos);

        // Add space after the word
        current_Xpos += 18 * scaleFactor;    // Increment X-position by the space width
        remaining_space -= 18 * scaleFactor; // Deduct space width from remaining line space
    }
}

// Send the data to the robot - note in 'PC' mode you need to hit space twice
// as the dummy 'WaitForReply' has a getch() within the function.
void SendCommands(char *buffer)
{
    // printf ("Buffer to send: %s", buffer); // For diagnostic purposes only, normally comment out
    PrintBuffer(&buffer[0]);
    WaitForReply();
    Sleep(100); // Can omit this when using the writing robot but has minimal effect
                // getch(); // Omit this once basic testing with emulator has taken place
}
