/******************************************************************************/
/*                                                                            */
/*   A M Q E R R   T O   Q U E U E                                            */
/*                                                                            */
/*  ------------------------------------------------------------------------  */
/*                                                                            */
/*  file: parse.c                                                             */
/*                                                                            */
/*  functions:                                                                */
/*    - parseFile800                        */
/*                                                  */
/*  history:                                */
/*                                                */
/******************************************************************************/

/******************************************************************************/
/*   I N C L U D E S                                                          */
/******************************************************************************/

// ---------------------------------------------------------
// system
// ---------------------------------------------------------
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

// ---------------------------------------------------------
// own 
// ---------------------------------------------------------
#include <ctl.h>

// ---------------------------------------------------------
// local
// ---------------------------------------------------------
#include <amqerr.h>

#include "lgloc.h"

/******************************************************************************/
/*   G L O B A L S                                                            */
/******************************************************************************/

/******************************************************************************/
/*   D E F I N E S                                                            */
/******************************************************************************/
#define AMQ_EXPLANATION "EXPLANATION:"
#define AMQ_ACTION      "ACTION:"

#define TXT_LENGTH 2048

#define DELIMETER_750 "-----"

/******************************************************************************/
/*   M A C R O S                                                              */
/******************************************************************************/

/******************************************************************************/
/*   E N U M                           */
/******************************************************************************/
#if(0)
enum  eAmqStanza
{
  NONE,
  EXPLANATION,
  ACTION
};
#endif

/******************************************************************************/
/*   T Y P E S                                 */
/******************************************************************************/
typedef enum eAmqStanza tAmqStanza ;
/******************************************************************************/
/*   P R O T O T Y P E S                                                      */
/******************************************************************************/

/******************************************************************************/
/*                                                                            */
/*   F U N C T I O N S                                                        */
/*                                                                            */
/******************************************************************************/

/******************************************************************************/
/*  P A R S E   F I L E   8 0 0                                               */
/*                                                                            */
/*  description:                                                              */
/*    parse AMQERR file with MQ Version 8.0                                   */
/*                                                                            */
/*  attributes:                                                               */
/*    - opened file pointer                                                   */ 
/*                                                                            */
/*  return code:                                                              */
/*    OK  -> bytes read                                                       */
/*    ERR -> reason < 0                                                       */
/*                                                                            */
/******************************************************************************/
int parseFile800( FILE* _fp, tAmqerrMessage *_msg )
{
  logFuncCall( );

  char buff[AMQERR_LINE_SIZE] ;

  int sysRc = 0 ;

  _msg->version = 800 ;

  const char txt[TXT_LENGTH];
  char *pTxt = (char*) txt;

  struct tm ti ;

  // -------------------------------------------------------  
  // 1st line
  //   Time Stamp, PID, User, Program
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) 
  { 
    sysRc = -1;
    logger(LAER_LOG_UNEXPECTED_EOF);
    goto _door; 
  }
  
  {
    char *time = strtok( buff,"-" );
    char *ptr = strtok(NULL,"(");
    char *pid = strtok(NULL,")");
          ptr = strtok(NULL,"(");
    char *usr = strtok(NULL,")");
          ptr = strtok(NULL,"(");
    char *prg = strtok(NULL,")");
          ptr = strtok(NULL,"\n");

    ti.tm_mon  = atol( strtok( time,"/" ) );
    ti.tm_mday = atol( strtok( NULL,"/" ) );
    ti.tm_year = atol( strtok( NULL," " ) ) - 1900 ;
    int h = atol( strtok( NULL,":" ) );
    ti.tm_min = atol( strtok( NULL,":" ) );
    ti.tm_sec = atol( strtok( NULL," " ) );
    char *apm = strtok( NULL," " ) ;
    if( apm[0] == 'A' )
    {
      if( h == 12 ) h=0;
    }
    else
    {
      h +=12 ;
    }
    ti.tm_hour = h;
   
    _msg->time = mktime(&ti);
    memcpy(_msg->pid    , pid, sizeof(_msg->pid)    );
    memcpy(_msg->user   , usr, sizeof(_msg->user)   );
    memcpy(_msg->program, prg, sizeof(_msg->program));

#if(0)
    printf("year  >%d<\n", ti.tm_year);
    printf("month >%d<\n", ti.tm_mon);
    printf("day   >%d<\n", ti.tm_mday);
    printf("hour  >%d<\n", ti.tm_hour);
    printf("min   >%d<\n", ti.tm_min);
    printf("sec   >%d<\n", ti.tm_sec);
    printf("pid   >%s<\n", pid) ;
    printf("user  >%s<\n", usr) ;
    printf("prg   >%s<\n", prg) ;
#endif
  }

  // -------------------------------------------------------  
  // 2nd line
  // host, installation
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) 
  { 
    sysRc = -1;
    logger(LAER_LOG_UNEXPECTED_EOF);
    goto _door; 
  }

  {
    char *ptr  = strtok(buff,"(");
    char *host = strtok(NULL,")");
          ptr  = strtok(NULL,"(");
    char *inst = strtok(NULL,")");
#if(0)
    printf("host   >%s<\n", host) ;
    printf("inst   >%s<\n", inst) ;
#endif
    memcpy(_msg->host,host,sizeof(_msg->host));
    memcpy(_msg->installation,inst,sizeof(_msg->installation));
  }

  // -------------------------------------------------------  
  // 3th line 
  // Version, queue manager
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) 
  { 
    sysRc = -1;
    logger(LAER_LOG_UNEXPECTED_EOF);
    goto _door; 
  }

  {
    char *ptr  = strtok(buff,"(");
    char *vrmf = strtok(NULL,")");
          ptr  = strtok(NULL,"(");
    char *qmgr = strtok(NULL,")");
    memcpy(_msg->vrmf,vrmf,sizeof(_msg->vrmf));
    memcpy(_msg->qmgr,qmgr,sizeof(_msg->qmgr));
    
  }

  // -------------------------------------------------------  
  // 4th line 
  // empty
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) 
  { 
    sysRc = -1;
    logger(LAER_LOG_UNEXPECTED_EOF);
    goto _door; 
  }

  // -------------------------------------------------------  
  // 5th line 
  // AMQ Message
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) 
  { 
    sysRc = -1;
    logger(LAER_LOG_UNEXPECTED_EOF);
    goto _door; 
  }

  {
    char *amq = strtok(buff,":");
    char *txt = strtok(NULL,"\n");
    memcpy(_msg->amqmsg,amq,sizeof(amq));
    _msg->level = ' ';
    memcpy(_msg->amqtxt,txt,sizeof(txt));
  }

  // -------------------------------------------------------  
  // 5th line 
  // empty
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) 
  { 
    sysRc = -1;
    logger(LAER_LOG_UNEXPECTED_EOF);
    goto _door; 
  }

  // -------------------------------------------------------  
  // 6th line 
  // 1st stanza EXPLANATION:
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) 
  { 
    sysRc = -1;
    logger(LAER_LOG_UNEXPECTED_EOF);
    goto _door; 
  }
  if( memcmp(buff,AMQ_EXPLANATION,sizeof(AMQ_EXPLANATION)-1)!= 0 )
  {
    sysRc = -2;
    logger(LAER_AMQ_FORMAT_ERR);
    goto _door;
  }
  _msg->explanation = pTxt; 

  while( fgets(buff,AMQERR_LINE_SIZE, _fp))
  {
    printf("buff %s",buff);
    if( memcmp(buff,AMQ_ACTION,sizeof(AMQ_ACTION)-1)== 0 )
    {
      *pTxt='\0';
      pTxt++;
      _msg->action = pTxt;
      continue;
    }
    if( memcmp(buff,DELIMETER_750,sizeof(DELIMETER_750)-1)==0)
    {
      *pTxt='\0';
      sysRc = ftell(_fp);
      _msg->explLeng = strlen(_msg->explanation);
      _msg->actLeng  = strlen(_msg->action);
      printf("---- expl\n%s\n----\n",_msg->explanation);
      printf("---- acti\n%s\n----\n",_msg->action);
      break;
    }
    memcpy(pTxt,buff,strlen(buff));
    pTxt += strlen(buff);
    if(pTxt-txt>TXT_LENGTH)
    {
      logger(LAER_AMQ_BUFFER_TO_SHORT);
    }
  }

  _door :

  logFuncExit( );

  return sysRc ;
}

