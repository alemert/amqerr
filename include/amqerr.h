/******************************************************************************/
/*                                    */
/*   A M Q E R R   T O   Q U E U E                                            */
/*                                    */
/******************************************************************************/

/******************************************************************************/
/*   I N C L U D E S                                                          */
/******************************************************************************/

// ---------------------------------------------------------
// system
// ---------------------------------------------------------
#include <time.h>
#include <limits.h>

// ---------------------------------------------------------
// MQ
// ---------------------------------------------------------
#include <cmqc.h>

// ---------------------------------------------------------
// own 
// ---------------------------------------------------------

/******************************************************************************/
/*   D E F I N E S                                                            */
/******************************************************************************/
#ifdef C_MODULE_AMQERR
const char progname[] = "amqerr" ;
#else
extern const char progname[] ;
#endif

#define AMQERR_LINE_SIZE  128
#define ITEM_LENGTH   PATH_MAX 
#define AMQERR        "AMQERR"
#define AMQ_FILE_NAME AMQERR"??.LOG"
#define AMQ_MAX_ID    99
#define AMQ_MAX_BASE_ID 3
#define CMPERR        "CMPERR03.LOG"

#define AMQERR_LOG_STRUC_ID       "AALI"   // admin amqerr log information
#define AMQERR_LOG_STRUC_ID_ARRAY 'A', 'A', 'L', 'I'
#define AMQERR_LOG_VERSION_1        1
#define AMQERR_LOG_CURENT_VERSION   AMQERR_LOG_VERSION_1
#define AMQERR_LOG_FILE_NONE_ARRAY  ' ',' ',' ',' ',' ',' ',' ',' ',' ',' ', \
                                    ' ',' ' 

#define AMQERR_LOG_DEFAULT {AMQERR_LOG_STRUC_ID_ARRAY}, \
                           AMQERR_LOG_VERSION_1, \
			   0,\
			   {AMQERR_LOG_FILE_NONE_ARRAY},\
			   0,\
			   0 
			   

/******************************************************************************/
/*   T Y P E S                                                                */
/******************************************************************************/
typedef struct sAmqerr tAmqerr ;
typedef struct sAmqerrState tAmqerrState;
typedef struct sAmgerrMessage tAmqerrMessage;

/******************************************************************************/
/*   S T R U C T S                                                            */
/******************************************************************************/
struct sAmqerr
{
  char name[PATH_MAX+1]; // length of AMQ_FILE_NAME + 1
  time_t mtime ;         // modification time of the AMQERR file
  off_t length;          // length of the AMQERR file
  MQBYTE24 msgId;        // message id containing information on STORE queue
};

struct sAmqerrState
{
  MQCHAR4  strucId;
  MQLONG   version;
  MQLONG   fileId;
  MQCHAR12 file;
  MQLONG   time;
  MQLONG   length;
};

struct sAmgerrMessage
{
  MQLONG version;
  time_t time   ;
  char user[12] ;
  char program[12];
  char host[16];
  char installation[MQ_INSTALLATION_NAME_LENGTH];
};

/******************************************************************************/
/*   G L O B A L E S                                                          */
/******************************************************************************/

/******************************************************************************/
/*   M A C R O S                                                              */
/******************************************************************************/
#define getDataPath( rc, path )  \
{                           \
  MQLONG _mDummy;             \
  rc = disQmgr( MQCA_SSL_KEY_REPOSITORY, path , &_mDummy );  \
}  

#define getCmdLevel( rc, cml )  \
{                           \
  char* _mDummy = NULL;              \
  rc= disQmgr( MQIA_COMMAND_LEVEL, _mDummy, cml );   \
}

/******************************************************************************/
/*   P R O T O T Y P E S                                                      */
/******************************************************************************/

// ---------------------------------------------------------
// amqerr.c
// ---------------------------------------------------------
int amqerr();

// ---------------------------------------------------------
// rotate.c
// ---------------------------------------------------------
int lsAmqerr( const char* _path, tAmqerr* _arr, int _lng);
int rotateAmqerr( tAmqerr *_arr );
int copy( const char* _src, const char* _dst );


// ---------------------------------------------------------
// mqcall.c
// ---------------------------------------------------------
MQLONG initMQ( const char* _qmgrName );
MQLONG houseKeepingMQ();
MQLONG disQmgr( MQLONG _selector, char* _strAttr, PMQLONG _intAttr );
MQLONG getSendState( tAmqerr* baseFile );
MQLONG putInitStateMsg( unsigned short _id );


// ---------------------------------------------------------
// parse.c
// ---------------------------------------------------------
int parseFile800( FILE* fp );

