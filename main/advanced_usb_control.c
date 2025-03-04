#if !defined(EMU)

#include "advanced_usb_control.h"
#include "tinyusb.h"
#include "tusb_hid_gamepad.h"
#include <stdio.h>
#include "esp_partition.h"
#include "esp_attr.h"
#include "mode_main_menu.h"
#include "esp_system.h"
#include "esp_spi_flash.h"
#include "esp_log.h"
#include "rom/cache.h"
#include "soc/sensitive_reg.h"
#include "soc/dport_access.h"
#include "soc/rtc_wdt.h"
#include "soc/soc.h"  // for WRITE_PERI_REG

#include "swadgeMode.h"

#include "esp_flash.h"

// Uncomment the ESP_LOGI to activate logging for this file.
// Logging can cause issues in operation, so by default it should remain off.
#define ULOG( x... ) 
    //ESP_LOGI( "advanced_usb_control", x )

uint32_t * advanced_usb_scratch_buffer_data;
uint32_t   advanced_usb_scratch_buffer_data_size;
uint32_t   advanced_usb_scratch_immediate[SCRATCH_IMMEDIATE_DWORDS];
uint8_t  advanced_usb_printf_buffer[2048];
int      advanced_usb_printf_head, advanced_usb_printf_tail;

uint32_t * advanced_usb_read_offset;
static uint8_t terminal_redirected;
static uint8_t did_init_flash_function;

void dummy_fn( void ) { }

swadgeMode dummy_swadge_mode =
{
    .modeName = "dummy",
    .fnEnterMode = NULL,
    .fnExitMode = dummy_fn,
    .fnMainLoop = NULL,
    .fnButtonCallback = NULL,
    .fnTouchCallback = NULL,
    .wifiMode = NO_WIFI,
    .fnEspNowRecvCb = NULL,
    .fnEspNowSendCb = NULL,
    .fnAccelerometerCallback = NULL,
    .fnAudioCallback = NULL,
    .fnTemperatureCallback = NULL
};

/**
 * @brief Accept a "get" feature report command from a USB host and write
 *         back whatever is needed to send back.
 * 
 * @param datalen Number of bytes host is requesting from us.
 * @param data Pointer to a feature get request for the command set.
 * @return Number of bytes that will be returned.
 */
int handle_advanced_usb_control_get( int reqlen, uint8_t * data )
{
    if( advanced_usb_read_offset == 0 ) return 0;
    memcpy( data, advanced_usb_read_offset, reqlen );
    return reqlen;
}

/**
 * @brief Internal function for writing log data to the ring buffer.
 * 
 * @param cookie *unused*
 * @param data Pointer to text that needs to be logged.
 * @return size Number of bytes in data that need to be logged.
 */
static int advanced_usb_write_log( void* cookie, const char* data, int size )
{
    int next = ( advanced_usb_printf_head + 1 ) % sizeof( advanced_usb_printf_buffer );
    int idx = 0;
    cookie = cookie; // unused
    // Drop extra characters on the floor.
    while( next != advanced_usb_printf_tail && idx < size )
    {
        advanced_usb_printf_buffer[next] = data[idx++];
        advanced_usb_printf_head = next;
        next = ( advanced_usb_printf_head + 1 ) % sizeof( advanced_usb_printf_buffer );
    }
    return size;
}

/**
 * @brief vaprintf standin for USB logging.
 * 
 * @param fmt vaprintf format
 * @param args vaprintf args
 * @return size Number of characters that were written.
 */
int advanced_usb_write_log_printf(const char *fmt, va_list args)
{
    char buffer[512];
    int l = vsnprintf( buffer, 511, fmt, args );
    advanced_usb_write_log( 0, buffer, l );
    return l;
}


/**
 * @brief USB request to get text in buffer
 * 
 * @param reqlen The number of bytes the host is requesting from us.
 * @param data The data that we will write back into
 * @return size Number of bytes to be returned to the host.
 */
int handle_advanced_usb_terminal_get( int reqlen, uint8_t * data )
{
    if( !terminal_redirected )
    {
        ULOG("redirecting stdout");
        esp_log_set_vprintf( advanced_usb_write_log_printf );
        ULOG("stdout redirected");
        terminal_redirected = 1;
    }
    int togo = ( advanced_usb_printf_head - advanced_usb_printf_tail +
        sizeof( advanced_usb_printf_buffer ) ) % sizeof( advanced_usb_printf_buffer );

    data[0] = 171;

    int mark = 1;
    if( togo )
    {
        if( togo > reqlen-1 ) togo = reqlen-1;
        while( mark++ != togo )
        {
            data[mark] = advanced_usb_printf_buffer[advanced_usb_printf_tail++];
            if( advanced_usb_printf_tail == sizeof( advanced_usb_printf_buffer ) )
                advanced_usb_printf_tail = 0;
        }
    }
    return mark;
}

/**
 * @brief Accept a "send" feature report command from a USB host and interpret it.
 *         executing whatever needs to be executed.
 * 
 * @param datalen Total length of the buffer (command ID incldued)
 * @param data Pointer to full command
 * @return A pointer to a null terminated JSON string. May be NULL if the load
 */
void IRAM_ATTR handle_advanced_usb_control_set( int datalen, const uint8_t * data )
{
    if( datalen < 6 ) return;
    intptr_t value = data[2] | ( data[3] << 8 ) | ( data[4] << 16 ) | ( data[5]<<24 );
    switch( data[1] )
    {
    case AUSB_CMD_REBOOT:
        {
        // This is mentioned a few places, but seems unnecessary.
        //void chip_usb_set_persist_flags( uint32_t x );
        //chip_usb_set_persist_flags( USBDC_PERSIST_ENA );

        // Decide to reboot into bootloader or not.
        REG_WRITE(RTC_CNTL_OPTION1_REG, value?RTC_CNTL_FORCE_DOWNLOAD_BOOT:0 );

        void software_reset(uint32_t x);
        software_reset( 0 ); // ROM function

        break;
        }
    case AUSB_CMD_WRITE_RAM:
        {
            // Write into scratch.
            if( datalen < 8 ) return;
            intptr_t length = data[6] | ( data[7] << 8 );
            ULOG( "Writing %d into %p", length, (void*)value );
            memcpy( (void*)value, data+8, length );
            break;
        }
    case AUSB_CMD_READ_RAM:
        // Configure read.
        advanced_usb_read_offset = (uint32_t*)value;
        break;
    case AUSB_CMD_EXEC_RAM:
        // Execute scratch
        {
            void (*scratchfn)() = (void (*)())(value);
            ULOG( "Executing %p (%p) // base %08x/%p", (void*)value, scratchfn, 0, advanced_usb_scratch_buffer_data );
            scratchfn();
        }
        break;
    case AUSB_CMD_SWITCH_MODE:
        // Switch Swadge Mode
        {
            ULOG( "SwadgeMode Value: 0x%08x", value );
			overrideToSwadgeMode( (swadgeMode*)(value?(swadgeMode*)value:&dummy_swadge_mode) );
        }
        break;
    case AUSB_CMD_ALLOC_SCRATCH:
        // (re) allocate the primary scratch buffer.
        {
            // value = -1 will just cause a report.
            // value = 0 will clear it.
            // value < 0 just read current data.
            ULOG( "Allocating to %d (Current: %p / %d)", value, advanced_usb_scratch_buffer_data, advanced_usb_scratch_buffer_data_size );
            if( ! (value & 0x80000000 ) )
            {
                if( value > advanced_usb_scratch_buffer_data_size  )
                {
                    advanced_usb_scratch_buffer_data = realloc( advanced_usb_scratch_buffer_data, value );
                    memset( advanced_usb_scratch_buffer_data, 0, value );
                    advanced_usb_scratch_buffer_data_size = value;
                }
                if( value == 0 )
                {
                    if( advanced_usb_scratch_buffer_data ) free( advanced_usb_scratch_buffer_data );
                    advanced_usb_scratch_buffer_data_size = 0;
                }
            }
            advanced_usb_scratch_immediate[0] = (intptr_t)advanced_usb_scratch_buffer_data;
            advanced_usb_scratch_immediate[1] = advanced_usb_scratch_buffer_data_size;
            advanced_usb_read_offset = (uint32_t*)(&advanced_usb_scratch_immediate[0]);
            ULOG( "New: %p / %d", advanced_usb_scratch_buffer_data, advanced_usb_scratch_buffer_data_size );
        }
        break;
    case ACMD_CMD_MEMSET:
    {
        if( datalen < 11 ) return;
        intptr_t length = data[6] | ( data[7] << 8 ) | ( data[8] << 16 ) | ( data[9] << 24 );
        ULOG( "Memset %d into %p", length, (void*)value );
        memset( (void*)value, data[10], length );
        break;
    }
    case ACMD_CMD_GETVER:
    {
        // TODO: This is terrible.  It should be improved.
        void app_main(void);
        advanced_usb_scratch_immediate[0] = (uint32_t)&app_main;
        advanced_usb_scratch_immediate[1] = (uint32_t)&advanced_usb_scratch_buffer_data;
        advanced_usb_scratch_immediate[2] = (uint32_t)&handle_advanced_usb_control_set;
        advanced_usb_scratch_immediate[3] = (uint32_t)&handle_advanced_usb_terminal_get;
        advanced_usb_read_offset = advanced_usb_scratch_immediate;
        break;
    }
    case AUSB_CMD_FLASH_ERASE: // Flash erase region
    {
        if( datalen < 10 ) return ;
        intptr_t length = data[6] | ( data[7] << 8 ) | ( data[8] << 16 ) | ( data[9]<<24 );
        if( !did_init_flash_function )
            esp_flash_init( 0 );
        if( ( length & 0x80000000 ) && value == 0 )
            esp_flash_erase_chip( 0 );
        else
            esp_flash_erase_region( 0, value, length );    
        break;
    }
    case AUSB_CMD_FLASH_WRITE: // Flash write region
    {
        if( datalen < 8 ) return;
        intptr_t length = data[6] | ( data[7] << 8 );
        esp_flash_write( 0, data+8, value, length );
        break;
    }
    case AUSB_CMD_FLASH_READ: // Flash read region
    {
        if( datalen < 8 ) return ;
        intptr_t length = data[6] | ( data[7] << 8 );
        if( length > sizeof( advanced_usb_scratch_immediate ) )
            length = sizeof( advanced_usb_scratch_immediate );
        esp_flash_read( 0, advanced_usb_scratch_immediate, value, length );
        advanced_usb_read_offset = advanced_usb_scratch_immediate;
        break;
    }
    }
}

#endif

