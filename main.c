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
    int Xposition;
    int Yposition;
    int Zposition;
} DataEntry;

float get_scale_factor()
{
    float scale;
    char cha;

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
    } while (scale <= 4 || scale >= 10);

    return scale / 18.0;
}
// Function to calculate the width of a word
double word_width(const char *word, double scaleFactor)
{
    int length = strlen(word);        // Calculate the length of the word
    return length * 18 * scaleFactor; // Width calculation
}
// Function to check if the word fits in the remaining space on the line
int space_remaining(double *remaining_space, double word_width)
{
    if (*remaining_space >= word_width)
    {
        *remaining_space -= word_width; // Subtract word width from remaining space
        printf("Space remaining: %.2f\n", *remaining_space);
        return 1; // Word fits
    }
    else
    {
        printf("No space remaining for this word. Starting new line.\n");
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

int main()
{
    FILE *file = fopen("SingleStrokeFont.txt", "r");
    if (file == NULL)

    {
        printf("Error opening file.\n");
        return 1;
    }

    // Declare array of structs
    DataEntry SingleStrokeData[LINE_COUNT];

    // Read the file and populate the array
    for (int i = 0; i < LINE_COUNT; i++)
    {
        fscanf(file, "%d %d %d", &SingleStrokeData[i].Xposition, &SingleStrokeData[i].Yposition, &SingleStrokeData[i].Zposition);
    }
    fclose(file);

    float scaleFactor = get_scale_factor();
    printf("Scale factor: %f\n", scaleFactor);

    {
        char filename[200]; // Buffer to store the file name

        // Prompt the user for the file name
        printf("Enter the name of the text file: ");
        scanf("%99s", filename); // Limit input size to prevent buffer overflow

        // Open the file using the new function
        FILE *file2 = open_file(filename);
        if (file2 == NULL)
        {
            return 1; // Exit the program if the file couldn't be opened
        }

        int capacity = 10;                                    // Initial size of the dynamic array
        char *word = (char *)malloc(capacity * sizeof(char)); // Allocate memory for the word
        if (word == NULL)
        {
            printf("Memory allocation failed.\n");
            fclose(file2);
            return 1;
        }

        int index = 0; // Position in the dynamic array
        char ch;
        double remaining_space = LINE_WIDTH; // Initialize remaining space for the line

        // Read characters one by one from the file
        while ((ch = fgetc(file2)) != EOF)
        {
            if (ch == 32 || ch == '\n')
            { // Check for space or newline (end of word)
                if (index > 0)
                {                               // Only process non-empty words
                    word[index] = '\0';         // Null-terminate the word
                    printf("Word: %s\n", word); // Print the word

                    // Calculate and print the width of the word
                    double wordWidth = word_width(word, scaleFactor);
                    // Check if the word fits
                    if (!space_remaining(&remaining_space, wordWidth))
                    {
                        // If word doesn't fit, reset the space and start a new line
                        remaining_space = LINE_WIDTH - wordWidth;
                        printf("New line started. Remaining space: %.2f\n", remaining_space);
                    }
                    index = 0;
                }
            }
            else
            {
                // Expand the dynamic array if necessary
                if (index >= capacity - 1)
                {
                    capacity *= 2; // Double the capacity
                    word = (char *)realloc(word, capacity * sizeof(char));
                    if (word == NULL)
                    {
                        printf("Memory reallocation failed.\n");
                        fclose(file2);
                        return 1;
                    }
                }
                word[index++] = ch; // Add the character to the word array
            }
        }

        // Print the last word if the file doesn't end with a space
        if (index > 0)
        {
            word[index] = '\0';
            printf("Word: %s\n", word);
            // Calculate and print the width of the last word
            double wordWidth = word_width(word, scaleFactor);
            if (!space_remaining(&remaining_space, wordWidth))
            {
                printf("Word doesn't fit in the remaining space. Starting new line.\n");
            }
        }

        // Clean up
        free(word);
        fclose(file2);

        printf("\nFile reading complete.\n");
        return 0;
    }
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

    // These are sample commands to draw out some information - these are the ones you will be generating.
    sprintf(buffer, "G0 X-13.41849 Y0.000\n");
    SendCommands(buffer);
    sprintf(buffer, "S1000\n");
    SendCommands(buffer);
    sprintf(buffer, "G1 X-13.41849 Y-4.28041\n");
    SendCommands(buffer);
    sprintf(buffer, "G1 X-13.41849 Y0.0000\n");
    SendCommands(buffer);
    sprintf(buffer, "G1 X-13.41089 Y4.28041\n");
    SendCommands(buffer);
    sprintf(buffer, "S0\n");
    SendCommands(buffer);
    sprintf(buffer, "G0 X-7.17524 Y0\n");
    SendCommands(buffer);
    sprintf(buffer, "S1000\n");
    SendCommands(buffer);
    sprintf(buffer, "G0 X0 Y0\n");
    SendCommands(buffer);

    // Before we exit the program we need to close the COM port
    CloseRS232Port();
    printf("Com port now closed\n");

    return (0);
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
