#include "platform_config.h"

#include "rtc/tz.h"
#include <string.h>

static rtcTime_t _tzDSTStartUTC;
static rtcTime_t _tzSTDStartUTC;
static rtcTime_t _tzDSTStartLocal;
static rtcTime_t _tzSTDStartLocal;

static tzRule_t _tzDST;
static tzRule_t _tzSTD;


/**************************************************************************/
/*!
 @brief  Initialises the TZ Lib
 */
/**************************************************************************/
void tzInit( void )
{
    tzLoadSTD(&_tzSTD);
    tzLoadDST(&_tzDST);
}

void   tzSetSTD( tzRule_t *std )
{
    _tzSTD = *std;
}

void   tzGetSTD( tzRule_t *std )
{
    *std = _tzSTD;
}

void tzStoreSTD( tzRule_t *std )
{
    /* Convert */
    uint16_t offset = std->offset;
    uint16_t hourdow = (std->hour << 8) + std->dow;
    uint16_t weekmonth = (std->week << 8) + std->month;

    /* Allow access to FLASH Domain */
    FLASH_Unlock();

    /* Write to the FLASH Domain */
    EE_WriteVariable(CFG_EEPROM_TZ_STD+0, offset);
    EE_WriteVariable(CFG_EEPROM_TZ_STD+1, hourdow);
    EE_WriteVariable(CFG_EEPROM_TZ_STD+2, weekmonth);

    /* Deny access to FLASH Domain */
    FLASH_Lock();
}

void tzLoadSTD( tzRule_t *std )
{
    uint16_t offset;
    uint16_t hourdow;
    uint16_t weekmonth;

    EE_ReadVariable(CFG_EEPROM_TZ_STD+0, &offset);
    EE_ReadVariable(CFG_EEPROM_TZ_STD+1, &hourdow);
    EE_ReadVariable(CFG_EEPROM_TZ_STD+2, &weekmonth);

    std->offset = offset;
    std->hour = hourdow >> 8;
    std->dow = hourdow & 0xFF;
    std->week = weekmonth >> 8;
    std->month = weekmonth & 0xFF;
}

void   tzSetDST( tzRule_t *dst )
{
    _tzDST = *dst;
}

void   tzGetDST( tzRule_t *dst )
{
    *dst = _tzDST;
}

void tzStoreDST( tzRule_t *dst )
{
	/* Convert */
	uint16_t offset = dst->offset;
	uint16_t hourdow = (dst->hour << 8) + dst->dow;
	uint16_t weekmonth = (dst->week << 8) + dst->month;

	/* Allow access to FLASH Domain */
	FLASH_Unlock();

	/* Write to the FLASH Domain */
	EE_WriteVariable(CFG_EEPROM_TZ_DST+0, offset);
	EE_WriteVariable(CFG_EEPROM_TZ_DST+1, hourdow);
	EE_WriteVariable(CFG_EEPROM_TZ_DST+2, weekmonth);

	/* Deny access to FLASH Domain */
	FLASH_Lock();
}

void tzLoadDST( tzRule_t *dst )
{
	uint16_t offset;
	uint16_t hourdow;
	uint16_t weekmonth;

	EE_ReadVariable(CFG_EEPROM_TZ_DST+0, &offset);
	EE_ReadVariable(CFG_EEPROM_TZ_DST+1, &hourdow);
	EE_ReadVariable(CFG_EEPROM_TZ_DST+2, &weekmonth);

	dst->offset = offset;
	dst->hour = hourdow >> 8;
	dst->dow = hourdow & 0xFF;
	dst->week = weekmonth >> 8;
	dst->month = weekmonth & 0xFF;
}

/**************************************************************************/
/*!
 @brief  Creates Time Change Rule
 @param  offset[in]:    offset to assign to tzRule_t
         hour[in]:      hour to assign to tzRule_t
         dow[in]:       day of week to assign to tzRule_t
         week[in]:      week to assign to tzRule_t
         month[in]:     month to assign to tzRule_t
         r[out]:        tzRule_t reference to manipulate
 @return errorCode
 */
/**************************************************************************/
void   tzCreateRule( uint16_t offset, uint8_t hour, rtcWeekdays_t dow,
                     uint8_t week, rtcMonths_t month, tzRule_t *r )
{
    //error_t errorCode;
    tzRule_t newRule;

    /* Basic validation */
    if ((hour > 23) || (dow > 6) || (week > 4) || (month > 12) || (month < 1))
    {
        return;
    }

    newRule.offset = offset;
    newRule.hour = hour;
    newRule.dow = dow;
    newRule.week = week;
    newRule.month = month;

    memcpy(r, &newRule, sizeof(tzRule_t));
}

/**************************************************************************/
/*!
 @brief  Convert a Time Change Rule to a RTC Time
 @param  r[in]:         tzRule_t time change rule
         year[in]:      year considered
         t[out]:        rtcTime_t reference to manipulate
 @return errorCode
 */
/**************************************************************************/
void   tzRuleToTime( tzRule_t *r, uint16_t year, rtcTime_t *t )
{
    // TEST THIS THING TO DEATH!!!!

    uint8_t tYear = year;
    uint8_t tMonth = r->month;
    uint8_t tWeek = r->week;

    if(tWeek == TZ_WEEK_LAST)
    {
        if(tMonth == RTC_MONTHS_DECEMBER)
        {
            tMonth = RTC_MONTHS_JANUARY;
            year++;
        }
        tMonth = tMonth + 1;
        tWeek = TZ_WEEK_FIRST;
    }

    rtcWeekdays_t weekday;
    rtcGetWeekday(tYear, tMonth, 1, &weekday);
    uint8_t tDay = 1 + 7 * (tWeek - 1) + (r->dow - weekday + 7) % 7;

    /*
    Example:
        Last sunday in march 2015
        tDay = 1 + 7 * (week - 1) + (rule->dow - weekday + 7) % 7;
        tDay = 1 + 7 * 0 + (6 - 2 + 7) % 7
        -> 5
    */

    uint8_t tHour = r->hour;
    uint8_t tMinute = 0;
    uint8_t tSecond = 0;
    uint8_t tTimezone = 0; //what is that anyway?

    rtcTime_t newTime;
    rtcCreateTime( tYear, tMonth, tDay, tHour, tMinute, tSecond, tTimezone, &newTime );

    if(r->week == TZ_WEEK_LAST)
    {
        rtcAddDays( &newTime, -7 );
    }

    memcpy(t, &newTime, sizeof(rtcTime_t));
}

/**************************************************************************/
/*!
 @brief  Calculates the start times of DST / STD times for a given year
 @param  year[in]:       year considered
 @return errorCode
 */
/**************************************************************************/
void   tzCalcStartTimes( uint16_t year )
{
    tzRuleToTime( &_tzDST, year, &_tzDSTStartLocal );
    _tzDSTStartUTC = _tzDSTStartLocal;
    rtcAddMinutes( &_tzDSTStartUTC, -(_tzSTD.offset) );

    tzRuleToTime( &_tzSTD, year, &_tzSTDStartLocal );
    _tzSTDStartUTC = _tzSTDStartLocal;
    rtcAddMinutes( &_tzSTDStartUTC, -(_tzDST.offset) );
}

/**************************************************************************/
/*!
 @brief  Checks if utc is in DST
 @param  utc[in]:       rtcTime_t utc time as input
 @return TZ_DST or TZ_NON_DST
 */
/**************************************************************************/
bool   tzUTCIsDST( rtcTime_t *utc )
{
    if( utc->years != _tzDSTStartUTC.years )
    {
        tzCalcStartTimes(utc->years);
    }

    int32_t diff;
    rtcGetDifference( &_tzSTDStartUTC, &_tzDSTStartUTC, &diff );

    int32_t diff1, diff2;
    rtcGetDifference( utc, &_tzDSTStartUTC, &diff1 );
    rtcGetDifference( utc, &_tzSTDStartUTC, &diff2 );

    if( diff > 0 )
    {
        return ( diff1 >= 0 && diff2 < 0 );
    }
    else
    {
        return !( diff1 >= 0 && diff2 < 0 );
    }
}

/**************************************************************************/
/*!
 @brief  Checks if local is in DST
 @param  local[in]:       rtcTime_t local time as input
 @return TZ_DST or TZ_NON_DST
 */
/**************************************************************************/
bool   tzLocalIsDST( rtcTime_t *local )
{
    if( local->years != _tzDSTStartLocal.years )
    {
        tzCalcStartTimes(local->years);
    }

    int32_t diff;
    rtcGetDifference( &_tzSTDStartLocal, &_tzDSTStartLocal, &diff );

    int32_t diff1, diff2;
    rtcGetDifference( local, &_tzDSTStartLocal, &diff1 );
    rtcGetDifference( local, &_tzSTDStartLocal, &diff2 );


    if( diff > 0 )
    {
        return ( diff1 >= 0 && diff2 < 0 );
    }
    else
    {
        return !( diff1 >= 0 && diff2 < 0  );
    }
}

/**************************************************************************/
/*!
 @brief  Convert UTC to Local time
 @param  utc[in]:       rtcTime_t utc time as input
         local[out]:    rtcTime_t local time as output
 @return errorCode
 */
/**************************************************************************/
void   tzUTCToLocal( rtcTime_t *utc, rtcTime_t *local )
{
    if( utc->years != _tzDSTStartUTC.years )
    {
        tzCalcStartTimes(utc->years);
    }

    if( tzUTCIsDST( utc ) )
    {
        *local = *utc;
        rtcAddMinutes( local, _tzDST.offset );
    }
    else
    {
        *local = *utc;
        rtcAddMinutes( local, _tzSTD.offset );
    }
}

/**************************************************************************/
/*!
 @brief  Convert Local to UTC time
 @param  local[in]:    rtcTime_t local time as input
         utc[out]:     rtcTime_t utc time as output
 @return errorCode
 */
/**************************************************************************/
void   tzLocalToUTC( rtcTime_t *local, rtcTime_t *utc )
{
    if( local->years != _tzDSTStartLocal.years )
    {
        tzCalcStartTimes(utc->years);
    }

    if( tzLocalIsDST( local ) )
    {
        *utc = *local;
        rtcAddMinutes( utc, -_tzDST.offset );
    }
    else
    {
        *utc = *local;
        rtcAddMinutes( utc, -_tzSTD.offset );
    }
}
