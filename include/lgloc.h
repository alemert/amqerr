/******************************************************************************/
/*          l o g g e r   l o c a l   m e s s a g e   c a t a l o g           */
/******************************************************************************/

/******************************************************************************/
/*   I N C L U D E S                                                          */
/******************************************************************************/
// ---------------------------------------------------------
// system
// ---------------------------------------------------------

// ---------------------------------------------------------
// own 
// ---------------------------------------------------------

/******************************************************************************/
/*   D E F I N E S                                                            */
/******************************************************************************/
#define     LAER_LOG_ID_HIGH              1010
#define LEV_LAER_LOG_ID_HIGH              ERR
#define TXT_LAER_LOG_ID_HIGH              "log id %d to high for AMQERR files"

#define     LAER_AMQ_BUFFER_TO_SHORT      1015
#define LEV_LAER_AMQ_BUFFER_TO_SHORT      ERR
#define TXT_LAER_AMQ_BUFFER_TO_SHORT      "AMQ buffer to short for text"

#define     LAER_LOG_UNEXPECTED_EOF       1015
#define LEV_LAER_LOG_UNEXPECTED_EOF       ERR
#define TXT_LAER_LOG_UNEXPECTED_EOF       "unexpected line EOF"

#define     LAER_MQ_CMD_VER_ERR           1020
#define LEV_LAER_MQ_CMD_VER_ERR           ERR
#define TXT_LAER_MQ_CMD_VER_ERR           "MQ Command level %d not supported"

#define     LAER_AMQ_FORMAT_ERR           1030
#define LEV_LAER_AMQ_FORMAT_ERR           ERR
#define TXT_LAER_AMQ_FORMAT_ERR           "unexpected line format"




