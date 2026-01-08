#ifndef _CONFIG_H
#define _CONFIG_H

void Diagnose(void);
void PrintDate(uint8_t row, uint8_t col);
void About(void);
void Profiles(void);
void LoadProfile(uint8_t profile);
void SaveProfile(uint8_t profile);
void SaveScreenshot(void);
void ShowScreenshot(void);
void OWSettings(void);

#endif