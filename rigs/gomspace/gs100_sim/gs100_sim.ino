/*
 *  Simple fundamental function simulator of GOMSPACE GS100 satellite 
 *  radio transceiver
 *  
 *  Created in 2022 by Richard Linhart OK1CTR OK1CTR@gmail.com
 *  and used during VZLUSAT-2 satellite mission.
 *
 *   This source code is free software; you can redistribute it and/or
 *   modify it under the terms of the GNU Lesser General Public
 *   License as published by the Free Software Foundation; either
 *   version 2.1 of the License, or (at your option) any later version.
 *
 *   This library is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *   Lesser General Public License for more details.
 *
 *   You should have received a copy of the GNU Lesser General Public
 *   License along with this library; if not, write to the Free Software
 *   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

/* Includes ------------------------------------------------------------------*/

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

/* Private defines -----------------------------------------------------------*/

//! If defined, also silent commands print their result
#undef _DBG_SILENT_
//! If defined, value range bounds are checked (originally not)
#undef _CHECK_BOUNDS_

//! Function finished with success
#define STATUS_OK                                                 0
//! Function finished with error
#define STATUS_ERROR                                             -1
//! Serial baud rate - cannot use 500000 as the GS100 have
#define BAUD_RATE                                            115200
//! Incoming serial buffer length
#define BUF_LENGTH                                               32
//! New line character on receive
#define NLRX                                                    '\n'
//! New line character on transmit
#define NLTX                                                "\r\r\n"
//! Displayed prompt
#define PROMPT     "\x1B[1;32mnanocom-ax\x1B[1;30m # \x1B[0m\x1B[0m"

/* Private macros ------------------------------------------------------------*/

//! Checks leading substring as command
#define LEADINGSTR(a,b) (strncmp((a), (b), strlen(b)) == 0)
//! Find the first alphabet character in string
#define FIRSTBETA(a) while(*(a) != '\0' && !isalpha(*(a))) { (a)++; }
//! Find the first alphabet character or number in string
#define FIRSTALNUM(a) while(*(a) != '\0' && !isalnum(*(a))) { (a)++; }
//! Find the first space in string
#define FIRSTSPACE(a) while(*(a) != '\0' && *(a) != ' ') { (a)++; }

/* Private typedefs ----------------------------------------------------------*/

//! Types of hint outputs
typedef enum {
  hint_param1,
  hint_param2,
  hint_mem,
  hint_get1,
  hint_get2,
  hint_set
} hint_t;

/* Private constants ---------------------------------------------------------*/

//! Low frequency limit, both for receive and transmit
static const unsigned long f_min = 430000000;
//! High frequency limit, both for receive and transmit
static const unsigned long f_max = 440000000;

/* Private variables ---------------------------------------------------------*/

//! Selected configuration table memory
static uint8_t mem = 0;
//! Radio receive frequency
static unsigned long rx_freq;
//! Radio transmit frequency
static unsigned long tx_freq;

/* Private function prototypes -----------------------------------------------*/

/**
 * @brief Store incoming serial characters until line end
 * @return The line if received successfully or NULL
 */
static char *serReadLine(void);

/**
 * @brief Process the command PARAM
 * @param The input command string
 * @return 0 in case of success
 */
static int cmdParam(char *cmd);

/**
 * @brief Process the command MEM
 * @param The input command string
 * @return 0 in case of success
 */
static int cmdMem(char *cmd);

/**
 * @brief Process the command GET
 * @param The input command string
 * @return 0 in case of success
 */
static int cmdGet(char *cmd);

/**
 * @brief Process the command SET
 * @param The input command string
 * @return 0 in case of success
 */
static int cmdSet(char *cmd);

/**
 * @brief Displays command prompt
 */
static void prnPrompt(void);

/**
 * @brief Displays hint output for commands
 * @param hint_type Type or command
 * @param cmd Part of command to comment
 */
static void prnHint(hint_t hint_type, char *cmd);

/* Functions -----------------------------------------------------------------*/

/**
 * @brief Arduino SETUP function
 */
void setup(void)
{
  Serial.begin(BAUD_RATE);
  rx_freq = f_min;
  tx_freq = f_min;
  return;
}


/**
 * @brief Arduino LOOP function
 */
void loop(void)
{
  char *c;

  // reading command line from serial
  if ((c = serReadLine()) != NULL)
  {
    FIRSTBETA(c);
    if (LEADINGSTR(c, "param"))
    {
      cmdParam(c);
    } else {
      if (strlen(c) > 0)
      {
        Serial.print("Unknown command '");
        Serial.print(c);
        Serial.print("'");
        Serial.print(NLTX);
      }
    }
    prnPrompt();
  }
  
  return;
}

/* Private functions ---------------------------------------------------------*/

/* Store incoming serial characters until line end */
static char *serReadLine(void)
{
  int inchar;
  static char buf[BUF_LENGTH] = "\0", *p = buf;

  if (Serial.available())
  {
    inchar = Serial.read();
    // is it end of line?
    if (inchar > 0)
    {
      if (inchar == NLRX)
      {
        Serial.print(NLTX);  // enf of line repeated strange
        *p = '\0';  // terminate the string
        p = buf;  // reset the buffer
        return(buf);
      }
      // repeat received character
      Serial.write(inchar & 0xFF);
      // insert new character into the buffer
      *p = inchar & 0xFF;
      if (p - buf < BUF_LENGTH - 1)
      {
        p++;
      } else {
        p = buf;  // reset the buffer
      }
    }
  }
  return(NULL);
}


/* Process the command PARAM */
static int cmdParam(char *cmd)
{
  FIRSTSPACE(cmd);
  FIRSTBETA(cmd);
  if (strlen(cmd) > 0)
  {
    if (LEADINGSTR(cmd, "mem")) return(cmdMem(cmd));
    if (LEADINGSTR(cmd, "get")) return(cmdGet(cmd));
    if (LEADINGSTR(cmd, "set")) return(cmdSet(cmd));
    prnHint(hint_param1, cmd);
    return(STATUS_ERROR);
  } else {
    prnHint(hint_param2, NULL);
    return(STATUS_ERROR);
  }
}


/* Process the command MEM */
static int cmdMem(char *cmd)
{
  FIRSTSPACE(cmd);
  FIRSTALNUM(cmd);
  if (strlen(cmd) > 0)
  {
    mem = strtol(cmd, &cmd, 10) & 0xFF;
#ifdef _DBG_SILENT_
    Serial.print(mem);
    Serial.print(NLTX);
#endif
    return(STATUS_OK);
  } else {
    prnHint(hint_mem, NULL);
    return(STATUS_ERROR);
  }
}


/* Process the command GET */
static int cmdGet(char *cmd)
{
  unsigned long f;

  FIRSTSPACE(cmd);
  FIRSTBETA(cmd);
  if (strlen(cmd) > 0)
  {
    if (LEADINGSTR(cmd, "freq"))
    {
      switch (mem)
      {
        case 1: f = rx_freq; break;
        case 5: f = tx_freq; break;
        default: return(STATUS_ERROR);
      }
      Serial.print("  GET freq = ");
      Serial.print(f);
      Serial.print(NLTX);
      return(STATUS_OK);
    } else {
      prnHint(hint_get1, cmd);
      return(STATUS_ERROR);
    }
  } else {
    prnHint(hint_get2, NULL);
    return(STATUS_ERROR);
  }
}


/* Process the command SET */
static int cmdSet(char *cmd)
{
  unsigned long f;

  FIRSTSPACE(cmd);
  FIRSTBETA(cmd);
  if (strlen(cmd) > 0)
  {
    if (LEADINGSTR(cmd, "freq"))
    {
      FIRSTSPACE(cmd);
      FIRSTALNUM(cmd);
      if (strlen(cmd) == 0)
      {
        prnHint(hint_set, NULL);
        return(STATUS_ERROR);        
      }
      f = strtol(cmd, &cmd, 10);
#ifdef _DBG_SILENT_
    Serial.print(f);
    Serial.print(NLTX);
#endif
#ifdef _CHECK_BOUNDS_
      if (f < f_min) f = f_min;
      if (f > f_max) f = f_max;
#endif
      switch (mem)
      {
        case 1: rx_freq = f; break;
        case 5: tx_freq = f; break;
        default: return(STATUS_ERROR);
      }      
      return(STATUS_OK);
    } else {
      // different behavior than at GET
      prnHint(hint_set, NULL);
      return(STATUS_ERROR);
    }
  } else {
    prnHint(hint_set, NULL);
    return(STATUS_ERROR);
  }
}


/* Displays command prompt */
static void prnPrompt(void)
{
  Serial.print(PROMPT);
  return;
}


/* Displays hint output for commands */
static void prnHint(hint_t hint_type, char *cmd)
{
  switch (hint_type)
  {
    case hint_param1:
      Serial.print("Unknown command 'param ");
      Serial.print(cmd);
      Serial.print("'");
      Serial.print(NLTX);
      return;

    case hint_param2:
      // reduced on simulated commands only!
      Serial.print("'param' contains sub-commands:");
      Serial.print(NLTX);
      Serial.print("  mem                 Set cmds working mem");
      Serial.print(NLTX);
      Serial.print("  set                 Set parameter");
      Serial.print(NLTX);
      Serial.print("  get                 Get parameter");
      Serial.print(NLTX);
      break;

    case hint_mem:
      Serial.print("usage: mem <mem>");
      Serial.print(NLTX);
      Serial.print("Could not execute command 'param mem', error -2");
      Serial.print(NLTX);
      break;

    case hint_get1:
      Serial.print("Unknown parameter ");
      Serial.print(cmd);
      Serial.print(NLTX);
      Serial.print("Could not execute command 'param get shit', error -1");
      Serial.print(NLTX);
      break;

    case hint_get2:
      Serial.print("usage: get <name|addr>");
      Serial.print(NLTX);
      Serial.print("Could not execute command 'param get', error -2");
      Serial.print(NLTX);
      break;

    case hint_set:
      Serial.print("usage: set <name|addr> <value> [quiet]");
      Serial.print(NLTX);
      Serial.print("Could not execute command 'param set', error -2");
      Serial.print(NLTX);      
      break;
  }
  return;
}

/*----------------------------------------------------------------------------*/
