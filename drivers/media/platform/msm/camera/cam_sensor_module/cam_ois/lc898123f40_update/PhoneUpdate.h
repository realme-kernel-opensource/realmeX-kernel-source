/**
 *		LC898123F40 Global declaration & prototype declaration
 *
 *		Copyright (C) 2017, ON Semiconductor, all right reserved.
 *
 **/
 
#ifndef PHONEUPDATE_H_
#define PHONEUPDATE_H_

//==============================================================================
//
//==============================================================================
#define	MODULE_VENDOR	1
#define	MDL_VER			1

#ifdef DEBUG
 extern void dbg_printf(const char *, ...);
 extern void dbg_Dump(const char *, int);
 #define TRACE_INIT(x)			dbgu_init(x)
 #define TRACE_USB(fmt, ...)	dbg_UsbData(fmt, ## __VA_ARGS__)
 #define TRACE(fmt, ...)		dbg_printf(fmt, ## __VA_ARGS__)
 #define TRACE_DUMP(x,y)		dbg_Dump(x,y)
#else
 #define TRACE_INIT(x)
 #define TRACE(...)
 #define TRACE_DUMP(x,y)
 #define TRACE_USB(...)
#endif

typedef	signed char			 INT_8;
typedef	short				 INT_16;
typedef	long                 INT_32;
typedef	long long            INT_64;
typedef	unsigned char       UINT_8;
typedef	unsigned short      UINT_16;
//typedef	unsigned long       UINT_32;
typedef unsigned int        UINT_32;
typedef	unsigned long long	UINT_64;

//****************************************************
//	STRUCTURE DEFINE
//****************************************************
typedef struct {
	UINT_16				Index;
	const UINT_8*		MagicCode;
	UINT_16				SizeMagicCode;
	const UINT_8*		FromCode;
	UINT_16				SizeFromCode;
}	DOWNLOAD_TBL;

typedef struct STRECALIB {
	INT_16	SsFctryOffX ;
	INT_16	SsFctryOffY ;
	INT_16	SsRecalOffX ;
	INT_16	SsRecalOffY ;
	INT_16	SsDiffX ;
	INT_16	SsDiffY ;
} stReCalib ;

typedef struct tag_Dual_Axis
{
	float xpos;
	float xhall;
	float ypos;
	float yhall;
}Dual_Axis_t;


#define	WPB_OFF			0x01
#define WPB_ON			0x00
#define	SUCCESS			0x00
#define	FAILURE			0x01

//==============================================================================
//
//==============================================================================
#define		SiVerNum						0x8000

#define		F40_IO_ADR_ACCESS				0xC000
#define		F40_IO_DAT_ACCESS				0xD000
#define 	SYSDSP_DSPDIV					0xD00014
#define 	SYSDSP_SOFTRES					0xD0006C
#define 	OSCRSEL							0xD00090
#define 	OSCCURSEL						0xD00094
#define 	SYSDSP_REMAP					0xD000AC
#define 	SYSDSP_CVER						0xD00100
#define	 	FLASHROM_123F40					0xE07000
#define 	FLASHROM_F40_RDATL				(FLASHROM_123F40 + 0x00)
#define 	FLASHROM_F40_RDATH				(FLASHROM_123F40 + 0x04)
#define 	FLASHROM_F40_WDATL				(FLASHROM_123F40 + 0x08)
#define 	FLASHROM_F40_WDATH				(FLASHROM_123F40 + 0x0C)
#define 	FLASHROM_F40_ADR				(FLASHROM_123F40 + 0x10)
#define 	FLASHROM_F40_ACSCNT				(FLASHROM_123F40 + 0x14)
#define 	FLASHROM_F40_CMD				(FLASHROM_123F40 + 0x18)
#define 	FLASHROM_F40_WPB				(FLASHROM_123F40 + 0x1C)
#define 	FLASHROM_F40_INT				(FLASHROM_123F40 + 0x20)
#define 	FLASHROM_F40_RSTB_FLA			(FLASHROM_123F40 + 0x4CC)
#define 	FLASHROM_F40_UNLK_CODE1			(FLASHROM_123F40 + 0x554)
#define 	FLASHROM_F40_CLK_FLAON			(FLASHROM_123F40 + 0x664)
#define 	FLASHROM_F40_UNLK_CODE2			(FLASHROM_123F40 + 0xAA8)
#define 	FLASHROM_F40_UNLK_CODE3			(FLASHROM_123F40 + 0xCCC)

#define		READ_STATUS_INI					0x01000000



//==============================================================================
// Calibration Data Memory Map
//==============================================================================
// Calibration Status
#define	CALIBRATION_STATUS		(  0 )
// Hall amplitude Calibration X
#define	HALL_MAX_BEFORE_X		(  1 )
#define	HALL_MIN_BEFORE_X		(  2 )
#define	HALL_MAX_AFTER_X		(  3 )
#define	HALL_MIN_AFTER_X		(  4 )
// Hall amplitude Calibration Y
#define	HALL_MAX_BEFORE_Y		(  5 )
#define	HALL_MIN_BEFORE_Y		(  6 )
#define	HALL_MAX_AFTER_Y		(  7 )
#define	HALL_MIN_AFTER_Y		(  8 )
// Hall Bias/Offset
#define	HALL_BIAS_DAC_X			(  9 )
#define	HALL_OFFSET_DAC_X		( 10 )
#define	HALL_BIAS_DAC_Y			( 11 )
#define	HALL_OFFSET_DAC_Y		( 12 )
// Loop Gain Calibration X
#define	LOOP_GAIN_X				( 13 )
// Loop Gain Calibration Y
#define	LOOP_GAIN_Y				( 14 )
// Lens Center Calibration
#define	MECHA_CENTER_X			( 15 )
#define	MECHA_CENTER_Y			( 16 )
// Optical Center Calibration
#define	OPT_CENTER_X			( 17 )
#define	OPT_CENTER_Y			( 18 )
// Gyro Offset Calibration
#define	GYRO_OFFSET_X			( 19 )
#define	GYRO_OFFSET_Y			( 20 )
// Gyro Gain Calibration
#define	GYRO_GAIN_X				( 21 )
#define	GYRO_GAIN_Y				( 22 )
// AF calibration
#ifdef	SEL_CLOSED_AF
#define	AF_HALL_BIAS_DAC		( 23 )
#define	AF_HALL_OFFSET_DAC		( 24 )
#define	AF_LOOP_GAIN			( 25 )
#define	AF_MECHA_CENTER			( 26 )
#define	AF_HALL_AMP_MAG			( 27 )
#define	AF_HALL_MAX_BEFORE		( 28 )
#define	AF_HALL_MIN_BEFORE		( 29 )
#define	AF_HALL_MAX_AFTER		( 30 )
#define	AF_HALL_MIN_AFTER		( 31 )
#else	// SEL_CLOSED_AF
#define	AF_LONG_M_RRMD1			( 23 )
#define	AF_LONG_I_RRMD1			( 24 )
#define	AF_SHORT_IIM_RRMD1		( 25 )
#define	AF_SHORT_IMI_RRMD1		( 26 )
#define	AF_SHORT_MIM_RRMD1		( 27 )
#define	AF_SHORT_MMI_RRMD1		( 28 )

#define	OIS_POS_BY_AF_X1		( 23 )
#define	OIS_POS_BY_AF_X2		( 24 )
#define	OIS_POS_BY_AF_X3		( 25 )
#define	OIS_POS_BY_AF_X4		( 26 )
#define	OIS_POS_BY_AF_X5		( 27 )
#define	OIS_POS_BY_AF_X6		( 28 )
#define	OIS_POS_BY_AF_X7		( 29 )
#define	OIS_POS_BY_AF_X8		( 30 )
#define	OIS_POS_BY_AF_X9		( 31 )
#endif	// SEL_CLOSED_AF
// Gyro mixing correction
#define MIXING_HX45X			( 32 )
#define MIXING_HX45Y			( 33 )
#define MIXING_HY45Y			( 34 )
#define MIXING_HY45X			( 35 )
#define MIXING_HXSX				( 36 )
#define MIXING_HYSX				( 36 )
// Gyro angle correction
#define MIXING_GX45X			( 37 )
#define MIXING_GX45Y			( 38 )
#define MIXING_GY45Y			( 39 )
#define MIXING_GY45X			( 40 )
// Liniearity correction
#define LN_POS1					( 41 )
#define LN_POS2					( 42 )
#define LN_POS3					( 43 )
#define LN_POS4					( 44 )
#define LN_POS5					( 45 )
#define LN_POS6					( 46 )
#define LN_POS7					( 47 )
#define LN_STEP					( 48 )
// Factory Gyro Gain Calibration
#define	GYRO_FCTRY_OFST_X		( 49 )
#define	GYRO_FCTRY_OFST_Y		( 50 )
// Gyro Offset Calibration
#define	GYRO_OFFSET_Z			( 51 )
// Accl offset
//#define	ACCL_OFFSET_X			( 52 )
//#define	ACCL_OFFSET_Y			( 53 )
#define	DEFAULT_GAIN_X			( 52 )
#define	DEFAULT_GAIN_Y			( 53 )
#define	ACCL_OFFSET_Z			( 54 )
#ifdef SEL_SHIFT_COR
// Accl matrix
#define	ACCL_MTRX_0				( 55 )
#define	ACCL_MTRX_1				( 56 )
#define	ACCL_MTRX_2				( 57 )
#define	ACCL_MTRX_3				( 58 )
#define	ACCL_MTRX_4				( 59 )
#define	ACCL_MTRX_5				( 60 )
#define	ACCL_MTRX_6				( 61 )
#define	ACCL_MTRX_7				( 62 )
#define	ACCL_MTRX_8				( 63 )
#else
#define	OIS_POS_BY_AF_Y1		( 55 )
#define	OIS_POS_BY_AF_Y2		( 56 )
#define	OIS_POS_BY_AF_Y3		( 57 )
#define	OIS_POS_BY_AF_Y4		( 58 )
#define	OIS_POS_BY_AF_Y5		( 59 )
#define	OIS_POS_BY_AF_Y6		( 60 )
#define	OIS_POS_BY_AF_Y7		( 61 )
#define	OIS_POS_BY_AF_Y8		( 62 )
#define	OIS_POS_BY_AF_Y9		( 63 )
#endif


//==============================================================================
// Prototype
//==============================================================================
extern void		F40_Control( UINT_16 ) ;
extern void		F40_IORead32A( UINT_32, UINT_32 * ) ;
extern void		F40_IOWrite32A( UINT_32, UINT_32 ) ;
extern UINT_8	F40_ReadWPB( void ) ;
extern UINT_8	F40_UnlockCodeSet( void ) ;
extern UINT_8	F40_UnlockCodeClear(void) ;
extern UINT_8	F40_FlashDownload( UINT_8, UINT_8, UINT_8 ) ;
extern UINT_8	F40_FlashUpdate( UINT_8, DOWNLOAD_TBL* ) ;
extern UINT_8	F40_FlashBlockErase( UINT_32 ) ;
extern UINT_8	F40_FlashBurstWrite( const UINT_8 *, UINT_32, UINT_32) ;
extern void		F40_FlashSectorRead( UINT_32, UINT_8 * ) ;
extern UINT_8	F40_FlashInt32Read( UINT_32, UINT_32 * ) ;
extern void		F40_CalcChecksum( const UINT_8 *, UINT_32, UINT_32 *, UINT_32 * ) ;
extern void		F40_CalcBlockChksum( UINT_8, UINT_32 *, UINT_32 * ) ;
extern void		F40_ReadCalData( UINT_32 * , UINT_32 *  ) ;
extern UINT_8	F40_GyroReCalib( stReCalib *  ) ;
extern UINT_8	F40_WrGyroOffsetData( void ) ;
extern UINT_8	F40_RdStatus( UINT_8  ) ;
extern UINT_8	F40_WriteCalData( UINT_32 * , UINT_32 *  );
extern UINT_16	Accuracy(float , UINT_16 , UINT_16 , UINT_16 , UINT_16 , UINT_16 );

#endif /* #ifndef OIS_H_ */
