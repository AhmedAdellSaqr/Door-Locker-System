/*
 * main.c
 *
 * Created: 8/13/2022 11:16:21 PM
 *  Author: Amr Samy
 */ 
#define F_CPU	8000000UL
#include <util/delay.h>

/* MCAL Modules Inclusions */
#include "DIO/DIO_interface.h"
#include "MCAL/SPI/SPI_interface.h"
#include "MCAL/EEPROM/EEPROM_interface.h"

/* HAL Modules Inclusions */
#include "HAL/LCD/LCD_interface.h"
#include "HAL/KEYPAD/KEYPAD_interface.h"
 
 #define OPTION_1	1
 #define OPTION_2	2

/* ======================= Enums ====================== */

typedef enum
{
	NONE,
	DOOR_OPEN,
	DOOR_CLOSE,
	BUZZER_ALERT,
	BUZZER_TONE
}State_t;

typedef enum
{
	SYSTEM_LOCKED,
	SYSTEM_UNLOCKED,
	SYSTEM_BLOCK,
}SystemState_t;

SystemState_t SystemState = SYSTEM_LOCKED;

/* ======================= Functions Prototypes ====================== */
u8 ChangePassword(void);
void GettingPassword(void);
u8 GettingOldPassword(void);
u8 GettingOption(void);

/* ======================= Global Variables ====================== */
/* Current Password: 1972 */
u16 Password;
u8 B0_PASS,B1_PASS;

u16 NewPassword = 0;
u16 EnteredPassword = 0;

u8 Key;
u8 FirstFlag = 0;
u8 Ret;

/* Controller Code */
/* Master */

int main(void)
{	
	/* Initializing DIO , LCD and UART */
	DIO_InitAllPins();
	LCD_Init();
	SPI_Init(MASTER);
	
	/* Loading Password from EEPROM */
	B0_PASS = EEPROM_Read(0x000);
	B1_PASS = EEPROM_Read(0x001);
	Password = ((u16)B0_PASS) | ((u16)(B1_PASS<<8));
	
	/* Loading System State from EEPROM */
	SystemState = EEPROM_Read(0x002);
	
	//LCD_GoTo(3,0);
	//LCD_WriteNumber_4D(Password);
	
	/* Printing APP Info */
	LCD_GoToWriteString(0,0,"Door Locker Security");
	LCD_GoToWriteString(1,5,"System");
	_delay_ms(1000);
	LCD_ClearDisplay();
	
	while(1)
	{
		if(SystemState == SYSTEM_LOCKED)
		{
			/* 1- Asking the user for password */
			GettingPassword();
		}
		
		if(SystemState == SYSTEM_BLOCK)
		{
			LCD_GoToWriteString(2,0,"Door Locked 5 Minutes");
			SPI_SendNoBlock(BUZZER_ALERT);
			EEPROM_Write(0x002,SYSTEM_BLOCK);
			_delay_ms(5000);
			/* Switch System State after 5 seconds */
			SystemState = SYSTEM_LOCKED;
			EEPROM_Write(0x002,SYSTEM_LOCKED);
			continue;
		}
		
		if(SystemState == SYSTEM_UNLOCKED)
		{
			if(FirstFlag == 0)
			{
				/* Setting the First Boot flag */
				FirstFlag = 1;
				/* Open the door */
				/* Sending Command to the Controller */
				SPI_SendNoBlock(DOOR_OPEN);
				LCD_GoToWriteString(2,0,"Welcome to your home");
				_delay_ms(2000);
				SPI_SendNoBlock(DOOR_CLOSE);
			}
			
			/* Main Menu */
			LCD_ClearDisplay();
			LCD_GoToWriteString(0,0,"Locker System Menu");
			LCD_GoToWriteString(1,0,"1-Change Password");
			LCD_GoToWriteString(2,0,"2-Open Door");
			
			Ret = GettingOption();
			
			if(Ret == OPTION_1)
			{
				if(ChangePassword())
				{
					LCD_GoToWriteString(0,0,"Password Changed");
					LCD_GoToWriteString(1,0,"Successfully");
					_delay_ms(500);
				}
			}
			else if(Ret == OPTION_2)
			{
				/* 2- Open the door */
				/* Sending Command to the Controller */
				SPI_SendNoBlock(DOOR_OPEN);
				_delay_ms(2000);
				SPI_SendNoBlock(DOOR_CLOSE);
			}
		}
	}
}

u8 ChangePassword(void)
{
	u8 Counter = 0;
	
	/* Clearing Display */
	LCD_ClearDisplay();
	
	/* Enter Old Password */
	u8 Ret = GettingOldPassword();
	if(Ret == 1)
	{
		LCD_GoToWriteString(0,0,"Enter New Password");
		while(1)
		{
			/* Getting Pressed Key */
			Key = KEYPAD_GetKey();
			
			if(Key != NO_KEY)
			{
				if(Key == '=')
				{
					if(NewPassword == Password)
					{
						LCD_ClearDisplay();
						LCD_GoToWriteString(0,0,"The old and new");
						LCD_GoToWriteString(1,0,"password are the same");
						_delay_ms(1000);
						LCD_ClearDisplay();
						LCD_GoToWriteString(0,0,"Enter New Password");
						NewPassword = 0;
						Counter = 0;
						continue;
					}
					LCD_ClearDisplay();
					Password = NewPassword;
					B0_PASS = (u8)Password;
					B1_PASS = (u8)(Password>>8);
					EEPROM_Write(0x000,B0_PASS);
					EEPROM_Write(0x001,B1_PASS);
					
					return 1;
				}
				else
				{
					LCD_GoToWriteChar(3,Counter,Key);
					_delay_ms(200);
					LCD_GoToWriteChar(3,Counter,'*');
					Counter++;
					NewPassword = (NewPassword*10) + (Key-'0');
				}
			}
		}
	}
	else if(Ret == 0)
	{
		LCD_GoToWriteString(2,0,"Door Locked 5 Minutes");
		SPI_SendNoBlock(BUZZER_ALERT);
		_delay_ms(1000);
	}
}

void GettingPassword(void)
{	
	u8 TrialCounter = 0;
	u8 Counter = 0;
	
	/* Clearing Display */
	LCD_ClearDisplay();
	/* Asking User to Enter Password */
	LCD_GoToWriteString(0,0,"Enter the password:");
	
	while(1)
	{
		if(TrialCounter == 3)
		{
			LCD_ClearDisplay();
			SystemState = SYSTEM_BLOCK;
			break;
		}
		
		/* Getting Pressed Key */
		Key = KEYPAD_GetKey();
		
		if(Key != NO_KEY)
		{
			if(Key == '=')
			{
				LCD_ClearDisplay();
				if(EnteredPassword == Password)
				{
					EnteredPassword = 0;
					LCD_GoToWriteString(2,3,"Correct Password");
					_delay_ms(1000);
					/* Clearing the display */
					LCD_ClearDisplay();
					
					SystemState = SYSTEM_UNLOCKED;
					break;
				}
				else
				{
					LCD_GoToWriteString(2,3,"Wrong Password");
					SPI_SendNoBlock(BUZZER_TONE);
					_delay_ms(1000);
					TrialCounter++;
					/* Clearing the display */
					LCD_ClearDisplay();
					LCD_GoToWriteString(0,0,"Enter the password:");
					LCD_GoToWriteString(1,0,"Trials:");
					LCD_GoToWriteNumber(1,8,TrialCounter);
					/* Resetting the counter and Entered Password */
					Counter = 0;
					EnteredPassword = 0;
				}
			}
			else
			{
				LCD_GoToWriteChar(3,Counter,Key);
				_delay_ms(200);
				LCD_GoToWriteChar(3,Counter,'*');
				Counter++;
				EnteredPassword = (EnteredPassword*10) + (Key-'0');
			}
		}
	}
}

u8 GettingOldPassword(void)
{
	static u8 TrialCounter;
	u8 Counter = 0;
	
	/* Clearing Display */
	LCD_ClearDisplay();
	/* Asking User to Enter Password */
	LCD_GoToWriteString(0,0,"Enter old password:");
	
	while(1)
	{
		if(TrialCounter == 3)
		{
			LCD_ClearDisplay();
			SystemState = SYSTEM_BLOCK;
			break;
		}
		
		/* Getting Pressed Key */
		Key = KEYPAD_GetKey();
		
		if(Key != NO_KEY)
		{
			if(Key == '=')
			{
				LCD_ClearDisplay();
				if(EnteredPassword == Password)
				{
					/* Correct Old Password */
					EnteredPassword = 0;
					/* Clearing the display */
					LCD_ClearDisplay();
					
					SystemState = SYSTEM_UNLOCKED;
				}
				else
				{
					/* Wrong Old Password */
					LCD_GoToWriteString(2,3,"Wrong Password");
					SPI_SendNoBlock(BUZZER_TONE);
					_delay_ms(1000);
					TrialCounter++;
					/* Clearing the display */
					LCD_ClearDisplay();
					LCD_GoToWriteString(0,0,"Enter old password:");
					LCD_GoToWriteString(1,0,"Trials:");
					LCD_GoToWriteNumber(1,8,TrialCounter);
					/* Resetting the counter and Entered Password */
					Counter = 0;
					EnteredPassword = 0;
				}
			}
			else
			{
				LCD_GoToWriteChar(3,Counter,Key);
				_delay_ms(200);
				LCD_GoToWriteChar(3,Counter,'*');
				Counter++;
				EnteredPassword = (EnteredPassword*10) + (Key-'0');
			}
		}
	}
}

u8 GettingOption(void)
{	
	while(1)
	{
		/* Getting Pressed Key */
		Key = KEYPAD_GetKey();
		
		if(Key != NO_KEY)
		{
			if(Key == '1' || Key == '2')
			{
				if(Key == '1')
				{
					LCD_GoToWriteString(3,0,"1");
					_delay_ms(500);
					return OPTION_1;
				}
				else if(Key == '2')
				{
					LCD_GoToWriteString(3,0,"2");
					_delay_ms(500);
					return OPTION_2;
				}
			}
			else
			{
				LCD_GoToWriteString(3,0,"Wrong Option");
				_delay_ms(1000);
				LCD_GoToWriteString(3,0,"                     ");
			}
		}
	}
}

ISR(BAD_vect)
{
	
}