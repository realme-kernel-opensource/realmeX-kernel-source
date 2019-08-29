/**
 *		LC898123F40 Flash update
 *
 *		Copyright (C) 2017, ON Semiconductor, all right reserved.
 *
 **/



//**************************
//	Include Header File
//**************************
#include	"PhoneUpdate.h"
#include	"PmemCode.h"
//#include <stdlib.h>
//#include	"math.h"

#include <linux/slab.h>
#include "cam_debug_util.h"

#include	"FromCode_01_00.h"


#define	USER_RESERVE			3
#define	ERASE_BLOCKS			(16 - USER_RESERVE)
#define BURST_LENGTH ( 8*5 )
#define DMB_COEFF_ADDRESS		0x21
#define BLOCK_UNIT				0x200
#define BLOCK_BYTE				2560
#define SECTOR_SIZE				320
#define HALF_SECTOR_ADD_UNIT	0x20
#define FLASH_ACCESS_SIZE		32		//2ÌæÅÝèBMAXÍ32
#define	USER_AREA_START			(BLOCK_UNIT * ERASE_BLOCKS)

#define	CNT100MS				1352
#define	CNT200MS				2703

//****************************************************
//	CUSTOMER NECESSARY CREATING FUNCTION LIST
//****************************************************
/* for I2C communication */
extern	void RamWrite32A( UINT_16 addr, UINT_32 data );
extern 	void RamRead32A( UINT_32 addr, UINT_32 *data );
/* for I2C Multi Translation : Burst Mode*/
extern 	void CntWrt( INT_8 *data, UINT_16 size ) ;
//extern	void CntRd3( UINT_32, void *, UINT_16 ) ;

/* WPB control for LC898123F40*/
extern void WPBCtrl( UINT_8 ctrl );
/* for Wait timer [Need to adjust for your system] */
extern void	WitTim( UINT_16 delay );

UINT_8	WrChangeCalData_07_0C_DC(  UINT_8 ) ;

//**************************
//	extern  Function LIST
//**************************
UINT_32	UlBufDat[ 64 ] ;							//!< Calibration data write buffer(256 bytes)
Dual_Axis_t	xy_raw_data[360/3 + 1];

//**************************
//	Table of download file
//**************************

const DOWNLOAD_TBL DTbl[] = {
	{0x0100, CcMagicCodeF40_01_00, sizeof(CcMagicCodeF40_01_00), CcFromCodeF40_01_00, sizeof(CcFromCodeF40_01_00) },
	{0xFFFF, (void*)0,                0,                               (void*)0,               0                  }
};

//**************************
//	Local Function Prototype
//**************************


//********************************************************************************
// Function Name 	: F40_IOWrite32A
//********************************************************************************
void F40_IORead32A( UINT_32 IOadrs, UINT_32 *IOdata )
{
	RamWrite32A( F40_IO_ADR_ACCESS, IOadrs ) ;
	RamRead32A ( F40_IO_DAT_ACCESS, IOdata ) ;
}

//********************************************************************************
// Function Name 	: F40_IOWrite32A
//********************************************************************************
void F40_IOWrite32A( UINT_32 IOadrs, UINT_32 IOdata )
{
	RamWrite32A( F40_IO_ADR_ACCESS, IOadrs ) ;
	RamWrite32A( F40_IO_DAT_ACCESS, IOdata ) ;
}

//********************************************************************************
// Function Name 	: WPB level read
//********************************************************************************
UINT_8 F40_ReadWPB( void )
{
	UINT_32 UlReadVal, UlCnt=0;

	do{
		F40_IORead32A( FLASHROM_F40_WPB, &UlReadVal );
		if( (UlReadVal & 0x00000004) != 0 )	return ( 1 ) ;
		WitTim( 1 );
	}while ( UlCnt++ < 10 );
	return ( 0 );
}

//********************************************************************************
// Function Name 	: F40_UnlockCodeSet
//********************************************************************************
UINT_8 F40_UnlockCodeSet( void )
{
	UINT_32 UlReadVal ;

	WPBCtrl( WPB_OFF ) ;
	if ( F40_ReadWPB() != 1 )
		return ( 5 ) ;

	F40_IOWrite32A( FLASHROM_F40_UNLK_CODE1,	0xAAAAAAAA ) ;
	F40_IOWrite32A( FLASHROM_F40_UNLK_CODE2,	0x55555555 ) ;
	F40_IOWrite32A( FLASHROM_F40_RSTB_FLA,		0x00000001 ) ;
	F40_IOWrite32A( FLASHROM_F40_CLK_FLAON,		0x00000010 ) ;
	F40_IOWrite32A( FLASHROM_F40_UNLK_CODE3,	0x0000ACD5 ) ;
	F40_IOWrite32A( FLASHROM_F40_WPB,			0x00000001 ) ;
	RamRead32A(  F40_IO_DAT_ACCESS,				&UlReadVal ) ;

	if ( (UlReadVal & 0x00000007) != 7 )
		return( 1 ) ;

	return( 0 ) ;
}

//********************************************************************************
// Function Name 	: F40_UnlockCodeClear
//********************************************************************************
UINT_8 F40_UnlockCodeClear(void)
{
	UINT_32 UlReadVal ;

	F40_IOWrite32A( FLASHROM_F40_WPB,			0x00000010 ) ;
	RamRead32A( F40_IO_DAT_ACCESS,			&UlReadVal ) ;

	if( (UlReadVal & 0x00000080) != 0 )
		return( 3 ) ;

	WPBCtrl( WPB_ON ) ;

	return( 0 ) ;
}



//********************************************************************************
// Function Name 	: F40_FlashBlockErase
//********************************************************************************
UINT_8 F40_FlashBlockErase( UINT_32 SetAddress )
{
	UINT_32	UlReadVal, UlCnt ;
	UINT_8	ans	= 0 ;

	if( SetAddress & 0x00010000 )
		return( 9 );

	ans	= F40_UnlockCodeSet() ;
	if( ans != 0 )
		return( ans ) ;

	F40_IOWrite32A( FLASHROM_F40_ADR,			(SetAddress & 0xFFFFFE00) ) ;
	F40_IOWrite32A( FLASHROM_F40_CMD,			0x00000006 ) ;

	WitTim( 5 ) ;

	UlCnt	= 0 ;
	do {
		if( UlCnt++ > 100 ) {
			ans = 2 ;
			break ;
		}

		F40_IORead32A( FLASHROM_F40_INT,		&UlReadVal ) ;
	} while( ( UlReadVal & 0x00000080 ) != 0 ) ;

	F40_UnlockCodeClear() ;

	return( ans ) ;
}


//********************************************************************************
// Function Name 	: F40_FlashBurstWrite
//********************************************************************************
UINT_8 F40_FlashBurstWrite( const UINT_8 *NcDataVal, UINT_32 NcDataLength, UINT_32 ScNvrMan )
{
	UINT_32	i, j, UlCnt ;
	UINT_8	data[163] ;
	UINT_32	UlReadVal ;
	UINT_8	UcOddEvn ;
	UINT_8	Remainder ;

	data[0] = 0xF0 ;
	data[1] = 0x08 ;
	data[2] = BURST_LENGTH ;

	for( i = 0 ; i < (NcDataLength / BURST_LENGTH) ; i++ ) {
		UlCnt = 3 ;

		UcOddEvn =i % 2 ;
		data[1] = 0x08 + UcOddEvn ;

		for( j = 0 ; j < BURST_LENGTH; j++ )
			data[UlCnt++] = *NcDataVal++ ;

		CntWrt( data, BURST_LENGTH + 3 ) ;
		RamWrite32A( 0xF00A ,(UINT_32) ( ( BURST_LENGTH / 5 ) * i + ScNvrMan) ) ;
		RamWrite32A( 0xF00B ,(UINT_32) (BURST_LENGTH / 5) ) ;

		RamWrite32A( 0xF00C , 4 + 4 * UcOddEvn ) ;
	}

	Remainder = NcDataLength % BURST_LENGTH ;
	if( Remainder != 0 ) {
		data[2] = Remainder ;
		UlCnt = 3 ;
		UcOddEvn =i % 2 ;
		data[1] = 0x08 + UcOddEvn ;

		for( j = 0 ; j < Remainder; j++ )
			data[UlCnt++] = *NcDataVal++ ;

		CntWrt( data, BURST_LENGTH + 3 ) ;
		RamWrite32A( 0xF00A ,(UINT_32) ( ( BURST_LENGTH / 5 ) * i + ScNvrMan) ) ;
		RamWrite32A( 0xF00B ,(UINT_32) (Remainder /5) ) ;
		RamWrite32A( 0xF00C , 4 + 4 * UcOddEvn ) ;
	}

	UlCnt = 0 ;
	do {
		if( UlCnt++ > 100 )
			return ( 1 );

		RamRead32A( 0xF00C, &UlReadVal ) ;
	} while ( UlReadVal != 0 ) ;
	
	return( 0 );
}


//********************************************************************************
// Function Name 	: F40_FlashSectorRead
//********************************************************************************
void F40_FlashSectorRead( UINT_32 UlAddress, UINT_8 *PucData )
{
	UINT_8	UcIndex, UcNum ;
	UINT_8	UcReadDat[ 4 ] ;

	F40_IOWrite32A( FLASHROM_F40_ADR,			( UlAddress & 0xFFFFFFC0 ) ) ;
	F40_IOWrite32A( FLASHROM_F40_ACSCNT,		63 ) ;
	UcNum	= 64 ;

	F40_IOWrite32A( FLASHROM_F40_CMD,			0x00000001 ) ;

	for( UcIndex = 0 ; UcIndex < UcNum ; UcIndex++ ) {
		RamWrite32A( F40_IO_ADR_ACCESS,		FLASHROM_F40_RDATH ) ;
		RamRead32A(  F40_IO_DAT_ACCESS,		(UINT_32 *)UcReadDat ) ;
		*PucData++		= UcReadDat[ 0 ] ;
		RamWrite32A( F40_IO_ADR_ACCESS,		FLASHROM_F40_RDATL ) ;
		RamRead32A(  F40_IO_DAT_ACCESS,		(UINT_32 *)UcReadDat ) ;
		*PucData++	= UcReadDat[ 3 ] ;
		*PucData++	= UcReadDat[ 2 ] ;
		*PucData++	= UcReadDat[ 1 ] ;
		*PucData++	= UcReadDat[ 0 ] ;
	}
}

//********************************************************************************
// Function Name 	: F40_FlashInt32Read
//********************************************************************************
UINT_8	F40_FlashInt32Read( UINT_32 UlAddress, UINT_32 *PuiData )
{
	UINT_8	UcResult = 0 ;

	F40_IOWrite32A( FLASHROM_F40_ADR,		UlAddress ) ;
	F40_IOWrite32A( FLASHROM_F40_ACSCNT,	0x00000000 ) ;
	F40_IOWrite32A( FLASHROM_F40_CMD,		0x00000001 ) ;
	F40_IORead32A( FLASHROM_F40_RDATL,		PuiData ) ;

	return UcResult ;
}

//********************************************************************************
// Function Name 	: F40_CalcChecksum
//********************************************************************************
void F40_CalcChecksum( const UINT_8 *pData, UINT_32 len, UINT_32 *pSumH, UINT_32 *pSumL )
{
	UINT_64 sum = 0 ;
	UINT_32 dat ;
	UINT_16 i ;

	for( i = 0; i < len / 5; i++ ) {
#ifdef _BIG_ENDIAN_

		dat  = (UINT_32)*pData++ ;
		dat += (UINT_32)*pData++ << 8;
		dat += (UINT_32)*pData++ << 16;
		dat += (UINT_32)*pData++ << 24;

		sum  += (UINT_64)*pData++ << 32;
#else
		sum  += (UINT_64)*pData++ << 32 ;

		dat  = (UINT_32)*pData++ << 24 ;
		dat += (UINT_32)*pData++ << 16 ;
		dat += (UINT_32)*pData++ << 8 ;
		dat += (UINT_32)*pData++ ;
#endif
		sum += (UINT_64)dat ;
	}

	*pSumH = (UINT_32)(sum >> 32) ;
	*pSumL = (UINT_32)(sum & 0xFFFFFFFF) ;
}

//********************************************************************************
// Function Name 	: F40_CalcBlockChksum
//********************************************************************************
void F40_CalcBlockChksum( UINT_8 num, UINT_32 *pSumH, UINT_32 *pSumL )
{
	UINT_8	SectorData[SECTOR_SIZE] ;
	UINT_32	top ;
	UINT_16	sec ;
	UINT_64	sum = 0 ;
	UINT_32	datH, datL ;

	top = num * BLOCK_UNIT ;

	for( sec = 0; sec < (BLOCK_BYTE / SECTOR_SIZE); sec++ ) {
		F40_FlashSectorRead( top + sec * 64, SectorData ) ;

		F40_CalcChecksum( SectorData, SECTOR_SIZE, &datH, &datL ) ;
		sum += ((UINT_64)datH << 32) + datL ;
	}

	*pSumH = (UINT_32)(sum >> 32);
	*pSumL = (UINT_32)(sum & 0xFFFFFFFF);
}


//********************************************************************************
// Function Name 	: F40_FlashDownload
//********************************************************************************
UINT_8 F40_FlashDownload( UINT_8 chiperase, UINT_8 ModuleVendor, UINT_8 ActVer )
{
	DOWNLOAD_TBL* ptr ;

	ptr = ( DOWNLOAD_TBL * )DTbl ;
	do {
		if( ptr->Index == ( ((UINT_16)ModuleVendor<<8) + ActVer) ) {
			return F40_FlashUpdate( chiperase, ptr );
		}
		ptr++ ;
	} while (ptr->Index != 0xFFFF ) ;

	return 0xF0 ;
}

//********************************************************************************
// Function Name 	: F40_FlashUpdate
//********************************************************************************
UINT_8 F40_FlashUpdate( UINT_8 flag, DOWNLOAD_TBL* ptr )
{
	UINT_32	SiWrkVl0 ,SiWrkVl1 ;
	UINT_32	SiAdrVal ;
	const UINT_8 *NcDatVal ;
	UINT_32	UlReadVal, UlCnt, ul_version ;
	UINT_8	ans, i ;
	UINT_16	UsChkBlocks ;
//	UINT_8	UserMagicCode[ ptr->SizeMagicCode ] ;
	UINT_8 *UserMagicCode = NULL;

	UserMagicCode = (UINT_8 *)kzalloc( ptr->SizeMagicCode, GFP_KERNEL);

	if( UserMagicCode == NULL ) {
		return (255) ;					// No enough memory
	}

    CAM_INFO(CAM_OIS, "F40_FlashUpdate E+++++++++");
    
	RamRead32A( SiVerNum, &ul_version ) ;						// Read DSP Code Version
	
//--------------------------------------------------------------------------------
// 0.
//--------------------------------------------------------------------------------
	F40_IOWrite32A( SYSDSP_REMAP,				0x00001440 ) ;
	WitTim( 25 ) ;
	F40_IORead32A( SYSDSP_SOFTRES,				(UINT_32 *)&SiWrkVl0 ) ;
	SiWrkVl0	&= 0xFFFFEFFF ;
	F40_IOWrite32A( SYSDSP_SOFTRES,				SiWrkVl0 ) ;
	RamWrite32A( 0xF006,					0x00000000 ) ;
	F40_IOWrite32A( SYSDSP_DSPDIV,				0x00000001 ) ;
	RamWrite32A( 0x0344,					0x00000014 ) ;
	SiAdrVal =0x00100000;

	for( UlCnt = 0 ;UlCnt < 25 ; UlCnt++ ) {
		RamWrite32A( 0x0340,				SiAdrVal ) ;
		SiAdrVal += 0x00000008 ;
		RamWrite32A( 0x0348,				UlPmemCodeF40[ UlCnt*5   ] ) ;
		RamWrite32A( 0x034C,				UlPmemCodeF40[ UlCnt*5+1 ] ) ;
		RamWrite32A( 0x0350,				UlPmemCodeF40[ UlCnt*5+2 ] ) ;
		RamWrite32A( 0x0354,				UlPmemCodeF40[ UlCnt*5+3 ] ) ;
		RamWrite32A( 0x0358,				UlPmemCodeF40[ UlCnt*5+4 ] ) ;
		RamWrite32A( 0x033c,				0x00000001 ) ;
	}
	for(UlCnt = 0 ;UlCnt < 9 ; UlCnt++ ){
		CntWrt( (INT_8 *)&UpData_CommandFromTable[ UlCnt*6 ], 0x00000006 ) ;
	}

    CAM_INFO(CAM_OIS, "step 1 +++++++");
//--------------------------------------------------------------------------------
// 1.
//--------------------------------------------------------------------------------
	if( flag ) {
		ans = F40_UnlockCodeSet() ;
		if ( ans != 0 ){
			kfree( UserMagicCode ) ;
			return( ans ) ;
		}

		F40_IOWrite32A( FLASHROM_F40_ADR,		0x00000000 ) ;
		F40_IOWrite32A( FLASHROM_F40_CMD,		0x00000005 ) ;
		WitTim( 13 ) ;
		UlCnt=0;
		do {
			if( UlCnt++ > 100 ) {
				ans=0x10 ;
				break ;
			}
			F40_IORead32A( FLASHROM_F40_INT,	&UlReadVal ) ;
		}while ( (UlReadVal & 0x00000080) != 0 ) ;

	} else {
		for( i = 0 ; i < ERASE_BLOCKS ; i++ ) {
			ans	= F40_FlashBlockErase( i * BLOCK_UNIT ) ;
			if( ans != 0 ) {
				kfree( UserMagicCode ) ;
				return( ans ) ;
			}
		}
		ans = F40_UnlockCodeSet() ;
		if ( ans != 0 ){
			kfree( UserMagicCode ) ;
			return( ans );
		}
	}
    CAM_INFO(CAM_OIS, "step 2 +++++++");
//--------------------------------------------------------------------------------
// 2.
//--------------------------------------------------------------------------------
	F40_IOWrite32A( FLASHROM_F40_ADR,			0x00010000 ) ;
	F40_IOWrite32A( FLASHROM_F40_CMD,			0x00000004 ) ;
	WitTim( 5 ) ;
	UlCnt=0;
	do {
		if( UlCnt++ > 100 ) {
			ans = 0x10 ;
			break ;
		}
		F40_IORead32A( FLASHROM_F40_INT,		&UlReadVal ) ;
	} while ( (UlReadVal & 0x00000080) != 0 ) ;

    CAM_INFO(CAM_OIS, "step 3 +++++++");
//--------------------------------------------------------------------------------
// 3.
//--------------------------------------------------------------------------------
	F40_FlashBurstWrite( ptr->FromCode,		ptr->SizeFromCode, 0 ) ;

	ans |= F40_UnlockCodeClear() ;
	if ( ans != 0 ){
		kfree( UserMagicCode ) ;
		return( ans ) ;
	}

    CAM_INFO(CAM_OIS, "step 4 +++++++");

//--------------------------------------------------------------------------------
// 4.
//--------------------------------------------------------------------------------
	UsChkBlocks = ( ptr->SizeFromCode / 160 ) + 1 ;
	RamWrite32A( 0xF00A,					0x00000000 ) ;
	RamWrite32A( 0xF00B,					UsChkBlocks ) ;
	RamWrite32A( 0xF00C,					0x00000100 ) ;

	NcDatVal = ptr->FromCode;
	SiWrkVl0 = 0;
	for( UlCnt = 0; UlCnt < ptr->SizeFromCode; UlCnt++ ) {
		SiWrkVl0 += *NcDatVal++ ;
	}
	UsChkBlocks *= 160  ;
	for( ; UlCnt < UsChkBlocks ; UlCnt++ ) {
		SiWrkVl0 += 0xFF ;
	}

	UlCnt=0;
	do {
		if( UlCnt++ > 100 ){
			kfree( UserMagicCode ) ;
			return ( 6 ) ;
		}

		RamRead32A( 0xF00C,					&UlReadVal ) ;
	} while( UlReadVal != 0 )  ;

    CAM_INFO(CAM_OIS, "Fromcode: UlReadVal = 0x%x, UsChkBlocks = %d", UlReadVal, UsChkBlocks);

	RamRead32A( 0xF00D,						&SiWrkVl1 ) ;
    CAM_INFO(CAM_OIS, "step test exit: SiWrkVl0=0x%x, SiWrkVl1=0x%x",
        SiWrkVl0, SiWrkVl1);

	if( SiWrkVl0 != SiWrkVl1 ){
		kfree( UserMagicCode ) ;
		return( 0x20 );
	}

//--------------------------------------------------------------------------------
// X.
//--------------------------------------------------------------------------------

	if ( !flag ) {
		UINT_32 sumH, sumL;
		UINT_16 Idx;

		// if you can use memcpy(), modify code.
		for( UlCnt = 0; UlCnt < ptr->SizeMagicCode; UlCnt++ ) {
			UserMagicCode[ UlCnt ] = ptr->MagicCode[ UlCnt ] ;
		}

		for( UlCnt = 0; UlCnt < USER_RESERVE; UlCnt++ ) {
			F40_CalcBlockChksum( ERASE_BLOCKS + UlCnt, &sumH, &sumL ) ;
			Idx =  (ERASE_BLOCKS + UlCnt) * 2 * 5 + 1 + 40 ;
			NcDatVal = (UINT_8 *)&sumH ;

#ifdef _BIG_ENDIAN_
			// for BIG ENDIAN SYSTEM
			UserMagicCode[ Idx++ ] = *NcDatVal++ ;
			UserMagicCode[ Idx++ ] = *NcDatVal++ ;
			UserMagicCode[ Idx++ ] = *NcDatVal++ ;
			UserMagicCode[ Idx++ ] = *NcDatVal++ ;
			Idx++;
			NcDatVal = (UINT_8 *)&sumL;
			UserMagicCode[ Idx++ ] = *NcDatVal++ ;
			UserMagicCode[ Idx++ ] = *NcDatVal++ ;
			UserMagicCode[ Idx++ ] = *NcDatVal++ ;
			UserMagicCode[ Idx++ ] = *NcDatVal++ ;
#else
			// for LITTLE ENDIAN SYSTEM
			UserMagicCode[ Idx+3 ] = *NcDatVal++ ;
			UserMagicCode[ Idx+2 ] = *NcDatVal++ ;
			UserMagicCode[ Idx+1 ] = *NcDatVal++ ;
			UserMagicCode[ Idx+0 ] = *NcDatVal++ ;
			Idx+=5;
			NcDatVal = (UINT_8 *)&sumL;
			UserMagicCode[ Idx+3 ] = *NcDatVal++ ;
			UserMagicCode[ Idx+2 ] = *NcDatVal++ ;
			UserMagicCode[ Idx+1 ] = *NcDatVal++ ;
			UserMagicCode[ Idx+0 ] = *NcDatVal++ ;
#endif
		}
		NcDatVal = UserMagicCode ;

	} else {
		NcDatVal = ptr->MagicCode ;
	}

    CAM_INFO(CAM_OIS, "step 5+++++++");
//--------------------------------------------------------------------------------
// 5.
//--------------------------------------------------------------------------------
	ans = F40_UnlockCodeSet() ;
	if ( ans != 0 ){
		kfree( UserMagicCode ) ;
		return( ans ) ;
	}

	F40_FlashBurstWrite( NcDatVal, ptr->SizeMagicCode, 0x00010000 );
	F40_UnlockCodeClear();

    CAM_INFO(CAM_OIS, "step 6+++++++");
//--------------------------------------------------------------------------------
// 6.
//--------------------------------------------------------------------------------
	RamWrite32A( 0xF00A,					0x00010000 ) ;
	RamWrite32A( 0xF00B,					0x00000002 ) ;
	RamWrite32A( 0xF00C,					0x00000100 ) ;

	SiWrkVl0 = 0;
	for( UlCnt = 0; UlCnt < ptr->SizeMagicCode; UlCnt++ ) {
		SiWrkVl0 += *NcDatVal++ ;
	}
	for( ; UlCnt < 320; UlCnt++ ) {
		SiWrkVl0 += 0xFF ;
	}
	kfree( UserMagicCode ) ;

	UlCnt=0 ;
	do {
		if( UlCnt++ > 100 )
			return( 6 ) ;

		RamRead32A( 0xF00C,					&UlReadVal ) ;
	} while( UlReadVal != 0 ) ;
	RamRead32A( 0xF00D,						&SiWrkVl1 ) ;

    CAM_INFO(CAM_OIS, "MagicCode: UlReadVal = 0x%x, SiWrkVl0=0x%x, SiWrkVl1=0x%x",
        UlReadVal, SiWrkVl0, SiWrkVl1);

	if(SiWrkVl0 != SiWrkVl1 )
		return( 0x30 );

	// The process of when you updated the firmware from Ver.07 to Ver.0D
	if( ( ul_version & 0x000000FF ) == 0x07 ) {
		ans	= WrChangeCalData_07_0C_DC( 0 ) ;
	} else if( ( ul_version & 0x000000FF ) == 0x0C ) {
		ans	= WrChangeCalData_07_0C_DC( 1 ) ;
	} else if( ( ul_version & 0x000000FF ) == 0xBC ) {
		ans	= WrChangeCalData_07_0C_DC( 1 ) ;
	} else {
		ans	= WrChangeCalData_07_0C_DC( 2 ) ;
	}

	if( ans ) {
		return( ans ) ;
	}

	F40_IOWrite32A( SYSDSP_REMAP,				0x00001000 ) ;

	WitTim(1000);

	CAM_INFO(CAM_OIS, "F40_FlashUpdate X+++++++++");

//--------------------------------------------------------------------------------
// FOR vendor test
//--------------------------------------------------------------------------------
    if (0) {
        UINT_32 UlCnt = 0;
        UINT_32 check_sum_flag = 0x12345678;

        RamWrite32A(0xC000, 0x0000000C);
        RamRead32A(0xD000, &check_sum_flag);
    	CAM_INFO(CAM_OIS, "check_sum_flag = 0x%x", check_sum_flag);

        for(UlCnt = 0; UlCnt < 13; UlCnt++) {
        	UINT_8	SectorData[SECTOR_SIZE];
        	UINT_32	top;
        	UINT_16	sec;
        	top = UlCnt * BLOCK_UNIT;

        	for(sec = 0; sec < (BLOCK_BYTE / SECTOR_SIZE); sec++) {
        		F40_FlashSectorRead(top + sec * 64, SectorData) ;
        	}
        }
    }

	return( 0 );
}

//********************************************************************************
// Function Name 	: F40_ReadCalData
//********************************************************************************

void F40_ReadCalData( UINT_32 * BufDat, UINT_32 * ChkSum )
{
	UINT_16	UsSize = 0, UsNum;

	*ChkSum = 0;

	do{
		// Count
		F40_IOWrite32A( FLASHROM_F40_ACSCNT, (FLASH_ACCESS_SIZE-1) ) ;

		// NVR2 Addres Set
		F40_IOWrite32A( FLASHROM_F40_ADR, 0x00010040 + UsSize ) ;		// set NVR2 area
		// Read Start
		F40_IOWrite32A( FLASHROM_F40_CMD, 1 ) ;  						// Read Start

		RamWrite32A( F40_IO_ADR_ACCESS , FLASHROM_F40_RDATL ) ;		// RDATL data

		for( UsNum = 0; UsNum < FLASH_ACCESS_SIZE; UsNum++ )
		{
			RamRead32A(  F40_IO_DAT_ACCESS , &(BufDat[ UsSize ]) ) ;
			*ChkSum += BufDat[ UsSize++ ];
		}
	}while (UsSize < 64);	// 64*5 = 320 : NVR sector size
}

//********************************************************************************
// Function Name 	: F40_GyroReCalib
//********************************************************************************
UINT_8	F40_GyroReCalib( stReCalib * pReCalib )
{
	UINT_8	UcSndDat ;
	UINT_32	UlRcvDat ;
	UINT_32	UlGofX, UlGofY ;
	UINT_32	UiChkSum ;
	UINT_32	UlStCnt = 0;
	
	F40_ReadCalData( UlBufDat, &UiChkSum );

	RamWrite32A( 0xF014 , 0x00000000 ) ;

	do {
		UcSndDat = F40_RdStatus(1);
	} while (UcSndDat != 0 && (UlStCnt++ < CNT100MS ));

	RamRead32A( 0xF014 , &UlRcvDat ) ;
	UcSndDat = (unsigned char)(UlRcvDat >> 24);	
	
	if( UlBufDat[ 49 ] == 0xFFFFFFFF )
		pReCalib->SsFctryOffX = (UlBufDat[ 19 ] >> 16) ;
	else
		pReCalib->SsFctryOffX = (UlBufDat[ 49 ] >> 16) ;

	if( UlBufDat[ 50 ] == 0xFFFFFFFF )
		pReCalib->SsFctryOffY = (UlBufDat[ 20 ] >> 16) ;
	else
		pReCalib->SsFctryOffY = (UlBufDat[ 50 ] >> 16) ;

	RamRead32A(  0x0278 , &UlGofX ) ;
	RamRead32A(  0x027C , &UlGofY ) ;

	pReCalib->SsRecalOffX = (UlGofX >> 16) ;
	pReCalib->SsRecalOffY = (UlGofY >> 16) ;
	pReCalib->SsDiffX = pReCalib->SsFctryOffX - pReCalib->SsRecalOffX ;
	pReCalib->SsDiffY = pReCalib->SsFctryOffY - pReCalib->SsRecalOffY ;

	return( UcSndDat );
}
//********************************************************************************
// Function Name 	: F40_EraseCalData
//********************************************************************************
UINT_8 F40_EraseCalData( void )
{
	UINT_32	UlReadVal, UlCnt;
	UINT_8 ans = 0;

	// Flash writeõ
	ans = F40_UnlockCodeSet();
	if ( ans != 0 ) return (ans);								// Unlock Code Set

	// set NVR2 area
	F40_IOWrite32A( FLASHROM_F40_ADR, 0x00010040 ) ;
	// Sector Erase Start
	F40_IOWrite32A( FLASHROM_F40_CMD, 4	/* SECTOR ERASE */ ) ;

	WitTim( 5 ) ;
	UlCnt=0;
	do{
		if( UlCnt++ > 100 ){	ans = 2;	break;	} ;
		F40_IORead32A( FLASHROM_F40_INT, &UlReadVal );
	}while ( (UlReadVal & 0x00000080) != 0 );

	ans = F40_UnlockCodeClear();									// Unlock Code Clear

	return(ans);
}

//********************************************************************************
// Function Name 	: F40_WrGyroOffsetData
//********************************************************************************
UINT_8	F40_WrGyroOffsetData( void )
{
	UINT_32	UlFctryX, UlFctryY;
	UINT_32	UlCurrX, UlCurrY;
	UINT_32	UlGofX, UlGofY;
	UINT_32	UiChkSum1,	UiChkSum2 ;
	UINT_32	UlSrvStat,	UlOisStat ;
	UINT_8	ans;
	UINT_32	UlStCnt = 0;
	UINT_8	UcSndDat ;


	RamRead32A( 0xF010 , &UlSrvStat ) ;
	RamRead32A( 0xF012 , &UlOisStat ) ;
	RamWrite32A( 0xF010 , 0x00000000 ) ;
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	F40_ReadCalData( UlBufDat, &UiChkSum2 );
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	ans = F40_EraseCalData();
	if ( ans == 0 ){
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
		RamRead32A(  0x0278 , &UlGofX ) ;
		RamRead32A(  0x027C , &UlGofY ) ;

		UlCurrX		= UlBufDat[ 19 ] ;
		UlCurrY		= UlBufDat[ 20 ] ;
		UlFctryX	= UlBufDat[ 49 ] ;
		UlFctryY	= UlBufDat[ 50 ] ;

		if( UlFctryX == 0xFFFFFFFF )
			UlBufDat[ 49 ] = UlCurrX ;

		if( UlFctryY == 0xFFFFFFFF )
			UlBufDat[ 50 ] = UlCurrY ;

		UlBufDat[ 19 ] = UlGofX ;
		UlBufDat[ 20 ] = UlGofY ;

//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
		F40_WriteCalData( UlBufDat, &UiChkSum1 );	
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
		F40_ReadCalData( UlBufDat, &UiChkSum2 );	

		if(UiChkSum1 != UiChkSum2 ){
			ans = 0x10;					
		}
	}
//------------------------------------------------------------------------------------------------
//------------------------------------------------------------------------------------------------
	if( !UlSrvStat ) {
		RamWrite32A( 0xF010 , 0x00000000 ) ;
	} else if( UlSrvStat == 3 ) {
		RamWrite32A( 0xF010 , 0x00000003 ) ;
	} else {
		RamWrite32A( 0xF010 , UlSrvStat ) ;
	}
	do {
		UcSndDat = F40_RdStatus(1);
	} while( UcSndDat == FAILURE && (UlStCnt++ < CNT200MS ));

	if( UlOisStat != 0) {
		RamWrite32A( 0xF012 , 0x00000001 ) ;
		UlStCnt = 0;
		//UcSndDat = 0;
		while( UcSndDat && ( UlStCnt++ < CNT100MS ) ) {
			UcSndDat = F40_RdStatus(1);
		}
	}

	return( ans );															// CheckSum OK

}

//********************************************************************************
// Function Name 	: F40_RdStatus
//********************************************************************************
UINT_8	F40_RdStatus( UINT_8 UcStBitChk )
{
	UINT_32	UlReadVal ;

	RamRead32A( 0xF100 , &UlReadVal );
	if( UcStBitChk ){
		UlReadVal &= READ_STATUS_INI ;
	}
	if( !UlReadVal ){
		return( SUCCESS );
	}else{
		return( FAILURE );
	}
}

//********************************************************************************
// Function Name 	: F40_WriteCalData
//********************************************************************************
UINT_8 F40_WriteCalData( UINT_32 * BufDat, UINT_32 * ChkSum )
{
	UINT_16	UsSize = 0, UsNum;
	UINT_8 ans = 0;
	UINT_32	UlReadVal = 0;

	*ChkSum = 0;

	ans = F40_UnlockCodeSet();
	if ( ans != 0 ) return (ans);

	F40_IOWrite32A( FLASHROM_F40_WDATH, 0x000000FF ) ;

	do{
		F40_IOWrite32A( FLASHROM_F40_ACSCNT, (FLASH_ACCESS_SIZE - 1) ) ;
		F40_IOWrite32A( FLASHROM_F40_ADR, 0x00010040 + UsSize ) ;
		F40_IOWrite32A( FLASHROM_F40_CMD, 2) ;  
		for( UsNum = 0; UsNum < FLASH_ACCESS_SIZE; UsNum++ )
		{
			F40_IOWrite32A( FLASHROM_F40_WDATL,  BufDat[ UsSize ] ) ;
			do {
				F40_IORead32A( FLASHROM_F40_INT, &UlReadVal );
			}while ( (UlReadVal & 0x00000020) != 0 );

			*ChkSum += BufDat[ UsSize++ ];	
		}
	}while (UsSize < 64);		

	ans = F40_UnlockCodeClear();	

	return( ans );
}


#if 0
static float fix2float(UINT_32 fix)
{
    if((fix & 0x80000000) > 0)
    {
        return ((float)fix-(float)0x100000000)/(float)0x7FFFFFFF;
    } else {
        return (float)fix/(float)0x7FFFFFFF;
    }
}

static UINT_32 float2fix(float f)
{
    if(f < 0)
    {
        return (UINT_32)(f * (float)0x7FFFFFFF + 0x100000000);
    } else {
        return (UINT_32)(f * (float)0x7FFFFFFF);
    }
}

/*-------------------------------------------------------------------
	Function Name: Accuracy
--------------------------------------------------------------------*/
UINT_16 Accuracy(float ACCURACY, UINT_16 RADIUS, UINT_16 DEGSTEP, UINT_16 WAIT_MSEC1, UINT_16 WAIT_MSEC2, UINT_16 WAIT_MSEC3)
{
	float xpos, ypos;
	UINT_32 xh_value, yh_value;
	float xMxHl, yMxHl;
    UINT_16 xng = 0, yng = 0;
    UINT_16 deg;
    float xRadius, yRadius;
	UINT_32 xGgain, yGgain;
    UINT_32 xGLenz, yGLenz;
	float		xMxAc, yMxAc;
	float		xLimit, yLimit;

	RamRead32A(0x82B8, &xGgain);
	RamRead32A(0x8318, &yGgain);

	RamRead32A(0x82BC, &xGLenz);
	RamRead32A(0x831C, &yGLenz);

	xRadius = 0.242970F * fabsf(fix2float(xGgain)) * fabsf(fix2float(xGLenz)) * 4;
	yRadius = 0.242970F * fabsf(fix2float(yGgain)) * fabsf(fix2float(yGLenz)) * 4;

	xLimit = ACCURACY / 117 * xRadius;
    yLimit = ACCURACY / 117 * yRadius;

	xRadius = xRadius * RADIUS / 117;
	yRadius = yRadius * RADIUS / 117;

	xMxAc = 0;
	yMxAc = 0;

	xpos = xRadius * cos(0);
	ypos = yRadius * sin(0);
	RamWrite32A(0x0114, float2fix(xpos));
	RamWrite32A(0x0164, float2fix(ypos));
	WitTim(WAIT_MSEC1);

	for( deg = 0; deg <= 360; deg += DEGSTEP ) 
	{
		xpos = xRadius * cos(deg * 3.14159/180);
		ypos = yRadius * sin(deg * 3.14159/180);
    	RamWrite32A(0x0114, float2fix(xpos));
		RamWrite32A(0x0164, float2fix(ypos));

		xMxHl = 0;
		yMxHl = 0;
		WitTim(WAIT_MSEC2);
		
		for(short i=0; i<3; i++)
		{
			WitTim(WAIT_MSEC3);
			RamRead32A( 0x0118, &xh_value );
			RamRead32A( 0x0168, &yh_value );
			if(fabsf(fix2float(xh_value) - xpos) > fabsf(xMxHl))	
				xMxHl = fix2float(xh_value) - xpos;
			if(fabsf(fix2float(yh_value) - ypos) > fabsf(yMxHl))	
				yMxHl = fix2float(yh_value) - ypos;
		}

		if(fabsf(xMxHl) > xMxAc)	xMxAc = fabsf(xMxHl);
		if(fabsf(yMxHl) > yMxAc)	yMxAc = fabsf(yMxHl);
		
		xy_raw_data[deg/DEGSTEP].xpos = xpos;
		xy_raw_data[deg/DEGSTEP].xhall = xMxHl + xpos;
		xy_raw_data[deg/DEGSTEP].ypos = ypos;
		xy_raw_data[deg/DEGSTEP].yhall = yMxHl + ypos;
		
		if(fabsf(xMxHl) > xLimit)	xng++; 
		if(fabsf(yMxHl) > yLimit)	yng++; 

	}
	RamWrite32A(0x0114, 0); 
	RamWrite32A(0x0164, 0); 

	return (xng << 8) | yng;
}
#endif


//********************************************************************************
// Function Name 	: WrChangeCalData_07_0C_DC
// Retun Value		: SUCCESS or FAILURE
// Argment Value	: 0x00 = From V07 to VDC
//					  0x01 = From V0C to VDC
//					  0x02 = From VDC to VDC
// Explanation		: Flash Write Calibration Data Function
// History			: First edition 									2018.08.10
//********************************************************************************
UINT_8	WrChangeCalData_07_0C_DC( UINT_8	uc_index )
{
	UINT_32	UiChkSum1,	UiChkSum2 ;
	UINT_8 ans;

//------------------------------------------------------------------------------------------------
// Backup ALL Calibration data
//------------------------------------------------------------------------------------------------
	F40_ReadCalData( UlBufDat, &UiChkSum2 ) ;
//------------------------------------------------------------------------------------------------
// Sector erase NVR2 Calibration area
//------------------------------------------------------------------------------------------------
	ans	= F40_EraseCalData() ;
	
	if( ans == 0 ) {
//------------------------------------------------------------------------------------------------
// Set Calibration data
//------------------------------------------------------------------------------------------------
		UlBufDat[ DEFAULT_GAIN_X ]	= UlBufDat[ GYRO_GAIN_X ] ;
		UlBufDat[ DEFAULT_GAIN_Y ]	= UlBufDat[ GYRO_GAIN_Y ] ;

		CAM_INFO(CAM_OIS, "before: GYRO_GAIN_X=0x%x, GYRO_GAIN_Y=0x%x, uc_index=%d",
			UlBufDat[ GYRO_GAIN_X ], UlBufDat[ GYRO_GAIN_Y ], uc_index);
		if( !uc_index ) {													// From V07 to VDC
			UlBufDat[ GYRO_GAIN_X ]		= ( UINT_32 )( (unsigned long)UlBufDat[ GYRO_GAIN_X ] / 100 * 87 ) ;
			UlBufDat[ GYRO_GAIN_X ]		= ( UINT_32 )( (unsigned long)UlBufDat[ GYRO_GAIN_X ] / 100 * 90 ) ;
			UlBufDat[ GYRO_GAIN_Y ]		= ( UINT_32 )( ( long )UlBufDat[ GYRO_GAIN_Y ] / 100 * 97 ) ;
			UlBufDat[ MIXING_HX45X ]	= 0x7FFFFFFF ;						// Cross Talk Coefficient X -> X
			UlBufDat[ MIXING_HX45Y ]	= 0L;								// Cross Talk Coefficient X -> Y
			UlBufDat[ MIXING_HY45Y ]	= 0x7FFFFFFF;						// Cross Talk Coefficient Y -> Y
			UlBufDat[ MIXING_HY45X ]	= 0L;								// Cross Talk Coefficient Y -> X
			UlBufDat[ MIXING_HXSX ]		= 0L;								// Cross Talk Shift X
			UlBufDat[ MIXING_HYSX ]		= 0L;								// Cross Talk Shift Y
		} else if( uc_index == 0x01 ) {										// From V0C(VBC) to VDC
			UlBufDat[ GYRO_GAIN_X ]		= ( UINT_32 )( (unsigned long)UlBufDat[ GYRO_GAIN_X ]  / 100 * 90 ) ;
			UlBufDat[ GYRO_GAIN_Y ]		= ( UINT_32 )( ( long )UlBufDat[ GYRO_GAIN_Y ]  / 100 * 97 ) ;
		}
		CAM_INFO(CAM_OIS, "after: GYRO_GAIN_X=0x%x, GYRO_GAIN_Y=0x%x", UlBufDat[ GYRO_GAIN_X ], UlBufDat[ GYRO_GAIN_Y ]);

//------------------------------------------------------------------------------------------------
// Write Hall calibration data
//------------------------------------------------------------------------------------------------
		F40_WriteCalData( UlBufDat, &UiChkSum1 );							// NVR2Ö²®lðwriteµCheckSumðvZ
//------------------------------------------------------------------------------------------------
// Calculate calibration data checksum
//------------------------------------------------------------------------------------------------
		F40_ReadCalData( UlBufDat, &UiChkSum2 );							// NVR2Öwrite³êÄ¢é²®lÌCheckSumðvZ

		if(UiChkSum1 != UiChkSum2 ){
			TRACE("CheckSum error\n");
			TRACE("UiChkSum1 = %08X, UiChkSum2 = %08X\n",(UINT_32)UiChkSum1, (UINT_32)UiChkSum2 );
			ans = 0x10;														// CheckSumG[
		}
		TRACE("CheckSum OK\n");
		TRACE("UiChkSum1 = %08X, UiChkSum2 = %08X\n",(UINT_32)UiChkSum1, (UINT_32)UiChkSum2 );
	}

	return( ans );															// CheckSum OK
}



//********************************************************************************
// Function Name 	: WrGyroGain
// Retun Value		: SUCCESS or FAILURE
// Argment Value	: X Gyro Gain, Y Gyro Gain
// Explanation		: Flash Write Calibration Data Function
// History			: First edition 									2018.08.28
//********************************************************************************
UINT_8	WrGyroGain( UINT_32 ul_gain_x, UINT_32 ul_gain_y )
{
	UINT_32	UiChkSum1,	UiChkSum2 ;
	UINT_8 ans;

	CAM_INFO(CAM_OIS, "WrGyroGain E+++++++++");
//------------------------------------------------------------------------------------------------
// Backup ALL Calibration data
//------------------------------------------------------------------------------------------------
	F40_ReadCalData( UlBufDat, &UiChkSum2 ) ;
//------------------------------------------------------------------------------------------------
// Sector erase NVR2 Calibration area
//------------------------------------------------------------------------------------------------
	ans	= F40_EraseCalData() ;

	if( ans == 0 ) {
//------------------------------------------------------------------------------------------------
// Set Calibration data
//------------------------------------------------------------------------------------------------
		UlBufDat[ GYRO_GAIN_X ]		= ul_gain_x ;
		UlBufDat[ GYRO_GAIN_Y ]		= ul_gain_y ;

//------------------------------------------------------------------------------------------------
// Write Hall calibration data
//------------------------------------------------------------------------------------------------
		F40_WriteCalData( UlBufDat, &UiChkSum1 );							// NVR2へ調整値をwriteしCheckSumを計算
//------------------------------------------------------------------------------------------------
// Calculate calibration data checksum
//------------------------------------------------------------------------------------------------
		F40_ReadCalData( UlBufDat, &UiChkSum2 );							// NVR2へwriteされている調整値のCheckSumを計算

		if(UiChkSum1 != UiChkSum2 ){
			TRACE("CheckSum error\n");
			TRACE("UiChkSum1 = %08X, UiChkSum2 = %08X\n",(UINT_32)UiChkSum1, (UINT_32)UiChkSum2 );
			ans = 0x10;														// CheckSumエラー
		}
		TRACE("CheckSum OK\n");
		TRACE("UiChkSum1 = %08X, UiChkSum2 = %08X\n",(UINT_32)UiChkSum1, (UINT_32)UiChkSum2 );
	}

	CAM_INFO(CAM_OIS, "WrGyroGain X--------, ans = %d", ans);
	return( ans );															// CheckSum OK
}



