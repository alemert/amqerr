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

/******************************************************************************/
/*   G L O B A L S                                                            */
/******************************************************************************/

/******************************************************************************/
/*   D E F I N E S                                                            */
/******************************************************************************/

/******************************************************************************/
/*   M A C R O S                                                              */
/******************************************************************************/

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
int parseFile800( FILE* _fp )
{
  logFuncCall( );

  char buff[AMQERR_LINE_SIZE] ;

  int sysRc ;

  tAmqerrMessage msg;
  msg.version = 800 ;

  struct tm ti = {0};

  // -------------------------------------------------------  
  // 1st line
  //   Time Stamp, PID, User, Program
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) goto _door;
  
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
   
    msg.time = mktime(&ti);
    memcpy(msg.pid    , pid, sizeof(msg.pid)    );
    memcpy(msg.user   , usr, sizeof(msg.user)   );
    memcpy(msg.program, prg, sizeof(msg.program));

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
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) goto _door;

  {
    char *ptr  = strtok(buff,"(");
    char *host = strtok(NULL,")");
          ptr  = strtok(NULL,"(");
    char *inst = strtok(NULL,")");
#if(0)
    printf("host   >%s<\n", host) ;
    printf("inst   >%s<\n", inst) ;
#endif
    memcpy(msg.host,host,sizeof(msg.host));
    memcpy(msg.installation,inst,sizeof(msg.installation));
  }

  // -------------------------------------------------------  
  // 3th line 
  // Version, queue manager
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) goto _door;

  {
    char *ptr  = strtok(buff,"(");
    char *vrmf = strtok(NULL,")");
          ptr  = strtok(NULL,"(");
    char *qmgr = strtok(NULL,")");
    memcpy(msg.vrmf,vrmf,sizeof(msg.vrmf));
    memcpy(msg.qmgr,qmgr,sizeof(msg.qmgr));
    
  }

  // -------------------------------------------------------  
  // 4th line 
  // empty
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) goto _door;

  // -------------------------------------------------------  
  // 5th line 
  // AMQ Message
  // -------------------------------------------------------  
  if( !fgets(buff,AMQERR_LINE_SIZE, _fp) ) goto _door;

  {
    char *amq = strtok(buff,":");
    char *txt = strtok(NULL,"\n");
    memcpy(msg.amqmsg,amq,sizeof(amq));
    msg.level = ' ';
    memcpy(msg.amqtxt,txt,sizeof(txt));
  }

  while( fgets(buff,AMQERR_LINE_SIZE, _fp))
  {
    printf("%s",buff);
  }

  _door :

  logFuncExit( );

  return sysRc ;
}

